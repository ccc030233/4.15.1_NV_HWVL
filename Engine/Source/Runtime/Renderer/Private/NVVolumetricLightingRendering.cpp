
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

const float SCATTER_PARAM_SCALE = 100.0f; // 100 unit/m

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
	return -FMath::Loge(InValue) * SCATTER_PARAM_SCALE;
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

	if (!Scene->bEnableProperties)
	{
		return;
	}

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
    
	FNVVolumetricLightingScatteringProperties& ScatteringProperties = Scene->ScatteringProperties;
	

	FVector Absorption = GetOpticalDepth(RemapTransmittance(ScatteringProperties.TransmittanceRange, FVector(FLinearColor(ScatteringProperties.AbsorptionColor) * ScatteringProperties.AbsorptionTransmittance)));
	MediumDesc.vAbsorption = *reinterpret_cast<const NvcVec3 *>(&Absorption);
	MediumDesc.uNumPhaseTerms = 0;
	// Rayleigh
	if (ScatteringProperties.bEnableRayleigh)
	{
		MediumDesc.PhaseTerms[MediumDesc.uNumPhaseTerms].ePhaseFunc = NvVl::PhaseFunctionType::RAYLEIGH;
		FVector Density = GetOpticalDepth(RemapTransmittance(ScatteringProperties.TransmittanceRange, ScatteringProperties.RayleighTransmittance)) * FVector(5.96f, 13.24f, 33.1f);
		MediumDesc.PhaseTerms[MediumDesc.uNumPhaseTerms].vDensity = *reinterpret_cast<const NvcVec3 *>(&Density);
		MediumDesc.uNumPhaseTerms++;
	}

	// Mie
	if(ScatteringProperties.MiePhase != EMiePhase::MIE_OFF)
	{
		MediumDesc.PhaseTerms[MediumDesc.uNumPhaseTerms].ePhaseFunc = ScatteringProperties.MiePhase == EMiePhase::MIE_HAZY ? NvVl::PhaseFunctionType::MIE_HAZY : NvVl::PhaseFunctionType::MIE_MURKY;
		FVector Density = GetOpticalDepth(RemapTransmittance(ScatteringProperties.TransmittanceRange, FVector(FLinearColor(ScatteringProperties.MieColor) * ScatteringProperties.MieTransmittance)));
		MediumDesc.PhaseTerms[MediumDesc.uNumPhaseTerms].vDensity = *reinterpret_cast<const NvcVec3 *>(&Density);
		MediumDesc.uNumPhaseTerms++;
	}

	// HG
	for(int32 PhaseIndex = 0; PhaseIndex < ScatteringProperties.HGScatteringPhases.Num(); PhaseIndex++)
	{
		MediumDesc.PhaseTerms[MediumDesc.uNumPhaseTerms + PhaseIndex].ePhaseFunc = NvVl::PhaseFunctionType::HENYEYGREENSTEIN;
		FVector Density = GetOpticalDepth(RemapTransmittance(ScatteringProperties.TransmittanceRange, FVector(FLinearColor(ScatteringProperties.HGScatteringPhases[PhaseIndex].HGColor) * ScatteringProperties.HGScatteringPhases[PhaseIndex].HGTransmittance)));
		MediumDesc.PhaseTerms[MediumDesc.uNumPhaseTerms + PhaseIndex].vDensity = *reinterpret_cast<const NvcVec3 *>(&Density);
		MediumDesc.PhaseTerms[MediumDesc.uNumPhaseTerms + PhaseIndex].fEccentricity = ScatteringProperties.HGScatteringPhases[PhaseIndex].HGEccentricity;
		MediumDesc.uNumPhaseTerms++;

		if (MediumDesc.uNumPhaseTerms >= NvVl::MAX_PHASE_TERMS)
		{
			break;
		}
	}

	GNVVolumetricLightingRHI->UpdateDownsampleMode(Scene->ContextProperties.DownsampleMode);
	GNVVolumetricLightingRHI->UpdateMsaaMode(Scene->ContextProperties.MsaaMode);
	GNVVolumetricLightingRHI->UpdateFilterMode(Scene->ContextProperties.FilterMode);

	int32 DebugMode = FMath::Clamp((int32)CVarNvVlDebugMode.GetValueOnRenderThread(), 0, 2);
	GNVVolumetricLightingRHI->BeginAccumulation(SceneContext.GetSceneDepthTexture(), ViewerDesc, MediumDesc, (Nv::VolumetricLighting::DebugFlags)DebugMode); //SceneContext.GetActualDepthTexture()?

	// clear the state cache
	GDynamicRHI->ClearStateCache();
}

void FDeferredShadingSceneRenderer::NVVolumetricLightingRemapShadowDepth(FRHICommandListImmediate& RHICmdList)
{
	if (!CVarNvVlEnable.GetValueOnRenderThread())
	{
		return;
	}

	if (!Scene->bEnableProperties)
	{
		return;
	}

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	const FTexture2DRHIRef& ShadowDepth = SceneContext.GetShadowDepthZTexture(false);
	GNVVolumetricLightingRHI->RemapShadowDepth(ShadowDepth);

	// clear the state cache
	GDynamicRHI->ClearStateCache();
}

void FDeferredShadingSceneRenderer::NVVolumetricLightingRenderVolume(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, const FProjectedShadowInfo* ShadowInfo)
{
	if (!CVarNvVlEnable.GetValueOnRenderThread() || !LightSceneInfo->Proxy->IsNVVolumetricLighting())
	{
		return;
	}

	if (!Scene->bEnableProperties)
	{
		return;
	}

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
	if (!CVarNvVlEnable.GetValueOnRenderThread() || !LightSceneInfo->Proxy->IsNVVolumetricLighting())
	{
		return;
	}

	if (!Scene->bEnableProperties)
	{
		return;
	}

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
	if (!CVarNvVlEnable.GetValueOnRenderThread())
	{
		return;
	}

	if (!Scene->bEnableProperties)
	{
		return;
	}

	GNVVolumetricLightingRHI->EndAccumulation();
}

void FDeferredShadingSceneRenderer::NVVolumetricLightingApplyLighting(FRHICommandListImmediate& RHICmdList)
{
	if (!CVarNvVlEnable.GetValueOnRenderThread())
	{
		return;
	}

	if (!Scene->bEnableProperties)
	{
		return;
	}

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	FNVVolumetricLightingPostprocessProperties& PostprocessProperties = Scene->PostprocessProperties;

	NvVl::PostprocessDesc PostprocessDesc;
	PostprocessDesc.bDoFog = PostprocessProperties.bEnableFog;
    PostprocessDesc.bIgnoreSkyFog = false;
    PostprocessDesc.eUpsampleQuality = (NvVl::UpsampleQuality)PostprocessProperties.UpsampleQuality.GetValue();
    PostprocessDesc.fBlendfactor = PostprocessProperties.Blendfactor;
    PostprocessDesc.fTemporalFactor = PostprocessProperties.TemporalFactor;
    PostprocessDesc.fFilterThreshold = PostprocessProperties.FilterThreshold;

	FVector FogLight = FLinearColor(PostprocessProperties.FogColor) * PostprocessProperties.FogIntensity;
    PostprocessDesc.vFogLight = *reinterpret_cast<const NvcVec3 *>(&FogLight);
    PostprocessDesc.fMultiscatter = PostprocessProperties.MultiScatter * 0.0001f; // 10^-6 * 100 unit

	check(Views.Num());
	if (!SceneContext.IsSeparateTranslucencyActive(Views[0]))
	{
		GNVVolumetricLightingRHI->ApplyLighting(SceneContext.GetSceneColorSurface(), PostprocessDesc);

		// clear the state cache
		GDynamicRHI->ClearStateCache();
		SetRenderTarget(RHICmdList, FTextureRHIParamRef(), FTextureRHIParamRef());
	}
	else
	{
		GNVVolumetricLightingRHI->SetSeparateTranslucencyPostprocess(CVarNvVlEnable.GetValueOnRenderThread() && Scene->bEnableProperties, PostprocessDesc);
	}
}

#endif