
/*=============================================================================
	NVVolumetricLightingRendering.cpp: Nvidia Volumetric Lighting rendering implementation.
=============================================================================*/

#include "RendererPrivate.h"

#if WITH_NVVOLUMETRICLIGHTING

#include "NVVolumetricLightingRHI.h"

void FDeferredShadingSceneRenderer::NVVolumetricLightingBeginAccumulation(FRHICommandListImmediate& RHICmdList)
{
	check(Views.Num());
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	const FViewInfo& View = Views[0];

	//GFSDK_GodraysLib_ViewerDesc ViewerDesc;
	//{
	//	ViewerDesc.uWidth = View.ViewRect.Width();
	//	ViewerDesc.uHeight = View.ViewRect.Height();
	//	FMatrix ProjMatrix = View.ViewMatrices.ProjMatrix;
	//	ViewerDesc.mProj = *reinterpret_cast<gfsdk_float4x4*>(&ProjMatrix.M[0][0]);
	//	FMatrix ViewProjMatrix = View.ViewMatrices.GetViewProjMatrix();
	//	ViewerDesc.mViewProj = *reinterpret_cast<gfsdk_float4x4*>(&ViewProjMatrix.M[0][0]);
	//	ViewerDesc.vViewPos = *reinterpret_cast<const gfsdk_float3*>(&View.ViewLocation);
	//}

	//GFSDK_GodraysLib_MediumDesc MediumDesc;
	//{
	//	const float GODRAY_PARAM_SCALE_FACTOR = 0.0001f;

	//	FVector AirScatter = FVector(0.60f, 1.52f, 3.31f) * GODRAY_PARAM_SCALE_FACTOR;
	//	FVector FwdScatter = FVector(2.00f, 2.00f, 2.00f) * GODRAY_PARAM_SCALE_FACTOR;
	//	FVector BackScatter = FVector(1.00f, 1.00f, 1.00f) * GODRAY_PARAM_SCALE_FACTOR;
	//	FVector Absorption = FVector::ZeroVector * GODRAY_PARAM_SCALE_FACTOR;
	//	float FwdScatterPhase = 0.75f;
	//	float BackScatterPhase = 0.0f;

	//	MediumDesc.vAirScatter = *reinterpret_cast<const gfsdk_float3*>(&AirScatter);				// Rayleigh scattering terms
	//	MediumDesc.vFwdScatter = *reinterpret_cast<const gfsdk_float3*>(&FwdScatter);				// Mie scattering terms
	//	MediumDesc.vBackScatter = *reinterpret_cast<const gfsdk_float3*>(&BackScatter);				// Mie scattering terms
	//	MediumDesc.fFwdScatterPhase = FwdScatterPhase;				// Mie scattering phase
	//	MediumDesc.fBackScatterPhase = BackScatterPhase;			// Mie scattering phase
	//	MediumDesc.vAbsorption = *reinterpret_cast<const gfsdk_float3*>(&Absorption);				// Absorption term
	//}
	// BREAK
	//RHICmdList.BeginAccumulation(ViewerDesc, MediumDesc, SceneContext.GetSceneColorTexture(), SceneContext.GetSceneDepthTexture()); //SceneContext.GetActualDepthTexture()?
}

void FDeferredShadingSceneRenderer::NVVolumetricLightingRenderVolume(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, const FProjectedShadowInfo* ShadowInfo)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	const FTexture2DRHIRef& ShadowDepth = SceneContext.GetShadowDepthZTexture(false);

	//GFSDK_GodraysLib_ShadowMapDesc ShadowMapDesc;
	//{
	//	uint32 Width, Height;
	//	if (ShadowDepth)
	//	{
	//		Width = ShadowDepth->GetSizeX();
	//		Height = ShadowDepth->GetSizeY();
	//	}
	//	else
	//	{
	//		FIntPoint Resolution = SceneContext.GetShadowDepthTextureResolution(); // Pre Cache?? GetPreShadowCacheTextureResolution()
	//		Width = Resolution.X;
	//		Height = Resolution.Y;
	//	}

	//	ShadowMapDesc.eType = GFSDK_GodraysLib_ShadowMapType_Simple;	// Shadow map structure type
	//	ShadowMapDesc.uWidth = Width;						// Shadow map texture width
	//	ShadowMapDesc.uHeight = Height;						// Shadow map texture height
	//	ShadowMapDesc.Cascades[0].uWidth = Width;
	//	ShadowMapDesc.Cascades[0].uHeight = Height;
	//	ShadowMapDesc.Cascades[0].uOffsetX = 0;
	//	ShadowMapDesc.Cascades[0].uOffsetY = 0;
	//	ShadowMapDesc.Cascades[0].mArrayIndex = 0;
	//	//ShadowMapDesc.Cascades[0].mViewProj = *reinterpret_cast<gfsdk_float4x4*>(&);
	//}
	//
	//GFSDK_GodraysLib_LightDesc LightDesc;
	//{
	//	float Intensity = 1.0f;
	//	FLinearColor LightColor = LightSceneInfo->Proxy->GetColor() * Intensity;

	//	FMatrix LightToWorld = LightSceneInfo->Proxy->GetLightToWorld();
	//	FVector Direction = LightToWorld.GetUnitAxis(EAxis::X);

	//	LightDesc.eType = GFSDK_GodraysLib_LightType_Directional;
	//	//LightDesc.mLightToWorld = *reinterpret_cast<gfsdk_float4x4*>(&);
	//	LightDesc.vIntensity = *reinterpret_cast<const gfsdk_float3*>(&LightColor);

	//	// Directional
	//	LightDesc.Directional.vDirection = *reinterpret_cast<const gfsdk_float3*>(&Direction);
	//	LightDesc.Directional.fLightToEyeDepth = 5000.0f;
	//}

	//uint32 GridResolution = 64;

	// BREAK
	//RHICmdList.RenderVolume(ShadowMapDesc, LightDesc, GridResolution, ShadowDepth);
}

void FDeferredShadingSceneRenderer::NVVolumetricLightingEndAccumulation(FRHICommandListImmediate& RHICmdList)
{
	//TODO
	// GNVVolumetricLightingRHI->EndAccumulation();
}

void FDeferredShadingSceneRenderer::NVVolumetricLightingApplyLighting(FRHICommandListImmediate& RHICmdList)
{
	check(Views.Num());
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	const FViewInfo& View = Views[0];

	//GFSDK_GodraysLib_PostProcessDesc PostProcessDesc;
	//{
	//	bool bDoFog = false;
	//	FVector FogLight = FVector::ZeroVector;
	//	float HazeIntensity = 1.0f;
	//	float MultiScatter = 0.01f;

	//	const float TEMPORAL_FACTOR = 0.98f;
	//	const float TEMPORAL_FILTER_THRESHOLD = 0.01f;

	//	PostProcessDesc.bDoFog = bDoFog;						// Apply fogging based on scattering
	//	PostProcessDesc.vFogLight = *reinterpret_cast<const gfsdk_float3*>(&FogLight);					// Light intensity to use for multi-scattering

	//	PostProcessDesc.vHazeIntensity = HazeIntensity;				// Multiplier for overall godray effect
	//	PostProcessDesc.bIgnoreSkyFog = true;				// Ignore depth values of (1.0f) for fogging
	//	PostProcessDesc.fMultiScatter = MultiScatter;				// Multi-scattering fraction (for fogging)	
	//	PostProcessDesc.fTemporalFactor = TEMPORAL_FACTOR;				// Weight of pixel history smoothing (0.0 for off)
	//	PostProcessDesc.fFilterThreshold = TEMPORAL_FILTER_THRESHOLD;				// Threshold of frame movement to use temporal history

	//	FMatrix ViewProjMatrix = View.ViewMatrices.GetViewProjMatrix();
	//	PostProcessDesc.mUnjitteredViewProj = *reinterpret_cast<gfsdk_float4x4*>(&ViewProjMatrix.M[0][0]);		// Camera view projection without jitter
	//}

	// BREAK
	//RHICmdList.ApplyLighting(PostProcessDesc, SceneContext.GetSceneColorTexture(), SceneContext.GetSceneDepthTexture());
}

#endif