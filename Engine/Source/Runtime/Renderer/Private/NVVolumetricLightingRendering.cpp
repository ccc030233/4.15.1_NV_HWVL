
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

static TAutoConsoleVariable<int32> CVarNvVlSPS(
	TEXT("r.NvVl.SPS"),
	0,
	TEXT("Enable Single Pass Stereo\n")
	TEXT("  0: off\n")
	TEXT("  1: on\n"),
	ECVF_RenderThreadSafe);

static NvVl::ContextDesc NvVlContextDesc;
static NvVl::ViewerDesc NvVlViewerDesc;
static NvVl::MediumDesc NvVlMediumDesc;
static NvVl::ShadowMapDesc NvVlShadowMapDesc;
static NvVl::LightDesc NvVlLightDesc;
static NvVl::VolumeDesc NvVlVolumeDesc;
static NvVl::PostprocessDesc NvVlPostprocessDesc;

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
	if (GNVVolumetricLightingRHI == nullptr)
	{
		return;
	}

	if (!CVarNvVlEnable.GetValueOnRenderThread() || GetShadowQuality() == 0)
	{
		GNVVolumetricLightingRHI->UpdateRendering(false);

		// cleanup render resource
		GNVVolumetricLightingRHI->ReleaseContext();
		return;
	}

	check(Views.Num());
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	
	TArray<NvVl::ViewerDesc> ViewerDescs;

	for(int i = 0; i < ((Views.Num() > 1) ? 2:1); ++i)
	{
		const FViewInfo& View = Views[i];
		FMemory::Memzero(NvVlViewerDesc);
		FMatrix ProjMatrix = View.ViewMatrices.GetProjectionMatrix();
		NvVlViewerDesc.mProj = *reinterpret_cast<const NvcMat44*>(&ProjMatrix.M[0][0]);
		FMatrix ViewProjMatrix = View.ViewMatrices.GetViewProjectionMatrix();
		NvVlViewerDesc.mViewProj = *reinterpret_cast<const NvcMat44*>(&ViewProjMatrix.M[0][0]);
		NvVlViewerDesc.vEyePosition = *reinterpret_cast<const NvcVec3 *>(&View.ViewLocation);
		NvVlViewerDesc.uViewportTopLeftX = View.ViewRect.Min.X;
		NvVlViewerDesc.uViewportTopLeftY = View.ViewRect.Min.Y;
		NvVlViewerDesc.uViewportWidth = View.ViewRect.Width();
		NvVlViewerDesc.uViewportHeight = View.ViewRect.Height();
		ViewerDescs.Add(NvVlViewerDesc);
	}

	const FViewInfo& View = Views[0];
	FMemory::Memzero(NvVlMediumDesc);
	const FNVVolumetricLightingProperties& Properties = Scene->VolumetricLightingProperties;
	const FFinalPostProcessSettings& FinalPostProcessSettings = View.FinalPostProcessSettings;

	FVector Absorption = GetOpticalDepth(RemapTransmittance(FinalPostProcessSettings.TransmittanceRange, FVector(FinalPostProcessSettings.AbsorptionColor * FinalPostProcessSettings.AbsorptionTransmittance)));
	NvVlMediumDesc.vAbsorption = *reinterpret_cast<const NvcVec3 *>(&Absorption);
	NvVlMediumDesc.uNumPhaseTerms = 0;
	// Rayleigh
	if (FinalPostProcessSettings.RayleighTransmittance < 1.0f)
	{
		NvVlMediumDesc.PhaseTerms[NvVlMediumDesc.uNumPhaseTerms].ePhaseFunc = NvVl::PhaseFunctionType::RAYLEIGH;
		FVector Density = GetOpticalDepth(RemapTransmittance(FinalPostProcessSettings.TransmittanceRange, FinalPostProcessSettings.RayleighTransmittance)) * FVector(5.96f, 13.24f, 33.1f);
		NvVlMediumDesc.PhaseTerms[NvVlMediumDesc.uNumPhaseTerms].vDensity = *reinterpret_cast<const NvcVec3 *>(&Density);
		NvVlMediumDesc.uNumPhaseTerms++;
	}

	// Mie
	if(FinalPostProcessSettings.MiePhase != EMiePhase::MIE_NONE)
	{
		NvVlMediumDesc.PhaseTerms[NvVlMediumDesc.uNumPhaseTerms].ePhaseFunc = FinalPostProcessSettings.MiePhase == EMiePhase::MIE_HAZY ? NvVl::PhaseFunctionType::MIE_HAZY : NvVl::PhaseFunctionType::MIE_MURKY;
		FVector Density = GetOpticalDepth(RemapTransmittance(FinalPostProcessSettings.TransmittanceRange, FVector(FinalPostProcessSettings.MieColor * FinalPostProcessSettings.MieTransmittance)));
		NvVlMediumDesc.PhaseTerms[NvVlMediumDesc.uNumPhaseTerms].vDensity = *reinterpret_cast<const NvcVec3 *>(&Density);
		NvVlMediumDesc.uNumPhaseTerms++;
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
			int32 PhaseTermsIndex = NvVlMediumDesc.uNumPhaseTerms + TermIndex;
			NvVlMediumDesc.PhaseTerms[PhaseTermsIndex].ePhaseFunc = NvVl::PhaseFunctionType::HENYEYGREENSTEIN;
			FVector Density = GetOpticalDepth(RemapTransmittance(FinalPostProcessSettings.TransmittanceRange, FVector(Color * Transmittance)));
			NvVlMediumDesc.PhaseTerms[PhaseTermsIndex].vDensity = *reinterpret_cast<const NvcVec3 *>(&Density);
			NvVlMediumDesc.PhaseTerms[PhaseTermsIndex].fEccentricity = Eccentricity;
			NvVlMediumDesc.uNumPhaseTerms++;

			if (NvVlMediumDesc.uNumPhaseTerms >= NvVl::MAX_PHASE_TERMS)
			{
				break;
			}
		}
	}

	GNVVolumetricLightingRHI->UpdateRendering(NvVlMediumDesc.uNumPhaseTerms != 0);

	if (GNVVolumetricLightingRHI->IsRendering())
	{
		SCOPED_DRAW_EVENT(RHICmdList, VolumetricLightingBeginAccumulation);
		SCOPED_GPU_STAT(RHICmdList, Stat_GPU_BeginAccumulation);

		FIntPoint BufferSize = SceneContext.GetBufferSizeXY();
		
		FMemory::Memzero(NvVlContextDesc);
		NvVlContextDesc.framebuffer.uWidth = BufferSize.X;
		NvVlContextDesc.framebuffer.uHeight = BufferSize.Y;
		NvVlContextDesc.framebuffer.uSamples = 1;
		NvVlContextDesc.bStereoEnabled = Views.Num() > 1;
		NvVlContextDesc.bSinglePassStereo = CVarNvVlSPS.GetValueOnRenderThread() != 0;
		NvVlContextDesc.bReversedZ = (int32)ERHIZBuffer::IsInverted != 0;
		NvVlContextDesc.eDownsampleMode = (NvVl::DownsampleMode)Properties.DownsampleMode.GetValue();
		NvVlContextDesc.eInternalSampleMode = (NvVl::MultisampleMode)Properties.MsaaMode.GetValue();
		NvVlContextDesc.eFilterMode = (NvVl::FilterMode)Properties.FilterMode.GetValue();
		GNVVolumetricLightingRHI->UpdateContext(NvVlContextDesc);

		int32 DebugMode = FMath::Clamp((int32)CVarNvVlDebugMode.GetValueOnRenderThread(), 0, 2);
		RHICmdList.BeginAccumulation(SceneContext.GetSceneDepthTexture(), ViewerDescs, NvVlMediumDesc, (Nv::VolumetricLighting::DebugFlags)DebugMode); //SceneContext.GetActualDepthTexture()?
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
	if (GNVVolumetricLightingRHI == nullptr || !GNVVolumetricLightingRHI->IsRendering())
	{
		return;
	}

	SCOPED_DRAW_EVENT(RHICmdList, VolumetricLightingRenderVolume);
	SCOPED_GPU_STAT(RHICmdList, Stat_GPU_RenderVolume);

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

	FMemory::Memzero(NvVlShadowMapDesc);
	FIntPoint Resolution = SceneContext.GetShadowDepthTextureResolution();

	NvVlShadowMapDesc.eType = NvVl::ShadowMapLayout::SIMPLE;
	NvVlShadowMapDesc.uWidth = Resolution.X;
	NvVlShadowMapDesc.uHeight = Resolution.Y;
	// Shadow depth type
	NvVlShadowMapDesc.bLinearizedDepth = false;
	// shadow space
	NvVlShadowMapDesc.bShadowSpace = false;

	NvVlShadowMapDesc.uElementCount = 1;
	NvVlShadowMapDesc.Elements[0].uOffsetX = 0;
	NvVlShadowMapDesc.Elements[0].uOffsetY = 0;
	NvVlShadowMapDesc.Elements[0].uWidth = NvVlShadowMapDesc.uWidth;
	NvVlShadowMapDesc.Elements[0].uHeight = NvVlShadowMapDesc.uHeight;
	NvVlShadowMapDesc.Elements[0].mArrayIndex = 0;

	FMemory::Memzero(NvVlLightDesc);

	FVector Intensity = LightSceneInfo->Proxy->GetNvVlIntensity();
	NvVlLightDesc.vIntensity = *reinterpret_cast<const NvcVec3 *>(&Intensity);

	FMatrix LightViewProjInv = LightViewProj.InverseFast();
	NvVlLightDesc.mLightToWorld = *reinterpret_cast<const NvcMat44*>(&LightViewProjInv.M[0][0]);

	switch (LightSceneInfo->Proxy->GetLightType())
	{
		case LightType_Point:
		{
			NvVlLightDesc.eType = NvVl::LightType::OMNI;
			NvVlLightDesc.Omni.fZNear = MinSubjectZ;
			NvVlLightDesc.Omni.fZFar = MaxSubjectZ;
			NvVlLightDesc.Omni.vPosition = *reinterpret_cast<const NvcVec3 *>(&LightPosition);
			NvVlLightDesc.Omni.eAttenuationMode = (NvVl::AttenuationMode)LightSceneInfo->Proxy->GetNvVlAttenuationMode();
			const FVector4& AttenuationFactors = LightSceneInfo->Proxy->GetNvVlAttenuationFactors();
			NvVlLightDesc.Omni.fAttenuationFactors[0] = AttenuationFactors.X;
			NvVlLightDesc.Omni.fAttenuationFactors[1] = AttenuationFactors.Y;
			NvVlLightDesc.Omni.fAttenuationFactors[2] = AttenuationFactors.Z;
			NvVlLightDesc.Omni.fAttenuationFactors[3] = AttenuationFactors.W;
		}
		break;
		case LightType_Spot:
		{
			NvVlLightDesc.eType = NvVl::LightType::SPOTLIGHT;
			NvVlLightDesc.Spotlight.fZNear = MinSubjectZ;
			NvVlLightDesc.Spotlight.fZFar = MaxSubjectZ;

			NvVlLightDesc.Spotlight.eFalloffMode = (NvVl::SpotlightFalloffMode)LightSceneInfo->Proxy->GetNvVlFalloffMode();
			const FVector2D& AngleAndPower = LightSceneInfo->Proxy->GetNvVlFalloffAngleAndPower();
			NvVlLightDesc.Spotlight.fFalloff_CosTheta = FMath::Cos(AngleAndPower.X);
			NvVlLightDesc.Spotlight.fFalloff_Power = AngleAndPower.Y;
            
			NvVlLightDesc.Spotlight.vDirection = *reinterpret_cast<const NvcVec3 *>(&LightDirection);
			NvVlLightDesc.Spotlight.vPosition = *reinterpret_cast<const NvcVec3 *>(&LightPosition);
			NvVlLightDesc.Spotlight.eAttenuationMode = (NvVl::AttenuationMode)LightSceneInfo->Proxy->GetNvVlAttenuationMode();
			const FVector4& AttenuationFactors = LightSceneInfo->Proxy->GetNvVlAttenuationFactors();
			NvVlLightDesc.Spotlight.fAttenuationFactors[0] = AttenuationFactors.X;
			NvVlLightDesc.Spotlight.fAttenuationFactors[1] = AttenuationFactors.Y;
			NvVlLightDesc.Spotlight.fAttenuationFactors[2] = AttenuationFactors.Z;
			NvVlLightDesc.Spotlight.fAttenuationFactors[3] = AttenuationFactors.W;
		}
		break;
		default:
		case LightType_Directional:
		{
			NvVlLightDesc.eType = NvVl::LightType::DIRECTIONAL;
			NvVlLightDesc.Directional.vDirection = *reinterpret_cast<const NvcVec3 *>(&LightDirection);
		}
	}

	FMemory::Memzero(NvVlVolumeDesc);
	{
		NvVlVolumeDesc.fTargetRayResolution = LightSceneInfo->Proxy->GetNvVlTargetRayResolution();
		NvVlVolumeDesc.uMaxMeshResolution = NvVlShadowMapDesc.uWidth;
		NvVlVolumeDesc.fDepthBias = LightSceneInfo->Proxy->GetNvVlDepthBias();
		NvVlVolumeDesc.eTessQuality = (NvVl::TessellationQuality)LightSceneInfo->Proxy->GetNvVlTessQuality();
	}

	TArray<FTextureRHIParamRef> ShadowDepthTextures;
	RHICmdList.RenderVolume(ShadowDepthTextures, NvVlShadowMapDesc, NvVlLightDesc, NvVlVolumeDesc);
}

void FDeferredShadingSceneRenderer::NVVolumetricLightingRenderVolume(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, const FProjectedShadowInfo* ShadowInfo)
{
	if (GNVVolumetricLightingRHI == nullptr || !GNVVolumetricLightingRHI->IsRendering())
	{
		return;
	}

	SCOPED_DRAW_EVENT(RHICmdList, VolumetricLightingRenderVolume);
	SCOPED_GPU_STAT(RHICmdList, Stat_GPU_RenderVolume);

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

	FMemory::Memzero(NvVlShadowMapDesc);
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

		NvVlShadowMapDesc.eType = NvVl::ShadowMapLayout::CUBE;
		NvVlShadowMapDesc.uWidth = ShadowmapWidth;
		NvVlShadowMapDesc.uHeight = ShadowmapHeight;
		// Shadow depth type
		NvVlShadowMapDesc.bLinearizedDepth = false;
		// shadow space
		NvVlShadowMapDesc.bShadowSpace = false;

		for (int32 FaceIndex = 0; FaceIndex < 6; FaceIndex++)
		{
			NvVlShadowMapDesc.mCubeViewProj[FaceIndex] = *reinterpret_cast<const NvcMat44*>(&(ShadowInfo->OnePassShadowViewProjectionMatrices[FaceIndex]).M[0][0]);
		}

		NvVlShadowMapDesc.uElementCount = 1;
		NvVlShadowMapDesc.Elements[0].uOffsetX = 0;
		NvVlShadowMapDesc.Elements[0].uOffsetY = 0;
		NvVlShadowMapDesc.Elements[0].uWidth = NvVlShadowMapDesc.uWidth;
		NvVlShadowMapDesc.Elements[0].uHeight = NvVlShadowMapDesc.uHeight;
		NvVlShadowMapDesc.Elements[0].mArrayIndex = 0;

		ShadowDepthTextures.Add(ShadowDepthTexture);
	}
	else // Spot light
	{
		const FTexture2DRHIRef& ShadowDepthTexture = ShadowInfo->RenderTargets.DepthTarget->GetRenderTargetItem().ShaderResourceTexture->GetTexture2D();

		if (ShadowDepthTexture)
		{
			ShadowmapWidth = ShadowDepthTexture->GetSizeX();
			ShadowmapHeight = ShadowDepthTexture->GetSizeY();
		}

		FVector4 ShadowmapMinMaxValue;
		FMatrix WorldToShadowMatrixValue = ShadowInfo->GetWorldToShadowMatrix(ShadowmapMinMaxValue);

		NvVlShadowMapDesc.eType = NvVl::ShadowMapLayout::SIMPLE;
		NvVlShadowMapDesc.uWidth = ShadowmapWidth;
		NvVlShadowMapDesc.uHeight = ShadowmapHeight;
		// Shadow depth type
		NvVlShadowMapDesc.bLinearizedDepth = true;
		// shadow space
		NvVlShadowMapDesc.bShadowSpace = true;

		NvVlShadowMapDesc.uElementCount = 1;
		NvVlShadowMapDesc.Elements[0].uOffsetX = 0;
		NvVlShadowMapDesc.Elements[0].uOffsetY = 0;
		NvVlShadowMapDesc.Elements[0].uWidth = NvVlShadowMapDesc.uWidth;
		NvVlShadowMapDesc.Elements[0].uHeight = NvVlShadowMapDesc.uHeight;
		NvVlShadowMapDesc.Elements[0].mViewProj = *reinterpret_cast<const NvcMat44*>(&WorldToShadowMatrixValue.M[0][0]);
		NvVlShadowMapDesc.Elements[0].mArrayIndex = 0;
		NvVlShadowMapDesc.Elements[0].fInvMaxSubjectDepth = ShadowInfo->InvMaxSubjectDepth;
		NvVlShadowMapDesc.Elements[0].vShadowmapMinMaxValue = *reinterpret_cast<const NvcVec4 *>(&ShadowmapMinMaxValue);

		ShadowDepthTextures.Add(ShadowDepthTexture);
	}

	FMemory::Memzero(NvVlLightDesc);

	FVector Intensity = LightSceneInfo->Proxy->GetNvVlIntensity();
	NvVlLightDesc.vIntensity = *reinterpret_cast<const NvcVec3 *>(&Intensity);

	FMatrix LightViewProjInv = LightViewProj.InverseFast();
	NvVlLightDesc.mLightToWorld = *reinterpret_cast<const NvcMat44*>(&LightViewProjInv.M[0][0]);

	switch (LightSceneInfo->Proxy->GetLightType())
	{
		case LightType_Point:
		{
			NvVlLightDesc.eType = NvVl::LightType::OMNI;
			NvVlLightDesc.Omni.fZNear = ShadowInfo->MinSubjectZ;
			NvVlLightDesc.Omni.fZFar = ShadowInfo->MaxSubjectZ;
			NvVlLightDesc.Omni.vPosition = *reinterpret_cast<const NvcVec3 *>(&LightPosition);
			NvVlLightDesc.Omni.eAttenuationMode = (NvVl::AttenuationMode)LightSceneInfo->Proxy->GetNvVlAttenuationMode();
			const FVector4& AttenuationFactors = LightSceneInfo->Proxy->GetNvVlAttenuationFactors();
			NvVlLightDesc.Omni.fAttenuationFactors[0] = AttenuationFactors.X;
			NvVlLightDesc.Omni.fAttenuationFactors[1] = AttenuationFactors.Y;
			NvVlLightDesc.Omni.fAttenuationFactors[2] = AttenuationFactors.Z;
			NvVlLightDesc.Omni.fAttenuationFactors[3] = AttenuationFactors.W;
		}
		break;
		case LightType_Spot:
		default:
		{
			NvVlLightDesc.eType = NvVl::LightType::SPOTLIGHT;
			NvVlLightDesc.Spotlight.fZNear = ShadowInfo->MinSubjectZ;
			NvVlLightDesc.Spotlight.fZFar = ShadowInfo->MaxSubjectZ;

			NvVlLightDesc.Spotlight.eFalloffMode = (NvVl::SpotlightFalloffMode)LightSceneInfo->Proxy->GetNvVlFalloffMode();
			const FVector2D& AngleAndPower = LightSceneInfo->Proxy->GetNvVlFalloffAngleAndPower();
			NvVlLightDesc.Spotlight.fFalloff_CosTheta = FMath::Cos(AngleAndPower.X);
			NvVlLightDesc.Spotlight.fFalloff_Power = AngleAndPower.Y;
            
			NvVlLightDesc.Spotlight.vDirection = *reinterpret_cast<const NvcVec3 *>(&LightDirection);
			NvVlLightDesc.Spotlight.vPosition = *reinterpret_cast<const NvcVec3 *>(&LightPosition);
			NvVlLightDesc.Spotlight.eAttenuationMode = (NvVl::AttenuationMode)LightSceneInfo->Proxy->GetNvVlAttenuationMode();
			const FVector4& AttenuationFactors = LightSceneInfo->Proxy->GetNvVlAttenuationFactors();
			NvVlLightDesc.Spotlight.fAttenuationFactors[0] = AttenuationFactors.X;
			NvVlLightDesc.Spotlight.fAttenuationFactors[1] = AttenuationFactors.Y;
			NvVlLightDesc.Spotlight.fAttenuationFactors[2] = AttenuationFactors.Z;
			NvVlLightDesc.Spotlight.fAttenuationFactors[3] = AttenuationFactors.W;
		}
		break;
	}

	FMemory::Memzero(NvVlVolumeDesc);
	{
		NvVlVolumeDesc.fTargetRayResolution = LightSceneInfo->Proxy->GetNvVlTargetRayResolution();
		NvVlVolumeDesc.uMaxMeshResolution = SceneContext.GetShadowDepthTextureResolution().X;
		NvVlVolumeDesc.fDepthBias = LightSceneInfo->Proxy->GetNvVlDepthBias();
		NvVlVolumeDesc.eTessQuality = (NvVl::TessellationQuality)LightSceneInfo->Proxy->GetNvVlTessQuality();
	}

	RHICmdList.RenderVolume(ShadowDepthTextures, NvVlShadowMapDesc, NvVlLightDesc, NvVlVolumeDesc);
}

// for cascaded shadow
void FDeferredShadingSceneRenderer::NVVolumetricLightingRenderVolume(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& ShadowInfos)
{
	if (GNVVolumetricLightingRHI == nullptr || !GNVVolumetricLightingRHI->IsRendering())
	{
		return;
	}

	SCOPED_DRAW_EVENT(RHICmdList, VolumetricLightingRenderVolume);
	SCOPED_GPU_STAT(RHICmdList, Stat_GPU_RenderVolume);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	FVector LightDirection = LightSceneInfo->Proxy->GetDirection();
	LightDirection.Normalize();

	FMatrix LightViewProj;
	
	LightViewProj = FTranslationMatrix(ShadowInfos[0]->PreShadowTranslation) * ShadowInfos[0]->SubjectAndReceiverMatrix;

	FMemory::Memzero(NvVlShadowMapDesc);
	TArray<FTextureRHIParamRef> ShadowDepthTextures;
	uint32 ShadowmapWidth = 0, ShadowmapHeight = 0;

	bool bAtlassing = !GRHINeedsUnatlasedCSMDepthsWorkaround;
	const FTexture2DRHIRef& ShadowDepthTexture = ShadowInfos[0]->RenderTargets.DepthTarget->GetRenderTargetItem().ShaderResourceTexture->GetTexture2D();

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
		NvVlShadowMapDesc.uWidth = ShadowmapWidth;
		NvVlShadowMapDesc.uHeight = ShadowmapHeight;

		ShadowDepthTextures.Add(ShadowDepthTexture);
	}

	NvVlShadowMapDesc.eType = bAtlassing ? NvVl::ShadowMapLayout::CASCADE_ATLAS : NvVl::ShadowMapLayout::CASCADE_MULTI;
	// Shadow depth type
	NvVlShadowMapDesc.bLinearizedDepth = false;
	// shadow space
	NvVlShadowMapDesc.bShadowSpace = true;

	NvVlShadowMapDesc.uElementCount = FMath::Min((uint32)ShadowInfos.Num(), NvVl::MAX_SHADOWMAP_ELEMENTS);

	for (uint32 ElementIndex = 0; ElementIndex < NvVlShadowMapDesc.uElementCount; ElementIndex++)
	{
		FVector4 ShadowmapMinMaxValue;
		uint32 ShadowIndex = ShadowInfos.Num() - NvVlShadowMapDesc.uElementCount + ElementIndex;
		FMatrix WorldToShadowMatrixValue = ShadowInfos[ShadowIndex]->GetWorldToShadowMatrix(ShadowmapMinMaxValue);
		NvVlShadowMapDesc.Elements[ElementIndex].uOffsetX = 0;
		NvVlShadowMapDesc.Elements[ElementIndex].uOffsetY = 0;
		NvVlShadowMapDesc.Elements[ElementIndex].mViewProj = *reinterpret_cast<const NvcMat44*>(&WorldToShadowMatrixValue.M[0][0]);
		NvVlShadowMapDesc.Elements[ElementIndex].mArrayIndex = ElementIndex;
		NvVlShadowMapDesc.Elements[ElementIndex].vShadowmapMinMaxValue = *reinterpret_cast<const NvcVec4 *>(&ShadowmapMinMaxValue);

		if (bAtlassing)
		{
			NvVlShadowMapDesc.Elements[ElementIndex].uWidth = ShadowmapWidth;
			NvVlShadowMapDesc.Elements[ElementIndex].uHeight = ShadowmapHeight;
		}
		else
		{
			const FTexture2DRHIRef& DepthTexture = ShadowInfos[ShadowIndex]->RenderTargets.DepthTarget->GetRenderTargetItem().ShaderResourceTexture->GetTexture2D();

			NvVlShadowMapDesc.Elements[ElementIndex].uWidth = DepthTexture->GetSizeX();
			NvVlShadowMapDesc.Elements[ElementIndex].uHeight = DepthTexture->GetSizeY();
			ShadowDepthTextures.Add(DepthTexture);
		}
	}

	FMemory::Memzero(NvVlLightDesc);

	FVector Intensity = LightSceneInfo->Proxy->GetNvVlIntensity();
	NvVlLightDesc.vIntensity = *reinterpret_cast<const NvcVec3 *>(&Intensity);

	FMatrix LightViewProjInv = LightViewProj.InverseFast();
	NvVlLightDesc.mLightToWorld = *reinterpret_cast<const NvcMat44*>(&LightViewProjInv.M[0][0]);

	NvVlLightDesc.eType = NvVl::LightType::DIRECTIONAL;
	NvVlLightDesc.Directional.vDirection = *reinterpret_cast<const NvcVec3 *>(&LightDirection);

	FMemory::Memzero(NvVlVolumeDesc);
	{
		NvVlVolumeDesc.fTargetRayResolution = LightSceneInfo->Proxy->GetNvVlTargetRayResolution();
		NvVlVolumeDesc.uMaxMeshResolution = SceneContext.GetShadowDepthTextureResolution().X;
		NvVlVolumeDesc.fDepthBias = LightSceneInfo->Proxy->GetNvVlDepthBias();
		NvVlVolumeDesc.eTessQuality = (NvVl::TessellationQuality)LightSceneInfo->Proxy->GetNvVlTessQuality();
	}

	RHICmdList.RenderVolume(ShadowDepthTextures, NvVlShadowMapDesc, NvVlLightDesc, NvVlVolumeDesc);
}


void FDeferredShadingSceneRenderer::NVVolumetricLightingEndAccumulation(FRHICommandListImmediate& RHICmdList)
{
	if (GNVVolumetricLightingRHI == nullptr || !GNVVolumetricLightingRHI->IsRendering())
	{
		return;
	}

	SCOPED_DRAW_EVENT(RHICmdList, VolumetricLightingEndAccumulation);
	SCOPED_GPU_STAT(RHICmdList, Stat_GPU_EndAccumulation);
	RHICmdList.EndAccumulation();
}

void FDeferredShadingSceneRenderer::NVVolumetricLightingApplyLighting(FRHICommandListImmediate& RHICmdList)
{
	if (GNVVolumetricLightingRHI == nullptr || !GNVVolumetricLightingRHI->IsRendering())
	{
		return;
	}

	check(Views.Num());
	const FViewInfo& View = Views[0];

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	const FNVVolumetricLightingProperties& Properties = Scene->VolumetricLightingProperties;
	const FFinalPostProcessSettings& FinalPostProcessSettings = View.FinalPostProcessSettings;

	FMemory::Memzero(NvVlPostprocessDesc);
	NvVlPostprocessDesc.bDoFog = (FinalPostProcessSettings.FogMode != EFogMode::FOG_NONE) && CVarNvVlFog.GetValueOnRenderThread();
    NvVlPostprocessDesc.bIgnoreSkyFog = FinalPostProcessSettings.FogMode == EFogMode::FOG_NOSKY;
    NvVlPostprocessDesc.eUpsampleQuality = (NvVl::UpsampleQuality)Properties.UpsampleQuality.GetValue();
    NvVlPostprocessDesc.fBlendfactor = Properties.Blendfactor;
    NvVlPostprocessDesc.fTemporalFactor = Properties.TemporalFactor;
    NvVlPostprocessDesc.fFilterThreshold = Properties.FilterThreshold;
	FMatrix ViewProjNoAAMatrix = View.ViewMatrices.GetViewMatrix() * View.ViewMatrices.ComputeProjectionNoAAMatrix();
	NvVlPostprocessDesc.mUnjitteredViewProj = *reinterpret_cast<const NvcMat44*>(&ViewProjNoAAMatrix.M[0][0]);

	FVector FogLight = FinalPostProcessSettings.FogColor * FinalPostProcessSettings.FogIntensity;
    NvVlPostprocessDesc.vFogLight = *reinterpret_cast<const NvcVec3 *>(&FogLight);
    NvVlPostprocessDesc.fMultiscatter = FinalPostProcessSettings.MultiScatter;
	NvVlPostprocessDesc.eStereoPass = NvVl::StereoscopicPass::FULL;

	if (!SceneContext.IsSeparateTranslucencyActive(Views[0]))
	{
		SCOPED_DRAW_EVENT(RHICmdList, VolumetricLightingApplyLighting);
		SCOPED_GPU_STAT(RHICmdList, Stat_GPU_ApplyLighting);

		RHICmdList.ApplyLighting(SceneContext.GetSceneColorSurface(), NvVlPostprocessDesc);
	}
	else
	{
		GNVVolumetricLightingRHI->SetSeparateTranslucencyPostprocessDesc(NvVlPostprocessDesc);
	}
}

#endif