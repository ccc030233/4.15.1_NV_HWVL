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
	void ReleaseContext();

	void UpdateFrameBuffer(int32 InBufferSizeX, int32 InBufferSizeY, uint16 InNumSamples);
	void UpdateDownsampleMode(uint32 InMode);
	void UpdateMsaaMode(uint32 InMode);
	void UpdateFilterMode(uint32 InMode);
	void UpdateRendering(bool Enabled);

	// SeparateTranslucency
	void SetSeparateTranslucencyPostprocessDesc(const NvVl::PostprocessDesc& InPostprocessDesc);
	const NvVl::PostprocessDesc* GetSeparateTranslucencyPostprocessDesc();

	// 
	bool IsMSAAEnabled() const { return ContextDesc.eInternalSampleMode == NvVl::MultisampleMode::MSAA2 || ContextDesc.eInternalSampleMode == NvVl::MultisampleMode::MSAA4; }
	bool IsTemporalFilterEnabled() const { return ContextDesc.eFilterMode == NvVl::FilterMode::TEMPORAL; }
	bool IsRendering() const { return bEnableRendering; }
	
	void BeginAccumulation(FTextureRHIParamRef SceneDepthTextureRHI, const NvVl::ViewerDesc& ViewerDesc, const NvVl::MediumDesc& MediumDesc, NvVl::DebugFlags DebugFlags);
	void RenderVolume(const TArray<FTextureRHIParamRef>& ShadowMapTextures, const NvVl::ShadowMapDesc& ShadowMapDesc, const NvVl::LightDesc& LightDesc, const NvVl::VolumeDesc& VolumeDesc);
	void EndAccumulation();
	void ApplyLighting(FTextureRHIParamRef SceneColorSurfaceRHI, const NvVl::PostprocessDesc& PostprocessDesc);
private:

	HMODULE ModuleHandle;
	bool bNeedUpdateContext;
	bool bEnableRendering;
	bool bEnableSeparateTranslucency;

	NvVl::ContextDesc		ContextDesc;
	NvVl::PlatformDesc		PlatformDesc;
	NvVl::Context			Context;
	NvVl::PostprocessDesc	SeparateTranslucencyPostprocessDesc;

	NvVl::PlatformRenderCtx	RenderCtx;

	NvVl::PlatformShaderResource SceneDepthSRV;
};

/** A global pointer to Nvidia Volumetric Lighting RHI implementation. */
extern RHI_API FNVVolumetricLightingRHI* GNVVolumetricLightingRHI;

extern RHI_API FNVVolumetricLightingRHI* CreateNVVolumetricLightingRHI();

#endif