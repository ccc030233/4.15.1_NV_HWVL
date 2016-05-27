
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


FVector GetLightIntensity(uint8 LightType, uint8 LightPower)
{
   const FVector LIGHT_POWER[] = {
        1.00f*FVector(1.00f, 0.95f, 0.90f),
        0.50f*FVector(1.00f, 0.95f, 0.90f),
        1.50f*FVector(1.00f, 0.95f, 0.90f),
        1.00f*FVector(1.00f, 0.75f, 0.50f),
        1.00f*FVector(0.75f, 1.00f, 0.75f),
        1.00f*FVector(0.50f, 0.75f, 1.00f)
    };
	switch(LightType)
	{
		case LightType_Point:
			return 25000.0f * LIGHT_POWER[LightPower];
			break;
		case LightType_Spot:
			return 50000.0f * LIGHT_POWER[LightPower];
			break;
		default: //LightType_Directional
			return 250.0f * LIGHT_POWER[LightPower];
			break;
	}
}

void FDeferredShadingSceneRenderer::NVVolumetricLightingBeginAccumulation(FRHICommandListImmediate& RHICmdList)
{
	if (!CVarNvVlEnable.GetValueOnRenderThread())
	{
		return;
	}

	check(Views.Num());
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	const FViewInfo& View = Views[0];

	int32 MediumType = 0; //TODO;

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
    MediumDesc.uNumPhaseTerms = 2;

	{
		MediumDesc.PhaseTerms[0].ePhaseFunc = NvVl::PhaseFunctionType::RAYLEIGH;
		FVector Density = 10.00f * SCATTER_PARAM_SCALE * FVector(0.596f, 1.324f, 3.310f);
		MediumDesc.PhaseTerms[0].vDensity = *reinterpret_cast<const NvcVec3 *>(&Density);
	}
	
    switch (MediumType)
    {
    default:
    case 0:
		{
			FVector Density = 10.00f * SCATTER_PARAM_SCALE * FVector(1.00f, 1.00f, 1.00f);
			FVector Absorption = 5.0f * SCATTER_PARAM_SCALE * FVector(1.00f, 1.00f, 1.00f);

			MediumDesc.PhaseTerms[1].ePhaseFunc = NvVl::PhaseFunctionType::HENYEYGREENSTEIN;
			MediumDesc.PhaseTerms[1].vDensity = *reinterpret_cast<const NvcVec3 *>(&Density);
			MediumDesc.PhaseTerms[1].fEccentricity = 0.85f;
			MediumDesc.vAbsorption = *reinterpret_cast<const NvcVec3 *>(&Absorption);
		}
		break;

    case 1:
		{
			FVector Density = 15.00f * SCATTER_PARAM_SCALE * FVector(1.00f, 1.00f, 1.00f);
			FVector Absorption = 25.0f * SCATTER_PARAM_SCALE * FVector(1.00f, 1.00f, 1.00f);

			MediumDesc.PhaseTerms[1].ePhaseFunc = NvVl::PhaseFunctionType::HENYEYGREENSTEIN;
			MediumDesc.PhaseTerms[1].vDensity = *reinterpret_cast<const NvcVec3 *>(&Density);
			MediumDesc.PhaseTerms[1].fEccentricity = 0.60f;
			MediumDesc.vAbsorption = *reinterpret_cast<const NvcVec3 *>(&Absorption);
		}
		break;

    case 2:
		{
			FVector Density = 20.00f * SCATTER_PARAM_SCALE * FVector(1.00f, 1.00f, 1.00f);
			FVector Absorption = 25.0f * SCATTER_PARAM_SCALE * FVector(1.00f, 1.00f, 1.00f);

			MediumDesc.PhaseTerms[1].ePhaseFunc = NvVl::PhaseFunctionType::MIE_HAZY;
			MediumDesc.PhaseTerms[1].vDensity = *reinterpret_cast<const NvcVec3 *>(&Density);
			MediumDesc.vAbsorption = *reinterpret_cast<const NvcVec3 *>(&Absorption);
		}
		break;

    case 3:
		{
			FVector Density = 30.00f * SCATTER_PARAM_SCALE * FVector(1.00f, 1.00f, 1.00f);
			FVector Absorption = 50.0f * SCATTER_PARAM_SCALE * FVector(1.00f, 1.00f, 1.00f);

			MediumDesc.PhaseTerms[1].ePhaseFunc = NvVl::PhaseFunctionType::MIE_MURKY;
			MediumDesc.PhaseTerms[1].vDensity = *reinterpret_cast<const NvcVec3 *>(&Density);
			MediumDesc.vAbsorption = *reinterpret_cast<const NvcVec3 *>(&Absorption);
		}
		break;
    }

	int32 DebugMode = FMath::Clamp((int32)CVarNvVlDebugMode.GetValueOnRenderThread(), 0, 2);

	GNVVolumetricLightingRHI->BeginAccumulation(SceneContext.GetSceneDepthTexture(), ViewerDesc, MediumDesc, (Nv::VolumetricLighting::DebugFlags)DebugMode); //SceneContext.GetActualDepthTexture()?
}

void FDeferredShadingSceneRenderer::NVVolumetricLightingRenderVolume(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, const FProjectedShadowInfo* ShadowInfo)
{
	if (!CVarNvVlEnable.GetValueOnRenderThread())
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

	//FViewInfo* FoundView = ShadowInfo->FindViewForShadow(this);
	//FMatrix ShadowViewProj = FTranslationMatrix(ShadowInfo->PreShadowTranslation - (FoundView ? FoundView->ViewMatrices.PreViewTranslation : FVector::ZeroVector)) * ShadowInfo->SubjectAndReceiverMatrix;
	FMatrix LightViewProj = FTranslationMatrix(ShadowInfo->PreShadowTranslation) * ShadowInfo->SubjectAndReceiverMatrix;

    NvVl::ShadowMapDesc ShadowmapDesc;
    {
        ShadowmapDesc.eType = (LightSceneInfo->Proxy->GetLightType() == LightType_Point) ? NvVl::ShadowMapLayout::PARABOLOID : NvVl::ShadowMapLayout::SIMPLE;
        ShadowmapDesc.uWidth = ShadowmapWidth;
        ShadowmapDesc.uHeight = ShadowmapHeight;
        ShadowmapDesc.uElementCount = 1;
        ShadowmapDesc.Elements[0].uOffsetX = 0;
        ShadowmapDesc.Elements[0].uOffsetY = 0;
        ShadowmapDesc.Elements[0].uWidth = ShadowmapDesc.uWidth;
        ShadowmapDesc.Elements[0].uHeight = ShadowmapDesc.uHeight;
        ShadowmapDesc.Elements[0].mViewProj = *reinterpret_cast<const NvcMat44*>(&LightViewProj.M[0][0]);
        ShadowmapDesc.Elements[0].mArrayIndex = 0;
        if (LightSceneInfo->Proxy->GetLightType() == LightType_Point)
        {
            ShadowmapDesc.uElementCount = 2;
            ShadowmapDesc.Elements[1].uOffsetX = 0;
            ShadowmapDesc.Elements[1].uOffsetY = 0;
            ShadowmapDesc.Elements[1].uWidth = ShadowmapDesc.uWidth;
            ShadowmapDesc.Elements[1].uHeight = ShadowmapDesc.uHeight;
            ShadowmapDesc.Elements[1].mViewProj = *reinterpret_cast<const NvcMat44*>(&LightViewProj.M[0][0]);
            ShadowmapDesc.Elements[1].mArrayIndex = 1;
        }
    }

    NvVl::LightDesc LightDesc;
	const float SPOTLIGHT_FALLOFF_ANGLE = PI / 4.0f;
	const float SPOTLIGHT_FALLOFF_POWER = 1.0f;

    FVector LightPosition = LightSceneInfo->Proxy->GetOrigin();
    FVector LightDirection = LightSceneInfo->Proxy->GetDirection();
    LightDirection.Normalize();

	FMatrix LightViewProjInv = LightViewProj.InverseFast();
	uint32 LightPower = 0;

	FVector Intensity = GetLightIntensity(LightSceneInfo->Proxy->GetLightType(), LightPower);
    LightDesc.vIntensity = *reinterpret_cast<const NvcVec3 *>(&Intensity);
    LightDesc.mLightToWorld = *reinterpret_cast<const NvcMat44*>(&LightViewProjInv.M[0][0]);

    switch (LightSceneInfo->Proxy->GetLightType())
    {
        case LightType_Point:
        {
            LightDesc.eType = NvVl::LightType::OMNI;
            LightDesc.Omni.fZNear = ShadowInfo->MinSubjectZ;
            LightDesc.Omni.fZFar = ShadowInfo->MaxSubjectZ;
            LightDesc.Omni.vPosition = *reinterpret_cast<const NvcVec3 *>(&LightPosition);
            LightDesc.Omni.eAttenuationMode = NvVl::AttenuationMode::INV_POLYNOMIAL;
            const float LIGHT_SOURCE_RADIUS = 0.5f; // virtual radius of a spheroid light source
            LightDesc.Omni.fAttenuationFactors[0] = 1.0f;
            LightDesc.Omni.fAttenuationFactors[1] = 2.0f / LIGHT_SOURCE_RADIUS;
            LightDesc.Omni.fAttenuationFactors[2] = 1.0f / (LIGHT_SOURCE_RADIUS*LIGHT_SOURCE_RADIUS);
            LightDesc.Omni.fAttenuationFactors[3] = 0.0f;
        }
        break;
        case LightType_Spot:
        {
            LightDesc.eType = NvVl::LightType::SPOTLIGHT;
            LightDesc.Spotlight.fZNear = ShadowInfo->MinSubjectZ;
            LightDesc.Spotlight.fZFar = ShadowInfo->MaxSubjectZ;
            LightDesc.Spotlight.eFalloffMode = NvVl::SpotlightFalloffMode::FIXED;
            LightDesc.Spotlight.fFalloff_Power = SPOTLIGHT_FALLOFF_POWER;
            LightDesc.Spotlight.fFalloff_CosTheta = FMath::Cos(SPOTLIGHT_FALLOFF_ANGLE);
            LightDesc.Spotlight.vDirection = *reinterpret_cast<const NvcVec3 *>(&LightDirection);
            LightDesc.Spotlight.vPosition = *reinterpret_cast<const NvcVec3 *>(&LightPosition);
            LightDesc.Spotlight.eAttenuationMode = NvVl::AttenuationMode::INV_POLYNOMIAL;
            const float LIGHT_SOURCE_RADIUS = 1.0f;  // virtual radius of a spheroid light source
            LightDesc.Spotlight.fAttenuationFactors[0] = 1.0f;
            LightDesc.Spotlight.fAttenuationFactors[1] = 2.0f / LIGHT_SOURCE_RADIUS;
            LightDesc.Spotlight.fAttenuationFactors[2] = 1.0f / (LIGHT_SOURCE_RADIUS*LIGHT_SOURCE_RADIUS);
            LightDesc.Spotlight.fAttenuationFactors[3] = 0.0f;
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
        VolumeDesc.fTargetRayResolution = 12.0f;
        VolumeDesc.uMaxMeshResolution = ShadowmapDesc.uWidth;
        VolumeDesc.fDepthBias = 0.0f;
        VolumeDesc.eTessQuality = NvVl::TessellationQuality::HIGH;
    }

	GNVVolumetricLightingRHI->RenderVolume(ShadowDepth, ShadowmapDesc, LightDesc, VolumeDesc);
}

void FDeferredShadingSceneRenderer::NVVolumetricLightingEndAccumulation(FRHICommandListImmediate& RHICmdList)
{
	if (!CVarNvVlEnable.GetValueOnRenderThread())
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
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	NvVl::PostprocessDesc PostprocessDesc;
	PostprocessDesc.bDoFog = true;
    PostprocessDesc.bIgnoreSkyFog = false;
    PostprocessDesc.eUpsampleQuality = NvVl::UpsampleQuality::BILINEAR;
    PostprocessDesc.fBlendfactor = 1.0f;
    PostprocessDesc.fTemporalFactor = 0.95f;
    PostprocessDesc.fFilterThreshold = 0.20f;

	FVector FogLight = FVector(50000.0f, 50000.0f, 50000.0f);
    PostprocessDesc.vFogLight = *reinterpret_cast<const NvcVec3 *>(&FogLight);
    PostprocessDesc.fMultiscatter = 0.000002f;

	GNVVolumetricLightingRHI->ApplyLighting(SceneContext.GetSceneColorSurface(), PostprocessDesc);

	// clear the state cache
	GDynamicRHI->ClearStateCache();
	SetRenderTarget(RHICmdList, FTextureRHIParamRef(), FTextureRHIParamRef());
}

#endif