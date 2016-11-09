
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

static TAutoConsoleVariable<float> CVarNvVlDirectionalVolumeScale(
	TEXT("r.NvVl.DirectionalVolumeScale"),
	5.0f,
	TEXT("Volume Scale for the directional light\n"),
	ECVF_RenderThreadSafe);

DECLARE_CYCLE_STAT(TEXT("Volumetric Lighting Begin Accumulation"), STAT_VolumetricLightingBeginAccumulation, STATGROUP_SceneRendering);
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
	if (GNVVolumetricLightingRHI == nullptr || !CVarNvVlEnable.GetValueOnRenderThread())
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
		FIntPoint BufferSize = SceneContext.GetBufferSizeXY();
		GNVVolumetricLightingRHI->UpdateFrameBuffer(BufferSize.X, BufferSize.Y, 1);
		GNVVolumetricLightingRHI->UpdateDownsampleMode(Properties.DownsampleMode);
		GNVVolumetricLightingRHI->UpdateMsaaMode(Properties.MsaaMode);
		GNVVolumetricLightingRHI->UpdateFilterMode(Properties.FilterMode);

		int32 DebugMode = FMath::Clamp((int32)CVarNvVlDebugMode.GetValueOnRenderThread(), 0, 2);
		RHICmdList.BeginAccumulation(SceneContext.GetSceneDepthTexture(), ViewerDesc, MediumDesc, (Nv::VolumetricLighting::DebugFlags)DebugMode); //SceneContext.GetActualDepthTexture()?

		// clear the state cache
		RHICmdList.ClearStateCache();
		SetRenderTarget(RHICmdList, FTextureRHIParamRef(), FTextureRHIParamRef());
	}
}

void GetLightMatrix(const FWholeSceneProjectedShadowInitializer& Initializer, float& MinSubjectZ, float& MaxSubjectZ, FVector& PreShadowTranslation, FMatrix& SubjectAndReceiverMatrix)
{
	FVector	XAxis, YAxis;
	Initializer.FaceDirection.FindBestAxisVectors(XAxis,YAxis);
	const FMatrix WorldToLightScaled = Initializer.WorldToLight * FScaleMatrix(Initializer.Scales);
	const FMatrix WorldToFace = WorldToLightScaled * FBasisVectorMatrix(-XAxis,YAxis,Initializer.FaceDirection.GetSafeNormal(),FVector::ZeroVector);

	MaxSubjectZ = WorldToFace.TransformPosition(Initializer.SubjectBounds.Origin).Z + Initializer.SubjectBounds.SphereRadius;
	MinSubjectZ = FMath::Max(MaxSubjectZ - Initializer.SubjectBounds.SphereRadius * 2,Initializer.MinLightW);
	PreShadowTranslation = Initializer.PreShadowTranslation;
	SubjectAndReceiverMatrix = WorldToFace * FShadowProjectionMatrix(MinSubjectZ, MaxSubjectZ, Initializer.WAxis);
}

void FDeferredShadingSceneRenderer::NVVolumetricLightingRenderVolume(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo)
{
	if (GNVVolumetricLightingRHI == nullptr || !CVarNvVlEnable.GetValueOnRenderThread() || Scene->bSkipCurrentFrameVL)
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_VolumetricLightingRenderVolume);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	FVector LightPosition = LightSceneInfo->Proxy->GetOrigin();
	FVector LightDirection = LightSceneInfo->Proxy->GetDirection();
	LightDirection.Normalize();

	float MinSubjectZ = 0, MaxSubjectZ = 0;
	FVector PreShadowTranslation;
	FMatrix SubjectAndReceiverMatrix;

	if (LightSceneInfo->Proxy->GetLightType() == LightType_Directional)
	{
		FWholeSceneProjectedShadowInitializer ProjectedShadowInitializer;
		if (LightSceneInfo->Proxy->GetViewDependentWholeSceneProjectedShadowInitializer(Views[0], INDEX_NONE, LightSceneInfo->IsPrecomputedLightingValid(), ProjectedShadowInitializer))
		{
			GetLightMatrix(ProjectedShadowInitializer, MinSubjectZ, MaxSubjectZ, PreShadowTranslation, SubjectAndReceiverMatrix);
		}
	}
	else
	{
		TArray<FWholeSceneProjectedShadowInitializer, TInlineAllocator<6> > ProjectedShadowInitializers;
		if (LightSceneInfo->Proxy->GetWholeSceneProjectedShadowInitializer(ViewFamily, ProjectedShadowInitializers))
		{
			GetLightMatrix(ProjectedShadowInitializers[0], MinSubjectZ, MaxSubjectZ, PreShadowTranslation, SubjectAndReceiverMatrix);
		}
	}

	FMatrix LightViewProj;
	if (LightSceneInfo->Proxy->GetLightType() == LightType_Point)
	{
		LightViewProj = FTranslationMatrix(-LightPosition);
	}
	else
	{
		LightViewProj = FTranslationMatrix(PreShadowTranslation) * SubjectAndReceiverMatrix;
	}

	NvVl::ShadowMapDesc ShadowmapDesc;
	FIntPoint Resolution = SceneContext.GetShadowDepthTextureResolution();

	ShadowmapDesc.eType = NvVl::ShadowMapLayout::SIMPLE;
	ShadowmapDesc.uWidth = Resolution.X;
	ShadowmapDesc.uHeight = Resolution.Y;
	// Shadow depth type
	ShadowmapDesc.bLinearizedDepth = false;
	// shadow space
	ShadowmapDesc.bShadowSpace = false;
	ShadowmapDesc.bInternalShadowDepth = false;

	ShadowmapDesc.uElementCount = 1;
	ShadowmapDesc.Elements[0].uOffsetX = 0;
	ShadowmapDesc.Elements[0].uOffsetY = 0;
	ShadowmapDesc.Elements[0].uWidth = ShadowmapDesc.uWidth;
	ShadowmapDesc.Elements[0].uHeight = ShadowmapDesc.uHeight;
	ShadowmapDesc.Elements[0].mArrayIndex = 0;

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
			LightDesc.Omni.fZNear = MinSubjectZ;
			LightDesc.Omni.fZFar = MaxSubjectZ;
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
			LightDesc.Spotlight.fZNear = MinSubjectZ;
			LightDesc.Spotlight.fZFar = MaxSubjectZ;

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
			LightDesc.Directional.bIgnoreSkyScattering = !LightSceneInfo->Proxy->IsNvVlSkyScattering();
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

	TArray<FTextureRHIParamRef> ShadowDepthTextures;
	RHICmdList.RenderVolume(ShadowDepthTextures, ShadowmapDesc, LightDesc, VolumeDesc);

	// clear the state cache
	RHICmdList.ClearStateCache();
	SetRenderTarget(RHICmdList, FTextureRHIParamRef(), FTextureRHIParamRef());
}

void FDeferredShadingSceneRenderer::NVVolumetricLightingRenderVolume(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, const FProjectedShadowInfo* ShadowInfo)
{
	if (GNVVolumetricLightingRHI == nullptr || !CVarNvVlEnable.GetValueOnRenderThread() || Scene->bSkipCurrentFrameVL)
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
	TArray<FTextureRHIParamRef> ShadowDepthTextures;

	if (LightSceneInfo->Proxy->GetLightType() == LightType_Point)
	{
		const FTextureCubeRHIRef& ShadowDepthTexture = ShadowInfo->RenderTargets.DepthTarget->GetRenderTargetItem().ShaderResourceTexture->GetTextureCube();
		if (ShadowDepthTexture)
		{
			ShadowmapWidth = ShadowDepthTexture->GetSize();
			ShadowmapHeight = ShadowDepthTexture->GetSize();
		}

		ShadowmapDesc.eType = NvVl::ShadowMapLayout::CUBE;
		ShadowmapDesc.uWidth = ShadowmapWidth;
		ShadowmapDesc.uHeight = ShadowmapHeight;
		// Shadow depth type
		ShadowmapDesc.bLinearizedDepth = false;
		// shadow space
		ShadowmapDesc.bShadowSpace = false;
		ShadowmapDesc.bInternalShadowDepth = false;

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

		ShadowDepthTextures.Add(ShadowDepthTexture);
	}
	else
	{
		const FTexture2DRHIRef& ShadowDepthTexture = ShadowInfo->RenderTargets.DepthTarget->GetRenderTargetItem().ShaderResourceTexture->GetTexture2D();

		if (ShadowDepthTexture)
		{
			ShadowmapWidth = ShadowDepthTexture->GetSizeX();
			ShadowmapHeight = ShadowDepthTexture->GetSizeY();
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
		ShadowmapDesc.bInternalShadowDepth = false;

		ShadowmapDesc.uElementCount = 1;
		ShadowmapDesc.Elements[0].uOffsetX = 0;
		ShadowmapDesc.Elements[0].uOffsetY = 0;
		ShadowmapDesc.Elements[0].uWidth = ShadowmapDesc.uWidth;
		ShadowmapDesc.Elements[0].uHeight = ShadowmapDesc.uHeight;
		ShadowmapDesc.Elements[0].mViewProj = *reinterpret_cast<const NvcMat44*>(&WorldToShadowMatrixValue.M[0][0]);
		ShadowmapDesc.Elements[0].mArrayIndex = 0;
		ShadowmapDesc.Elements[0].fInvMaxSubjectDepth = ShadowInfo->InvMaxSubjectDepth;
		ShadowmapDesc.Elements[0].vShadowmapMinMaxValue = *reinterpret_cast<const NvcVec4 *>(&ShadowmapMinMaxValue);

		ShadowDepthTextures.Add(ShadowDepthTexture);
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
			LightDesc.Directional.bIgnoreSkyScattering = !LightSceneInfo->Proxy->IsNvVlSkyScattering();
			LightDesc.Directional.vDirection = *reinterpret_cast<const NvcVec3 *>(&LightDirection);
		}
	}

	NvVl::VolumeDesc VolumeDesc;
	{
		VolumeDesc.fTargetRayResolution = LightSceneInfo->Proxy->GetNvVlTargetRayResolution();
		VolumeDesc.uMaxMeshResolution = SceneContext.GetShadowDepthTextureResolution().X;
		VolumeDesc.fDepthBias = LightSceneInfo->Proxy->GetNvVlDepthBias();
		VolumeDesc.eTessQuality = (NvVl::TessellationQuality)LightSceneInfo->Proxy->GetNvVlTessQuality();
	}

	RHICmdList.RenderVolume(ShadowDepthTextures, ShadowmapDesc, LightDesc, VolumeDesc);

	// clear the state cache
	RHICmdList.ClearStateCache();
	SetRenderTarget(RHICmdList, FTextureRHIParamRef(), FTextureRHIParamRef());
}

// for cascaded shadow
void FDeferredShadingSceneRenderer::NVVolumetricLightingRenderVolume(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& ShadowInfos)
{
	if (GNVVolumetricLightingRHI == nullptr || !CVarNvVlEnable.GetValueOnRenderThread() || Scene->bSkipCurrentFrameVL)
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_VolumetricLightingRenderVolume);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	FVector LightDirection = LightSceneInfo->Proxy->GetDirection();
	LightDirection.Normalize();

	FMatrix LightViewProj;
	const uint32 Cascade = ShadowInfos.Num() - 1; // Take the last cascade as the base
	const FMatrix ShadowProjection = FShadowProjectionMatrix(ShadowInfos[Cascade]->MinSubjectZ, ShadowInfos[Cascade]->MaxSubjectZ, FVector4(0,0,0,1));
	const FMatrix WorldToFace = ShadowInfos[Cascade]->SubjectAndReceiverMatrix * ShadowProjection.InverseFast();
	float VolumeScale = CVarNvVlDirectionalVolumeScale.GetValueOnRenderThread();
	const FMatrix SubjectAndReceiverMatrix = WorldToFace * FScaleMatrix(1/(VolumeScale > 0 ? VolumeScale : 1.0f)) * ShadowProjection;
	LightViewProj = FTranslationMatrix(ShadowInfos[Cascade]->PreShadowTranslation) * SubjectAndReceiverMatrix;

	NvVl::ShadowMapDesc ShadowmapDesc;
	TArray<FTextureRHIParamRef> ShadowDepthTextures;
	uint32 ShadowmapWidth = 0, ShadowmapHeight = 0;

	bool bAtlassing = true;
	const FTexture2DRHIRef& ShadowDepthTexture = ShadowInfos[0]->RenderTargets.DepthTarget->GetRenderTargetItem().ShaderResourceTexture->GetTexture2D();

	//check Shadow Depth Texture for each shadow info
	for ( int i = 1; i < ShadowInfos.Num(); i++)
	{
		if(ShadowDepthTexture != ShadowInfos[i]->RenderTargets.DepthTarget->GetRenderTargetItem().ShaderResourceTexture->GetTexture2D())
		{
			bAtlassing = false;
			break;
		}
	}

	if (bAtlassing)
	{
		if (ShadowDepthTexture)
		{
			ShadowmapWidth = ShadowDepthTexture->GetSizeX();
			ShadowmapHeight = ShadowDepthTexture->GetSizeY();
		}
		else
		{
			FIntPoint Resolution = SceneContext.GetShadowDepthTextureResolution();
			ShadowmapWidth = Resolution.X;
			ShadowmapHeight = Resolution.Y;
		}
		ShadowmapDesc.uWidth = ShadowmapWidth;
		ShadowmapDesc.uHeight = ShadowmapHeight;

		ShadowDepthTextures.Add(ShadowDepthTexture);
	}

	ShadowmapDesc.eType = bAtlassing ? NvVl::ShadowMapLayout::CASCADE_ATLAS : NvVl::ShadowMapLayout::CASCADE_MULTI;
	// Shadow depth type
	ShadowmapDesc.bLinearizedDepth = false;
	// shadow space
	ShadowmapDesc.bShadowSpace = true;
	ShadowmapDesc.bInternalShadowDepth = false;

	ShadowmapDesc.uElementCount = FMath::Min((uint32)ShadowInfos.Num(), NvVl::MAX_SHADOWMAP_ELEMENTS);

	for (uint32 ElementIndex = 0; ElementIndex < ShadowmapDesc.uElementCount; ElementIndex++)
	{
		FVector4 ShadowmapMinMaxValue;
		uint32 ShadowIndex = ShadowInfos.Num() - ShadowmapDesc.uElementCount + ElementIndex;
		FMatrix WorldToShadowMatrixValue = ShadowInfos[ShadowIndex]->GetWorldToShadowMatrix(ShadowmapMinMaxValue);
		ShadowmapDesc.Elements[ElementIndex].uOffsetX = 0;
		ShadowmapDesc.Elements[ElementIndex].uOffsetY = 0;
		ShadowmapDesc.Elements[ElementIndex].mViewProj = *reinterpret_cast<const NvcMat44*>(&WorldToShadowMatrixValue.M[0][0]);
		ShadowmapDesc.Elements[ElementIndex].mArrayIndex = ElementIndex;
		ShadowmapDesc.Elements[ElementIndex].vShadowmapMinMaxValue = *reinterpret_cast<const NvcVec4 *>(&ShadowmapMinMaxValue);

		if (bAtlassing)
		{
			ShadowmapDesc.Elements[ElementIndex].uWidth = ShadowmapWidth;
			ShadowmapDesc.Elements[ElementIndex].uHeight = ShadowmapHeight;
		}
		else
		{
			const FTexture2DRHIRef& DepthTexture = ShadowInfos[ShadowIndex]->RenderTargets.DepthTarget->GetRenderTargetItem().ShaderResourceTexture->GetTexture2D();

			ShadowmapDesc.Elements[ElementIndex].uWidth = DepthTexture->GetSizeX();
			ShadowmapDesc.Elements[ElementIndex].uHeight = DepthTexture->GetSizeY();
			ShadowDepthTextures.Add(DepthTexture);
		}
	}

	NvVl::LightDesc LightDesc;

	FVector Intensity = LightSceneInfo->Proxy->GetNvVlIntensity();
	LightDesc.vIntensity = *reinterpret_cast<const NvcVec3 *>(&Intensity);

	FMatrix LightViewProjInv = LightViewProj.InverseFast();
	LightDesc.mLightToWorld = *reinterpret_cast<const NvcMat44*>(&LightViewProjInv.M[0][0]);

	LightDesc.eType = NvVl::LightType::DIRECTIONAL;
	LightDesc.Directional.bIgnoreSkyScattering = !LightSceneInfo->Proxy->IsNvVlSkyScattering();
	LightDesc.Directional.vDirection = *reinterpret_cast<const NvcVec3 *>(&LightDirection);

	NvVl::VolumeDesc VolumeDesc;
	{
		VolumeDesc.fTargetRayResolution = LightSceneInfo->Proxy->GetNvVlTargetRayResolution();
		VolumeDesc.uMaxMeshResolution = SceneContext.GetShadowDepthTextureResolution().X;
		VolumeDesc.fDepthBias = LightSceneInfo->Proxy->GetNvVlDepthBias();
		VolumeDesc.eTessQuality = (NvVl::TessellationQuality)LightSceneInfo->Proxy->GetNvVlTessQuality();
	}

	RHICmdList.RenderVolume(ShadowDepthTextures, ShadowmapDesc, LightDesc, VolumeDesc);

	// clear the state cache
	RHICmdList.ClearStateCache();
	SetRenderTarget(RHICmdList, FTextureRHIParamRef(), FTextureRHIParamRef());
}


void FDeferredShadingSceneRenderer::NVVolumetricLightingEndAccumulation(FRHICommandListImmediate& RHICmdList)
{
	if (GNVVolumetricLightingRHI == nullptr || !CVarNvVlEnable.GetValueOnRenderThread() || Scene->bSkipCurrentFrameVL)
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_VolumetricLightingEndAccumulation);
	RHICmdList.EndAccumulation();
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
	PostprocessDesc.bDoFog = (FinalPostProcessSettings.FogMode != EFogMode::FOG_NONE) && CVarNvVlFog.GetValueOnRenderThread();
    PostprocessDesc.bIgnoreSkyFog = FinalPostProcessSettings.FogMode == EFogMode::FOG_NOSKY;
    PostprocessDesc.eUpsampleQuality = (NvVl::UpsampleQuality)Properties.UpsampleQuality.GetValue();
    PostprocessDesc.fBlendfactor = Properties.Blendfactor;
    PostprocessDesc.fTemporalFactor = Properties.TemporalFactor;
    PostprocessDesc.fFilterThreshold = Properties.FilterThreshold;
	FMatrix ViewProjNoAAMatrix = View.ViewMatrices.ViewMatrix * View.ViewMatrices.GetProjNoAAMatrix();
	PostprocessDesc.mUnjitteredViewProj = *reinterpret_cast<const NvcMat44*>(&ViewProjNoAAMatrix.M[0][0]);

	FVector FogLight = FinalPostProcessSettings.FogColor * FinalPostProcessSettings.FogIntensity;
    PostprocessDesc.vFogLight = *reinterpret_cast<const NvcVec3 *>(&FogLight);
    PostprocessDesc.fMultiscatter = FinalPostProcessSettings.MultiScatter;

	RHICmdList.ApplyLighting(SceneContext.GetSceneColorSurface(), PostprocessDesc);

	// clear the state cache
	RHICmdList.ClearStateCache();
	if (Properties.MsaaMode == MultisampleMode::SINGLE && Properties.FilterMode == EFilterMode::NONE)
	{
		SetRenderTarget(RHICmdList, FTextureRHIParamRef(), FTextureRHIParamRef());
	}
	else
	{
		FTextureRHIParamRef RenderTargets[2] =
		{
			FTextureRHIParamRef(),
			FTextureRHIParamRef()
		};
		SetRenderTargets(RHICmdList, 2, RenderTargets, FTextureRHIParamRef(), 0, NULL);
	}
}

#endif