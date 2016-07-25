
/*=============================================================================
	NVVolumetricLightingRHI.cpp: Nvidia Volumetric Lighting rendering implementation.
=============================================================================*/

#if WITH_NVVOLUMETRICLIGHTING
#include "RHI.h"
#include "NVVolumetricLightingRHI.h"

FNVVolumetricLightingRHI* GNVVolumetricLightingRHI = NULL;

FNVVolumetricLightingRHI* CreateNVVolumetricLightingRHI()
{
	return new FNVVolumetricLightingRHI();
}

FNVVolumetricLightingRHI::FNVVolumetricLightingRHI()
	: ModuleHandle(NULL)
	, Context(NULL)
	, RenderCtx(NULL)
	, SceneDepthSRV(NULL)
	, bNeedUpdateContext(true)
	, bEnableSeparateTranslucencyPostprocess(false)
	, MaxShadowBufferWidthPerFrame(0)
	, MaxShadowBufferHeightPerFrame(0)
	, MaxShadowBufferSlicesPerFrame(0)
{
}

void FNVVolumetricLightingRHI::Init()
{
	FString NVVolumetricLightingBinariesPath = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/GameWorks/NvVolumetricLighting/");
#if PLATFORM_64BITS
#if UE_BUILD_DEBUG && !defined(NDEBUG)
	ModuleHandle = LoadLibraryW(*(NVVolumetricLightingBinariesPath + "NvVolumetricLighting.win64.D.dll"));
#else
	ModuleHandle = LoadLibraryW(*(NVVolumetricLightingBinariesPath + "NvVolumetricLighting.win64.dll"));
#endif
#endif
	check(ModuleHandle);

	NvVl::OpenLibrary();

    ContextDesc.framebuffer.uWidth = 0;
    ContextDesc.framebuffer.uHeight = 0;
    ContextDesc.framebuffer.uSamples = 0;
	ContextDesc.cascadedShadowBuffer.uWidth = 0;
	ContextDesc.cascadedShadowBuffer.uHeight = 0;
	ContextDesc.cascadedShadowBuffer.uSlices = 0;
    ContextDesc.eDownsampleMode = NvVl::DownsampleMode::FULL;
    ContextDesc.eInternalSampleMode = NvVl::MultisampleMode::SINGLE;
    ContextDesc.eFilterMode = NvVl::FilterMode::NONE;

	check(GDynamicRHI);
	GDynamicRHI->GetVolumeLightingPlatformDesc(PlatformDesc);
	GDynamicRHI->GetVolumeLightingPlatformRenderCtx(RenderCtx);

}

void FNVVolumetricLightingRHI::Shutdown()
{
	if (Context)
	{
		NvVl::ReleaseContext(Context);
		Context = NULL;
	}

	NvVl::CloseLibrary();

	if (ModuleHandle)
	{
		FreeLibrary(ModuleHandle);
		ModuleHandle = NULL;
	}
}

void FNVVolumetricLightingRHI::UpdateContext()
{
	if (bNeedUpdateContext)
	{
		if (Context)
		{
			NvVl::ReleaseContext(Context);
			Context = NULL;
		}

		NvVl::Status NvVlStatus = NvVl::CreateContext(Context, &PlatformDesc, &ContextDesc);
		check(NvVlStatus == NvVl::Status::OK);

		bNeedUpdateContext = false;
	}
}

void FNVVolumetricLightingRHI::UpdateFrameBuffer(int32 InBufferSizeX, int32 InBufferSizeY, uint16 InNumSamples)
{
	if (ContextDesc.framebuffer.uWidth != InBufferSizeX
	||  ContextDesc.framebuffer.uHeight != InBufferSizeY
	||  ContextDesc.framebuffer.uSamples != InNumSamples)
	{
		ContextDesc.framebuffer.uWidth = InBufferSizeX;
		ContextDesc.framebuffer.uHeight = InBufferSizeY;
		ContextDesc.framebuffer.uSamples = InNumSamples;

		bNeedUpdateContext = true;
	}
}

void FNVVolumetricLightingRHI::UpdateCascadedShadow(int32 InBufferSizeX, int32 InBufferSizeY, uint32 InSlices)
{
	if ((uint32)InBufferSizeX > MaxShadowBufferWidthPerFrame) MaxShadowBufferWidthPerFrame = InBufferSizeX;
	if ((uint32)InBufferSizeY > MaxShadowBufferHeightPerFrame) MaxShadowBufferHeightPerFrame = InBufferSizeY;
	if ((uint32)InSlices > MaxShadowBufferSlicesPerFrame)
	{
		MaxShadowBufferSlicesPerFrame = FMath::Min(NvVl::MAX_SHADOWMAP_ELEMENTS, InSlices);
	}
}

void FNVVolumetricLightingRHI::UpdateShadowBuffer()
{
	if (ContextDesc.cascadedShadowBuffer.uWidth != MaxShadowBufferWidthPerFrame
	|| ContextDesc.cascadedShadowBuffer.uHeight != MaxShadowBufferHeightPerFrame
	|| ContextDesc.cascadedShadowBuffer.uSlices != MaxShadowBufferSlicesPerFrame)
	{
		ContextDesc.cascadedShadowBuffer.uWidth = MaxShadowBufferWidthPerFrame;
		ContextDesc.cascadedShadowBuffer.uHeight = MaxShadowBufferHeightPerFrame;
		ContextDesc.cascadedShadowBuffer.uSlices = MaxShadowBufferSlicesPerFrame;

		bNeedUpdateContext = true;
	}

	MaxShadowBufferWidthPerFrame = 0;
	MaxShadowBufferHeightPerFrame = 0;
	MaxShadowBufferSlicesPerFrame = 0;
}

void FNVVolumetricLightingRHI::BeginAccumulation(FTextureRHIParamRef SceneDepthTextureRHI, const NvVl::ViewerDesc& ViewerDesc, const NvVl::MediumDesc& MediumDesc, NvVl::DebugFlags DebugFlags)
{
	bEnableSeparateTranslucencyPostprocess = false;
	UpdateShadowBuffer();
	UpdateContext();
	GDynamicRHI->GetPlatformShaderResource(SceneDepthTextureRHI, SceneDepthSRV);
	NvVl::Status Status = NvVl::BeginAccumulation(Context, RenderCtx, SceneDepthSRV, &ViewerDesc, &MediumDesc, DebugFlags);
	check(Status == NvVl::Status::OK);
}

void FNVVolumetricLightingRHI::RemapShadowDepth(FTextureRHIParamRef ShadowMapTextureRHI)
{
	NvVl::PlatformShaderResource ShadowMapSRV(NULL);
	GDynamicRHI->GetPlatformShaderResource(ShadowMapTextureRHI, ShadowMapSRV);
	NvVl::Status Status = NvVl::RemapShadowDepth(Context, RenderCtx, ShadowMapSRV);
	check(Status == NvVl::Status::OK);
}

void FNVVolumetricLightingRHI::RenderVolume(FTextureRHIParamRef ShadowMapTextureRHI, const NvVl::ShadowMapDesc& ShadowMapDesc, const NvVl::LightDesc& LightDesc, const NvVl::VolumeDesc& VolumeDesc)
{
	NvVl::PlatformShaderResource ShadowMapSRV(NULL);
	GDynamicRHI->GetPlatformShaderResource(ShadowMapTextureRHI, ShadowMapSRV);
	NvVl::Status Status = NvVl::RenderVolume(Context, RenderCtx, ShadowMapSRV, &ShadowMapDesc, &LightDesc, &VolumeDesc);
	check(Status == NvVl::Status::OK);
}

void FNVVolumetricLightingRHI::EndAccumulation()
{
	NvVl::Status Status = NvVl::EndAccumulation(Context, RenderCtx);
	check(Status == NvVl::Status::OK);
}

void FNVVolumetricLightingRHI::ApplyLighting(FTextureRHIParamRef SceneColorSurfaceRHI, const NvVl::PostprocessDesc InPostprocessDesc)
{
	NvVl::PlatformRenderTarget SceneRTV(NULL);
	GDynamicRHI->GetPlatformRenderTarget(SceneColorSurfaceRHI, SceneRTV);
	NvVl::Status Status = NvVl::ApplyLighting(Context, RenderCtx, SceneRTV, SceneDepthSRV, &InPostprocessDesc);
	check(Status == NvVl::Status::OK);
}

void FNVVolumetricLightingRHI::SetSeparateTranslucencyPostprocess(bool bEnable, const NvVl::PostprocessDesc InPostprocessDesc)
{
	bEnableSeparateTranslucencyPostprocess = bEnable;
	SeparateTranslucencyPostprocessDesc = InPostprocessDesc;
}

bool FNVVolumetricLightingRHI::SeparateTranslucencyApplyLighting(FTextureRHIParamRef SceneColorSurfaceRHI)
{
	if (bEnableSeparateTranslucencyPostprocess)
	{
		NvVl::PlatformRenderTarget SceneRTV(NULL);
		GDynamicRHI->GetPlatformRenderTarget(SceneColorSurfaceRHI, SceneRTV);
		NvVl::Status Status = NvVl::ApplyLighting(Context, RenderCtx, SceneRTV, SceneDepthSRV, &SeparateTranslucencyPostprocessDesc);
		check(Status == NvVl::Status::OK);
	}

	return bEnableSeparateTranslucencyPostprocess;
}

#endif