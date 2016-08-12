
/*=============================================================================
	NVVolumetricLightingRendering.cpp: Nvidia Volumetric Lighting rendering implementation.
=============================================================================*/

#include "RendererPrivate.h"

#if WITH_NVVOLUMETRICLIGHTING

#include "NVVolumetricLightingRHI.h"

static TAutoConsoleVariable<int32> CVarNvVlDebugMode(
	TEXT("r.NvVl.DebugMode"),
	0,
	TEXT("Debug Mode\n")
	TEXT("  0: off\n")
	TEXT("  1: wireframe\n")
	TEXT("  2: no blend\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarNvVlEnable(
	TEXT("r.NvVl.Enable"),
	1,
	TEXT("Enable Nvidia Volumetric Lighting\n")
	TEXT("  0: off\n")
	TEXT("  1: on\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarNvVlScatterScale(
	TEXT("r.NvVl.ScatterScale"),
	10.0f,
	TEXT("Scattering Scale\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarNvVlFog(
	TEXT("r.NvVl.Fog"),
	1,
	TEXT("Enable Scattering Fogging\n")
	TEXT("  0: off\n")
	TEXT("  1: on\n"),
	ECVF_RenderThreadSafe);

const float MULTI_SCATTER = 0.0001f;

DECLARE_CYCLE_STAT(TEXT("Volumetric Lighting Begin Accumulation"), STAT_VolumetricLightingBeginAccumulation, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("Volumetric Lighting Remap Shadow Depth"), STAT_VolumetricLightingRemapShadowDepth, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("Volumetric Lighting Render Volume"), STAT_VolumetricLightingRenderVolume, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("Volumetric Lighting End Accumulation"), STAT_VolumetricLightingEndAccumulation, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("Volumetric Lighting Apply Lighting"), STAT_VolumetricLightingApplyLighting, STATGROUP_SceneRendering);

static FORCEINLINE float RemapTransmittance(const float Range, const float InValue)
{
	return InValue * (1.0f - (1.0f - Range)) + (1.0f - Range);
}

static FORCEINLINE FVector RemapTransmittance(const float Range, const FVector& InValue)
{
	return FVector(RemapTransmittance(Range, InValue.X), RemapTransmittance(Range, InValue.Y), RemapTransmittance(Range, InValue.Z));
}

static FORCEINLINE float GetOpticalDepth(const float InValue)
{
	return -FMath::Loge(InValue) * CVarNvVlScatterScale.GetValueOnRenderThread();
}

static FORCEINLINE FVector GetOpticalDepth(const FVector& InValue)
{
	return FVector(GetOpticalDepth(InValue.X), GetOpticalDepth(InValue.Y), GetOpticalDepth(InValue.Z));
}

void FDeferredShadingSceneRenderer::NVVolumetricLightingBeginAccumulation(FRHICommandListImmediate& RHICmdList)
{
	if (!CVarNvVlEnable.GetValueOnRenderThread())
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_VolumetricLightingBeginAccumulation);

	check(Views.Num());
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	const FViewInfo& View = Views[0];

	NvVl::ViewerDesc ViewerDesc;
	FMatrix ProjMatrix = View.ViewMatrices.ProjMatrix;
    ViewerDesc.mProj = *reinterpret_cast<const NvcMat44*>(&ProjMatrix.M[0][0]);
	FMatrix ViewProjMatrix = View.ViewMatrices.GetViewProjMatrix();
    ViewerDesc.mViewProj = *reinterpret_cast<const NvcMat44*>(&ViewProjMatrix.M[0][0]);
    ViewerDesc.vEyePosition = *reinterpret_cast<const NvcVec3 *>(&View.ViewLocation);
	ViewerDesc.uViewportTopLeftX = View.ViewRect.Min.X;
	ViewerDesc.uViewportTopLeftY = View.ViewRect.Min.Y;
    ViewerDesc.uViewportWidth = View.ViewRect.Width();
    ViewerDesc.uViewportHeight = View.ViewRect.Height();
	ViewerDesc.bReversedZ = ((int32)ERHIZBuffer::IsInverted != 0);
	
	NvVl::MediumDesc MediumDesc;
	const FNVVolumetricLightingProperties& Properties = Scene->VolumetricLightingProperties;
	const FFinalPostProcessSettings& FinalPostProcessSettings = View.FinalPostProcessSettings;

	FVector Absorption = GetOpticalDepth(RemapTransmittance(FinalPostProcessSettings.TransmittanceRange, FVector(FinalPostProcessSettings.AbsorptionColor * FinalPostProcessSettings.AbsorptionTransmittance)));
	MediumDesc.vAbsorption = *reinterpret_cast<const NvcVec3 *>(&Absorption);
	MediumDesc.uNumPhaseTerms = 0;
	// Rayleigh
	if (FinalPostProcessSettings.RayleighTransmittance < 1.0f)
	{
		MediumDesc.PhaseTerms[MediumDesc.uNumPhaseTerms].ePhaseFunc = NvVl::PhaseFunctionType::RAYLEIGH;
		FVector Density = GetOpticalDepth(RemapTransmittance(FinalPostProcessSettings.TransmittanceRange, FinalPostProcessSettings.RayleighTransmittance)) * FVector(5.96f, 13.24f, 33.1f);
		MediumDesc.PhaseTerms[MediumDesc.uNumPhaseTerms].vDensity = *reinterpret_cast<const NvcVec3 *>(&Density);
		MediumDesc.uNumPhaseTerms++;
	}

	// Mie
	if(FinalPostProcessSettings.MiePhase != EMiePhase::MIE_NONE)
	{
		MediumDesc.PhaseTerms[MediumDesc.uNumPhaseTerms].ePhaseFunc = FinalPostProcessSettings.MiePhase == EMiePhase::MIE_HAZY ? NvVl::PhaseFunctionType::MIE_HAZY : NvVl::PhaseFunctionType::MIE_MURKY;
		FVector Density = GetOpticalDepth(RemapTransmittance(FinalPostProcessSettings.TransmittanceRange, FVector(FinalPostProcessSettings.MieColor * FinalPostProcessSettings.MieTransmittance)));
		MediumDesc.PhaseTerms[MediumDesc.uNumPhaseTerms].vDensity = *reinterpret_cast<const NvcVec3 *>(&Density);
		MediumDesc.uNumPhaseTerms++;
	}

	// HG
	const FHGScatteringTerm* HGTerms[NvVl::MAX_PHASE_TERMS] = {
		&FinalPostProcessSettings.HGScattering1Term,
		&FinalPostProcessSettings.HGScattering2Term,
		&FinalPostProcessSettings.HGScattering3Term,
		&FinalPostProcessSettings.HGScattering4Term,
	};

	for(int32 TermIndex = 0; TermIndex < NvVl::MAX_PHASE_TERMS; TermIndex++)
	{
		const float Transmittance = HGTerms[TermIndex]->HGTransmittance;
		const float Eccentricity = HGTerms[TermIndex]->HGEccentricity;
		const FLinearColor& Color = HGTerms[TermIndex]->HGColor;
		if (Transmittance < 1.0f || Color != FLinearColor::White)
		{
			int32 PhaseTermsIndex = MediumDesc.uNumPhaseTerms + TermIndex;
			MediumDesc.PhaseTerms[PhaseTermsIndex].ePhaseFunc = NvVl::PhaseFunctionType::HENYEYGREENSTEIN;
			FVector Density = GetOpticalDepth(RemapTransmittance(FinalPostProcessSettings.TransmittanceRange, FVector(Color * Transmittance)));
			MediumDesc.PhaseTerms[PhaseTermsIndex].vDensity = *reinterpret_cast<const NvcVec3 *>(&Density);
			MediumDesc.PhaseTerms[PhaseTermsIndex].fEccentricity = Eccentricity;
			MediumDesc.uNumPhaseTerms++;

			if (MediumDesc.uNumPhaseTerms >= NvVl::MAX_PHASE_TERMS)
			{
				break;
			}
		}
	}

	Scene->bSkipCurrentFrameVL = (MediumDesc.uNumPhaseTerms == 0);

	if (!Scene->bSkipCurrentFrameVL)
	{
		GNVVolumetricLightingRHI->UpdateDownsampleMode(Properties.DownsampleMode);
		GNVVolumetricLightingRHI->UpdateMsaaMode(Properties.MsaaMode);
		GNVVolumetricLightingRHI->UpdateFilterMode(Properties.FilterMode);

		int32 DebugMode = FMath::Clamp((int32)CVarNvVlDebugMode.GetValueOnRenderThread(), 0, 2);
		GNVVolumetricLightingRHI->BeginAccumulation(SceneContext.GetSceneDepthTexture(), ViewerDesc, MediumDesc, (Nv::VolumetricLighting::DebugFlags)DebugMode); //SceneContext.GetActualDepthTexture()?

		// clear the state cache
		GDynamicRHI->ClearStateCache();
	}
}

void FDeferredShadingSceneRenderer::NVVolumetricLightingRemapShadowDepth(FRHICommandListImmediate& RHICmdList)
{
	if (!CVarNvVlEnable.GetValueOnRenderThread() || Scene->bSkipCurrentFrameVL)
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_VolumetricLightingRemapShadowDepth);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	const FTexture2DRHIRef& ShadowDepth = SceneContext.GetShadowDepthZTexture(false);
	GNVVolumetricLightingRHI->RemapShadowDepth(ShadowDepth);

	// clear the state cache
	GDynamicRHI->ClearStateCache();
}

void FDeferredShadingSceneRenderer::NVVolumetricLightingRenderVolume(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, const FProjectedShadowInfo* ShadowInfo)
{
	if (!CVarNvVlEnable.GetValueOnRenderThread() || Scene->bSkipCurrentFrameVL)
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_VolumetricLightingRenderVolume);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	FVector LightPosition = LightSceneInfo->Proxy->GetOrigin();
	FVector LightDirection = LightSceneInfo->Proxy->GetDirection();
	LightDirection.Normalize();

	FMatrix LightViewProj;
	if (LightSceneInfo->Proxy->GetLightType() == LightType_Point)
	{
		LightViewProj = FTranslationMatrix(-LightPosition);
	}
	else
	{
		LightViewProj = FTranslationMatrix(ShadowInfo->PreShadowTranslation) * ShadowInfo->SubjectAndReceiverMatrix;
	}


	NvVl::ShadowMapDesc ShadowmapDesc;
	uint32 ShadowmapWidth = 0, ShadowmapHeight = 0;

	FTextureRHIParamRef DepthMap = nullptr;

	if (LightSceneInfo->Proxy->GetLightType() == LightType_Point)
	{
		const FTextureCubeRHIRef& ShadowDepth = SceneContext.GetCubeShadowDepthZTexture(ShadowInfo->ResolutionX);

		if (ShadowDepth)
		{
			ShadowmapWidth = ShadowDepth->GetSize();
			ShadowmapHeight = ShadowDepth->GetSize();
		}

		ShadowmapDesc.eType = NvVl::ShadowMapLayout::CUBE;
		ShadowmapDesc.uWidth = ShadowmapWidth;
		ShadowmapDesc.uHeight = ShadowmapHeight;
		// Shadow depth type
		ShadowmapDesc.bLinearizedDepth = false;
		// shadow space
		ShadowmapDesc.bShadowSpace = false;
		ShadowmapDesc.bLowToHighCascadedShadow = true;

		for (int32 FaceIndex = 0; FaceIndex < 6; FaceIndex++)
		{
			ShadowmapDesc.mCubeViewProj[FaceIndex] = *reinterpret_cast<const NvcMat44*>(&(ShadowInfo->OnePassShadowViewProjectionMatrices[FaceIndex]).M[0][0]);
		}

		ShadowmapDesc.uElementCount = 1;
		ShadowmapDesc.Elements[0].uOffsetX = 0;
		ShadowmapDesc.Elements[0].uOffsetY = 0;
		ShadowmapDesc.Elements[0].uWidth = ShadowmapDesc.uWidth;
		ShadowmapDesc.Elements[0].uHeight = ShadowmapDesc.uHeight;
		ShadowmapDesc.Elements[0].mArrayIndex = 0;

		DepthMap = ShadowDepth;
	}
	else
	{
		const FTexture2DRHIRef& ShadowDepth = SceneContext.GetShadowDepthZTexture(false);

		uint32 ShadowmapWidth, ShadowmapHeight;
		if (ShadowDepth)
		{
			ShadowmapWidth = ShadowDepth->GetSizeX();
			ShadowmapHeight = ShadowDepth->GetSizeY();
		}
		else
		{
			FIntPoint Resolution = SceneContext.GetShadowDepthTextureResolution(); // Pre Cache?? GetPreShadowCacheTextureResolution()
			ShadowmapWidth = Resolution.X;
			ShadowmapHeight = Resolution.Y;
		}

		FVector4 ShadowmapMinMaxValue;
		FMatrix WorldToShadowMatrixValue = ShadowInfo->GetWorldToShadowMatrix(ShadowmapMinMaxValue);

		ShadowmapDesc.eType = NvVl::ShadowMapLayout::SIMPLE;
		ShadowmapDesc.uWidth = ShadowmapWidth;
		ShadowmapDesc.uHeight = ShadowmapHeight;
		// Shadow depth type
		ShadowmapDesc.bLinearizedDepth = LightSceneInfo->Proxy->GetLightType() == LightType_Spot ? true : false;
		// shadow space
		ShadowmapDesc.bShadowSpace = true;
		ShadowmapDesc.bLowToHighCascadedShadow = true;

		ShadowmapDesc.uElementCount = 1;
		ShadowmapDesc.Elements[0].uOffsetX = 0;
		ShadowmapDesc.Elements[0].uOffsetY = 0;
		ShadowmapDesc.Elements[0].uWidth = ShadowmapDesc.uWidth;
		ShadowmapDesc.Elements[0].uHeight = ShadowmapDesc.uHeight;
		ShadowmapDesc.Elements[0].mViewProj = *reinterpret_cast<const NvcMat44*>(&WorldToShadowMatrixValue.M[0][0]);
		ShadowmapDesc.Elements[0].mArrayIndex = 0;
		ShadowmapDesc.Elements[0].fInvMaxSubjectDepth = ShadowInfo->InvMaxSubjectDepth;
		ShadowmapDesc.Elements[0].vShadowmapMinMaxValue = *reinterpret_cast<const NvcVec4 *>(&ShadowmapMinMaxValue);

		DepthMap = ShadowDepth;
	}

	NvVl::LightDesc LightDesc;

	FVector Intensity = LightSceneInfo->Proxy->GetNvVlIntensity();
	LightDesc.vIntensity = *reinterpret_cast<const NvcVec3 *>(&Intensity);

	FMatrix LightViewProjInv = LightViewProj.InverseFast();
	LightDesc.mLightToWorld = *reinterpret_cast<const NvcMat44*>(&LightViewProjInv.M[0][0]);

	switch (LightSceneInfo->Proxy->GetLightType())
	{
		case LightType_Point:
		{
			LightDesc.eType = NvVl::LightType::OMNI;
			LightDesc.Omni.fZNear = ShadowInfo->MinSubjectZ;
			LightDesc.Omni.fZFar = ShadowInfo->MaxSubjectZ;
			LightDesc.Omni.vPosition = *reinterpret_cast<const NvcVec3 *>(&LightPosition);
			LightDesc.Omni.eAttenuationMode = (NvVl::AttenuationMode)LightSceneInfo->Proxy->GetNvVlAttenuationMode();
			const FVector4& AttenuationFactors = LightSceneInfo->Proxy->GetNvVlAttenuationFactors();
			LightDesc.Omni.fAttenuationFactors[0] = AttenuationFactors.X;
			LightDesc.Omni.fAttenuationFactors[1] = AttenuationFactors.Y;
			LightDesc.Omni.fAttenuationFactors[2] = AttenuationFactors.Z;
			LightDesc.Omni.fAttenuationFactors[3] = AttenuationFactors.W;
		}
		break;
		case LightType_Spot:
		{
			LightDesc.eType = NvVl::LightType::SPOTLIGHT;
			LightDesc.Spotlight.fZNear = ShadowInfo->MinSubjectZ;
			LightDesc.Spotlight.fZFar = ShadowInfo->MaxSubjectZ;

			LightDesc.Spotlight.eFalloffMode = (NvVl::SpotlightFalloffMode)LightSceneInfo->Proxy->GetNvVlFalloffMode();
			const FVector2D& AngleAndPower = LightSceneInfo->Proxy->GetNvVlFalloffAngleAndPower();
			LightDesc.Spotlight.fFalloff_CosTheta = FMath::Cos(AngleAndPower.X);
			LightDesc.Spotlight.fFalloff_Power = AngleAndPower.Y;
            
			LightDesc.Spotlight.vDirection = *reinterpret_cast<const NvcVec3 *>(&LightDirection);
			LightDesc.Spotlight.vPosition = *reinterpret_cast<const NvcVec3 *>(&LightPosition);
			LightDesc.Spotlight.eAttenuationMode = (NvVl::AttenuationMode)LightSceneInfo->Proxy->GetNvVlAttenuationMode();
			const FVector4& AttenuationFactors = LightSceneInfo->Proxy->GetNvVlAttenuationFactors();
			LightDesc.Spotlight.fAttenuationFactors[0] = AttenuationFactors.X;
			LightDesc.Spotlight.fAttenuationFactors[1] = AttenuationFactors.Y;
			LightDesc.Spotlight.fAttenuationFactors[2] = AttenuationFactors.Z;
			LightDesc.Spotlight.fAttenuationFactors[3] = AttenuationFactors.W;
		}
		break;
		default:
		case LightType_Directional:
		{
			LightDesc.eType = NvVl::LightType::DIRECTIONAL;
			LightDesc.Directional.vDirection = *reinterpret_cast<const NvcVec3 *>(&LightDirection);
		}
	}

	NvVl::VolumeDesc VolumeDesc;
	{
		VolumeDesc.fTargetRayResolution = LightSceneInfo->Proxy->GetNvVlTargetRayResolution();
		VolumeDesc.uMaxMeshResolution = ShadowmapDesc.uWidth;
		VolumeDesc.fDepthBias = LightSceneInfo->Proxy->GetNvVlDepthBias();
		VolumeDesc.eTessQuality = (NvVl::TessellationQuality)LightSceneInfo->Proxy->GetNvVlTessQuality();
	}

	GNVVolumetricLightingRHI->RenderVolume(DepthMap, ShadowmapDesc, LightDesc, VolumeDesc);

	// clear the state cache
	GDynamicRHI->ClearStateCache();
}

// for cascaded shadow
void FDeferredShadingSceneRenderer::NVVolumetricLightingRenderVolume(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& ShadowInfos)
{
	if (!CVarNvVlEnable.GetValueOnRenderThread() || Scene->bSkipCurrentFrameVL)
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_VolumetricLightingRenderVolume);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	FVector LightDirection = LightSceneInfo->Proxy->GetDirection();
	LightDirection.Normalize();

	FMatrix LightViewProj;
	uint32 Cascade = ShadowInfos.Num() - 2; // TODO: better LightToWorld
	LightViewProj = FTranslationMatrix(ShadowInfos[Cascade]->PreShadowTranslation) * ShadowInfos[Cascade]->SubjectAndReceiverMatrix; // use the first cascade as the LightToWorld

	NvVl::ShadowMapDesc ShadowmapDesc;
	uint32 ShadowmapWidth = 0, ShadowmapHeight = 0;

	const FTexture2DRHIRef& ShadowDepth = SceneContext.GetShadowDepthZTexture(false);

	if (ShadowDepth)
	{
		ShadowmapWidth = ShadowDepth->GetSizeX();
		ShadowmapHeight = ShadowDepth->GetSizeY();
	}
	else
	{
		FIntPoint Resolution = SceneContext.GetShadowDepthTextureResolution(); // Pre Cache?? GetPreShadowCacheTextureResolution()
		ShadowmapWidth = Resolution.X;
		ShadowmapHeight = Resolution.Y;
	}

	ShadowmapDesc.eType = NvVl::ShadowMapLayout::SIMPLE;
	ShadowmapDesc.uWidth = ShadowmapWidth;
	ShadowmapDesc.uHeight = ShadowmapHeight;
	// Shadow depth type
	ShadowmapDesc.bLinearizedDepth = false;
	// shadow space
	ShadowmapDesc.bShadowSpace = true;
	ShadowmapDesc.bLowToHighCascadedShadow = true;

	ShadowmapDesc.uElementCount = FMath::Min((uint32)ShadowInfos.Num(), NvVl::MAX_SHADOWMAP_ELEMENTS);
	for (uint32 ElementIndex = 0; ElementIndex < ShadowmapDesc.uElementCount; ElementIndex++)
	{
		FVector4 ShadowmapMinMaxValue;
		FMatrix WorldToShadowMatrixValue = ShadowInfos[ShadowInfos.Num() - ShadowmapDesc.uElementCount + ElementIndex]->GetWorldToShadowMatrix(ShadowmapMinMaxValue);
		ShadowmapDesc.Elements[ElementIndex].uOffsetX = 0;
		ShadowmapDesc.Elements[ElementIndex].uOffsetY = 0;
		ShadowmapDesc.Elements[ElementIndex].uWidth = ShadowmapDesc.uWidth;
		ShadowmapDesc.Elements[ElementIndex].uHeight = ShadowmapDesc.uHeight;
		ShadowmapDesc.Elements[ElementIndex].mViewProj = *reinterpret_cast<const NvcMat44*>(&WorldToShadowMatrixValue.M[0][0]);
		ShadowmapDesc.Elements[ElementIndex].mArrayIndex = ElementIndex;
		ShadowmapDesc.Elements[ElementIndex].vShadowmapMinMaxValue = *reinterpret_cast<const NvcVec4 *>(&ShadowmapMinMaxValue);
	}

	NvVl::LightDesc LightDesc;

	FVector Intensity = LightSceneInfo->Proxy->GetNvVlIntensity();
	LightDesc.vIntensity = *reinterpret_cast<const NvcVec3 *>(&Intensity);

	FMatrix LightViewProjInv = LightViewProj.InverseFast();
	LightDesc.mLightToWorld = *reinterpret_cast<const NvcMat44*>(&LightViewProjInv.M[0][0]);

	LightDesc.eType = NvVl::LightType::DIRECTIONAL;
	LightDesc.Directional.vDirection = *reinterpret_cast<const NvcVec3 *>(&LightDirection);

	NvVl::VolumeDesc VolumeDesc;
	{
		VolumeDesc.fTargetRayResolution = LightSceneInfo->Proxy->GetNvVlTargetRayResolution();
		VolumeDesc.uMaxMeshResolution = ShadowmapDesc.uWidth;
		VolumeDesc.fDepthBias = LightSceneInfo->Proxy->GetNvVlDepthBias();
		VolumeDesc.eTessQuality = (NvVl::TessellationQuality)LightSceneInfo->Proxy->GetNvVlTessQuality();
	}

	GNVVolumetricLightingRHI->RenderVolume(ShadowDepth, ShadowmapDesc, LightDesc, VolumeDesc);

	// clear the state cache
	GDynamicRHI->ClearStateCache();
}


void FDeferredShadingSceneRenderer::NVVolumetricLightingEndAccumulation(FRHICommandListImmediate& RHICmdList)
{
	if (!CVarNvVlEnable.GetValueOnRenderThread() || Scene->bSkipCurrentFrameVL)
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_VolumetricLightingEndAccumulation);
	GNVVolumetricLightingRHI->EndAccumulation();
}

void FDeferredShadingSceneRenderer::NVVolumetricLightingApplyLighting(FRHICommandListImmediate& RHICmdList)
{
	if (!CVarNvVlEnable.GetValueOnRenderThread() || Scene->bSkipCurrentFrameVL)
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_VolumetricLightingApplyLighting);

	check(Views.Num());
	const FViewInfo& View = Views[0];

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	const FNVVolumetricLightingProperties& Properties = Scene->VolumetricLightingProperties;
	const FFinalPostProcessSettings& FinalPostProcessSettings = View.FinalPostProcessSettings;

	NvVl::PostprocessDesc PostprocessDesc;
	PostprocessDesc.bDoFog = FinalPostProcessSettings.FogColor != FLinearColor::Black && FinalPostProcessSettings.FogIntensity > 0 && CVarNvVlFog.GetValueOnRenderThread();
    PostprocessDesc.bIgnoreSkyFog = false;
    PostprocessDesc.eUpsampleQuality = (NvVl::UpsampleQuality)Properties.UpsampleQuality.GetValue();
    PostprocessDesc.fBlendfactor = Properties.Blendfactor;
    PostprocessDesc.fTemporalFactor = Properties.TemporalFactor;
    PostprocessDesc.fFilterThreshold = Properties.FilterThreshold;

	FVector FogLight = FinalPostProcessSettings.FogColor * FinalPostProcessSettings.FogIntensity;
    PostprocessDesc.vFogLight = *reinterpret_cast<const NvcVec3 *>(&FogLight);
    PostprocessDesc.fMultiscatter = MULTI_SCATTER;

#if 0
	if (!SceneContext.IsSeparateTranslucencyActive(Views[0]))
#endif
	{
		GNVVolumetricLightingRHI->ApplyLighting(SceneContext.GetSceneColorSurface(), PostprocessDesc);

		// clear the state cache
		GDynamicRHI->ClearStateCache();
		SetRenderTarget(RHICmdList, FTextureRHIParamRef(), FTextureRHIParamRef());
	}
#if 0
	else
	{
		GNVVolumetricLightingRHI->SetSeparateTranslucencyPostprocess(CVarNvVlEnable.GetValueOnRenderThread() && !Scene->bSkipCurrentFrameVL, PostprocessDesc);
	}
#endif
}

#endif