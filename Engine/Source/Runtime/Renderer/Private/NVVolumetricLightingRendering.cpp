
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

void FDeferredShadingSceneRenderer::NVVolumetricLightingBeginAccumulation(FRHICommandListImmediate& RHICmdList)
{
	if (!CVarNvVlEnable.GetValueOnRenderThread())
	{
		return;
	}

	if (!Scene->bEnableVolumetricLightingSettings)
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
    ViewerDesc.uViewportWidth = View.ViewRect.Width();
    ViewerDesc.uViewportHeight = View.ViewRect.Height();
	ViewerDesc.bReversedZ = ((int32)ERHIZBuffer::IsInverted != 0);
	
	NvVl::MediumDesc MediumDesc;
    const float SCATTER_PARAM_SCALE = 0.0001f;
    
	FNVVolumetricLightingMediumSettings& MediumSettings = Scene->MediumSettings;
	check(MediumSettings.PhaseTerms.Num() > 0);

	uint32 NumPhaseTerms = MediumSettings.PhaseTerms.Num();
	MediumDesc.uNumPhaseTerms = NumPhaseTerms > NvVl::MAX_PHASE_TERMS ? NvVl::MAX_PHASE_TERMS : NumPhaseTerms;

	FVector Absorption = MediumSettings.Absorption * SCATTER_PARAM_SCALE;
	MediumDesc.vAbsorption = *reinterpret_cast<const NvcVec3 *>(&Absorption);

	for(uint32 PhaseIndex = 0; PhaseIndex < MediumDesc.uNumPhaseTerms; PhaseIndex++)
	{
		MediumDesc.PhaseTerms[PhaseIndex].ePhaseFunc = (NvVl::PhaseFunctionType)MediumSettings.PhaseTerms[PhaseIndex].PhaseFunc.GetValue();

		FVector Density = MediumSettings.PhaseTerms[PhaseIndex].Density * SCATTER_PARAM_SCALE;
		MediumDesc.PhaseTerms[PhaseIndex].vDensity = *reinterpret_cast<const NvcVec3 *>(&Density);

		MediumDesc.PhaseTerms[PhaseIndex].fEccentricity = MediumSettings.PhaseTerms[PhaseIndex].Eccentricity;
	}

	int32 DebugMode = FMath::Clamp((int32)CVarNvVlDebugMode.GetValueOnRenderThread(), 0, 2);

	GNVVolumetricLightingRHI->BeginAccumulation(SceneContext.GetSceneDepthTexture(), ViewerDesc, MediumDesc, (Nv::VolumetricLighting::DebugFlags)DebugMode); //SceneContext.GetActualDepthTexture()?
}

void FDeferredShadingSceneRenderer::NVVolumetricLightingRenderVolume(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, const FProjectedShadowInfo* ShadowInfo)
{
	// TODO
	// Point light need to be supported by the cubemap shadowmap
	if (LightSceneInfo->Proxy->GetLightType() == LightType_Point)
	{
		return;
	}

	if (!CVarNvVlEnable.GetValueOnRenderThread() || !LightSceneInfo->Proxy->IsNVVolumetricLighting())
	{
		return;
	}

	if (!Scene->bEnableVolumetricLightingSettings)
	{
		return;
	}

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
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

	FMatrix LightViewProj = FTranslationMatrix(ShadowInfo->PreShadowTranslation) * ShadowInfo->SubjectAndReceiverMatrix;

	FVector4 ShadowmapMinMaxValue;
	FMatrix WorldToShadowMatrixValue = ShadowInfo->GetWorldToShadowMatrix(ShadowmapMinMaxValue);

    NvVl::ShadowMapDesc ShadowmapDesc;
    {
        ShadowmapDesc.eType = (LightSceneInfo->Proxy->GetLightType() == LightType_Point) ? NvVl::ShadowMapLayout::PARABOLOID : NvVl::ShadowMapLayout::SIMPLE;
        ShadowmapDesc.uWidth = ShadowmapWidth;
        ShadowmapDesc.uHeight = ShadowmapHeight;
		// Shadow depth type
		ShadowmapDesc.bLinearizedDepth = LightSceneInfo->Proxy->GetLightType() == LightType_Directional ? false : true;
		ShadowmapDesc.fInvMaxSubjectDepth = ShadowInfo->InvMaxSubjectDepth;
		// shadow space
		ShadowmapDesc.bShadowSpace = true;
		ShadowmapDesc.vShadowmapMinMaxValue = *reinterpret_cast<const NvcVec4 *>(&ShadowmapMinMaxValue);

        ShadowmapDesc.uElementCount = 1;
		ShadowmapDesc.Elements[0].uOffsetX = 0;
        ShadowmapDesc.Elements[0].uOffsetY = 0;
        ShadowmapDesc.Elements[0].uWidth = ShadowmapDesc.uWidth;
        ShadowmapDesc.Elements[0].uHeight = ShadowmapDesc.uHeight;
        ShadowmapDesc.Elements[0].mViewProj = *reinterpret_cast<const NvcMat44*>(&WorldToShadowMatrixValue.M[0][0]);
        ShadowmapDesc.Elements[0].mArrayIndex = 0;
        if (LightSceneInfo->Proxy->GetLightType() == LightType_Point)
        {
            ShadowmapDesc.uElementCount = 2;
            ShadowmapDesc.Elements[1].uOffsetX = 0;
            ShadowmapDesc.Elements[1].uOffsetY = 0;
            ShadowmapDesc.Elements[1].uWidth = ShadowmapDesc.uWidth;
            ShadowmapDesc.Elements[1].uHeight = ShadowmapDesc.uHeight;
            ShadowmapDesc.Elements[1].mViewProj = *reinterpret_cast<const NvcMat44*>(&WorldToShadowMatrixValue.M[0][0]);
            ShadowmapDesc.Elements[1].mArrayIndex = 1;
        }
    }

    NvVl::LightDesc LightDesc;

    FVector LightPosition = LightSceneInfo->Proxy->GetOrigin();
    FVector LightDirection = LightSceneInfo->Proxy->GetDirection();
    LightDirection.Normalize();

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

	if (!Scene->bEnableVolumetricLightingSettings)
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

	if (!Scene->bEnableVolumetricLightingSettings)
	{
		return;
	}

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	FNVVolumetricLightingPostprocessSettings& PostprocessSettings = Scene->PostprocessSettings;

	NvVl::PostprocessDesc PostprocessDesc;
	PostprocessDesc.bDoFog = PostprocessSettings.bEnableFog;
    PostprocessDesc.bIgnoreSkyFog = PostprocessSettings.bIgnoreSkyFog;
    PostprocessDesc.eUpsampleQuality = (NvVl::UpsampleQuality)PostprocessSettings.UpsampleQuality.GetValue();
    PostprocessDesc.fBlendfactor = PostprocessSettings.Blendfactor;
    PostprocessDesc.fTemporalFactor = PostprocessSettings.TemporalFactor;
    PostprocessDesc.fFilterThreshold = PostprocessSettings.FilterThreshold;

	FVector FogLight = FLinearColor(PostprocessSettings.FogColor) * PostprocessSettings.FogIntensity;
    PostprocessDesc.vFogLight = *reinterpret_cast<const NvcVec3 *>(&FogLight);
    PostprocessDesc.fMultiscatter = PostprocessSettings.Multiscatter;

	GNVVolumetricLightingRHI->ApplyLighting(SceneContext.GetSceneColorSurface(), PostprocessDesc);

	// clear the state cache
	GDynamicRHI->ClearStateCache();
	SetRenderTarget(RHICmdList, FTextureRHIParamRef(), FTextureRHIParamRef());
}

#endif