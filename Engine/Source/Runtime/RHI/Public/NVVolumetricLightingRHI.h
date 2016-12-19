/*=============================================================================
	NVVolumetricLightingRHI.h: Nvidia Volumetric Lighting Render Hardware Interface definitions.
=============================================================================*/

#pragma once

#if WITH_NVVOLUMETRICLIGHTING

DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("VolumetricLighting BeginAccumulation"), Stat_GPU_BeginAccumulation, STATGROUP_GPU, RHI_API);
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("VolumetricLighting RenderVolume"), Stat_GPU_RenderVolume, STATGROUP_GPU, RHI_API);
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("VolumetricLighting EndAccumulation"), Stat_GPU_EndAccumulation, STATGROUP_GPU, RHI_API);
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("VolumetricLighting ApplyLighting"), Stat_GPU_ApplyLighting, STATGROUP_GPU, RHI_API);

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
	void UpdateStereoMode(bool IsStereo);
	void UpdateMRSLevel(int32 InLevel);
	void UpdateProjectionMode(bool IsReversedZ);

	// SeparateTranslucency
	void SetSeparateTranslucencyPostprocessDesc(const NvVl::PostprocessDesc& InPostprocessDesc);
	const NvVl::PostprocessDesc* GetSeparateTranslucencyPostprocessDesc();

	// 
	bool IsMSAAEnabled() const { return ContextDesc.eInternalSampleMode == NvVl::MultisampleMode::MSAA2 || ContextDesc.eInternalSampleMode == NvVl::MultisampleMode::MSAA4; }
	bool IsTemporalFilterEnabled() const { return ContextDesc.eFilterMode == NvVl::FilterMode::TEMPORAL; }
	bool IsRendering() const { return bEnableRendering; }
	
	void BeginAccumulation(FTextureRHIParamRef SceneDepthTextureRHI, const TArray<NvVl::ViewerDesc>& ViewerDescs, const NvVl::MediumDesc& MediumDesc, NvVl::DebugFlags DebugFlags);
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