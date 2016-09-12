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
	void UpdateDownsampleMode(uint32 InMode);
	void UpdateMsaaMode(uint32 InMode);
	void UpdateFilterMode(uint32 InMode);
	
	void BeginAccumulation(FTextureRHIParamRef SceneDepthTextureRHI, const NvVl::ViewerDesc& ViewerDesc, const NvVl::MediumDesc& MediumDesc, NvVl::DebugFlags DebugFlags);
	void RemapShadowDepth(FTextureRHIParamRef ShadowMapTextureRHI);
	void RenderVolume(FTextureRHIParamRef ShadowMapTextureRHI, const NvVl::ShadowMapDesc& ShadowMapDesc, const NvVl::LightDesc& LightDesc, const NvVl::VolumeDesc& VolumeDesc);
	void EndAccumulation();
	void ApplyLighting(FTextureRHIParamRef SceneColorSurfaceRHI, const NvVl::PostprocessDesc& PostprocessDesc);
private:

	HMODULE ModuleHandle;
	bool bNeedUpdateContext;

	NvVl::ContextDesc		ContextDesc;
	NvVl::PlatformDesc		PlatformDesc;
	NvVl::Context			Context;

	NvVl::PlatformRenderCtx	RenderCtx;

	NvVl::PlatformShaderResource SceneDepthSRV;

	uint32 MaxShadowBufferWidth;
	uint32 MaxShadowBufferHeight;
	uint32 MaxShadowBufferSlices;
};

/** A global pointer to Nvidia Volumetric Lighting RHI implementation. */
extern RHI_API FNVVolumetricLightingRHI* GNVVolumetricLightingRHI;

extern RHI_API FNVVolumetricLightingRHI* CreateNVVolumetricLightingRHI();

#endif