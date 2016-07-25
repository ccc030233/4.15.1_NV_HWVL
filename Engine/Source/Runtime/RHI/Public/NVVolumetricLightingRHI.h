/*=============================================================================
	NVVolumetricLightingRHI.h: Nvidia Volumetric Lighting Render Hardware Interface definitions.
=============================================================================*/

#pragma once

#if WITH_NVVOLUMETRICLIGHTING

/** The interface which is implemented by Nvidia Volumetric Lighting RHI. */
class RHI_API FNVVolumetricLightingRHI
{
public:
	FNVVolumetricLightingRHI();

	/** Declare a virtual destructor. */
	~FNVVolumetricLightingRHI() {}

	/** Initializes the RHI;*/
	void Init();

	/** Shutdown the RHI; */
	void Shutdown();

	void UpdateContext();

	void UpdateFrameBuffer(int32 InBufferSizeX, int32 InBufferSizeY, uint16 InNumSamples);
	void UpdateCascadedShadow(int32 InBufferSizeX, int32 InBufferSizeY, uint32 InSlices);
	void BeginAccumulation(FTextureRHIParamRef SceneDepthTextureRHI, const NvVl::ViewerDesc& ViewerDesc, const NvVl::MediumDesc& MediumDesc, NvVl::DebugFlags DebugFlags);
	void RemapShadowDepth(FTextureRHIParamRef ShadowMapTextureRHI);
	void RenderVolume(FTextureRHIParamRef ShadowMapTextureRHI, const NvVl::ShadowMapDesc& ShadowMapDesc, const NvVl::LightDesc& LightDesc, const NvVl::VolumeDesc& VolumeDesc);
	void EndAccumulation();
	void ApplyLighting(FTextureRHIParamRef SceneColorSurfaceRHI, const NvVl::PostprocessDesc InPostprocessDesc);
	void SetSeparateTranslucencyPostprocess(bool bEnable, const NvVl::PostprocessDesc InPostprocessDesc);
	bool SeparateTranslucencyApplyLighting(FTextureRHIParamRef SceneColorSurfaceRHI);
private:
	void UpdateShadowBuffer();

	HMODULE ModuleHandle;
	bool bNeedUpdateContext;
	bool bEnableSeparateTranslucencyPostprocess;

	NvVl::ContextDesc		ContextDesc;
	NvVl::PlatformDesc		PlatformDesc;
	NvVl::Context			Context;
	NvVl::PostprocessDesc	SeparateTranslucencyPostprocessDesc;

	NvVl::PlatformRenderCtx	RenderCtx;

	NvVl::PlatformShaderResource SceneDepthSRV;

	uint32 MaxShadowBufferWidthPerFrame;
	uint32 MaxShadowBufferHeightPerFrame;
	uint32 MaxShadowBufferSlicesPerFrame;
};

/** A global pointer to Nvidia Volumetric Lighting RHI implementation. */
extern RHI_API FNVVolumetricLightingRHI* GNVVolumetricLightingRHI;

extern RHI_API FNVVolumetricLightingRHI* CreateNVVolumetricLightingRHI();

#endif