
/*=============================================================================
	NVVolumetricLightingRHI.cpp: Nvidia Volumetric Lighting rendering implementation.
=============================================================================*/

#if WITH_NVVOLUMETRICLIGHTING
#include "RHIPrivatePCH.h"
#include "RHI.h"
#include "NVVolumetricLightingRHI.h"

FNVVolumetricLightingRHI* GNVVolumetricLightingRHI = NULL;

static TAutoConsoleVariable<int32> CVarNvVl(
	TEXT("r.NvVl"),
	1,
	TEXT("0 to disable volumetric lighting feature. Restart required"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

FNVVolumetricLightingRHI* CreateNVVolumetricLightingRHI()
{
	if (CVarNvVl.GetValueOnGameThread() == false)
	{
		return nullptr;
	}

	return new FNVVolumetricLightingRHI();
}

FNVVolumetricLightingRHI::FNVVolumetricLightingRHI()
	: ModuleHandle(NULL)
	, Context(NULL)
	, RenderCtx(NULL)
	, SceneDepthSRV(NULL)
	, bNeedUpdateContext(true)
	, bEnableRendering(false)
	, bEnableSeparateTranslucency(false)
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
	ContextDesc.remapCascadedShadowBuffer.uWidth = 0;
	ContextDesc.remapCascadedShadowBuffer.uHeight = 0;
	ContextDesc.remapCascadedShadowBuffer.uSamples = 0;
	ContextDesc.remapCascadedShadowBuffer.uSlices = 0;
    ContextDesc.eDownsampleMode = NvVl::DownsampleMode::FULL;
    ContextDesc.eInternalSampleMode = NvVl::MultisampleMode::SINGLE;
    ContextDesc.eFilterMode = NvVl::FilterMode::NONE;

	check(GDynamicRHI);
	GDynamicRHI->GetPlatformDesc(PlatformDesc);
	GDynamicRHI->GetPlatformRenderCtx(RenderCtx);

}

void FNVVolumetricLightingRHI::ReleaseContext()
{
	if (Context)
	{
		NvVl::ReleaseContext(Context);
		Context = NULL;
	}
}

void FNVVolumetricLightingRHI::Shutdown()
{
	ReleaseContext();

	NvVl::CloseLibrary();

	if (ModuleHandle)
	{
		FreeLibrary(ModuleHandle);
		ModuleHandle = NULL;
	}
}

void FNVVolumetricLightingRHI::UpdateContext()
{
	if (bNeedUpdateContext || Context == nullptr)
	{
		ReleaseContext();

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

void FNVVolumetricLightingRHI::UpdateDownsampleMode(uint32 InMode)
{
	if (ContextDesc.eDownsampleMode != (NvVl::DownsampleMode)InMode)
	{
		ContextDesc.eDownsampleMode = (NvVl::DownsampleMode)InMode;
		bNeedUpdateContext = true;
	}
}

void FNVVolumetricLightingRHI::UpdateMsaaMode(uint32 InMode)
{
	if (ContextDesc.eInternalSampleMode != (NvVl::MultisampleMode)InMode)
	{
		ContextDesc.eInternalSampleMode = (NvVl::MultisampleMode)InMode;
		bNeedUpdateContext = true;
	}
}

void FNVVolumetricLightingRHI::UpdateFilterMode(uint32 InMode)
{
	if (ContextDesc.eFilterMode != (NvVl::FilterMode)InMode)
	{
		ContextDesc.eFilterMode = (NvVl::FilterMode)InMode;
		bNeedUpdateContext = true;
	}
}

void FNVVolumetricLightingRHI::BeginAccumulation(FTextureRHIParamRef SceneDepthTextureRHI, const NvVl::ViewerDesc& ViewerDesc, const NvVl::MediumDesc& MediumDesc, NvVl::DebugFlags DebugFlags)
{
	UpdateContext();
	GDynamicRHI->GetPlatformShaderResource(SceneDepthTextureRHI, SceneDepthSRV);
	NvVl::Status Status = NvVl::BeginAccumulation(Context, RenderCtx, SceneDepthSRV, &ViewerDesc, &MediumDesc, DebugFlags);
	check(Status == NvVl::Status::OK);
	GDynamicRHI->ClearStateCache();
}

void FNVVolumetricLightingRHI::RenderVolume(const TArray<FTextureRHIParamRef>& ShadowMapTextures, const NvVl::ShadowMapDesc& ShadowMapDesc, const NvVl::LightDesc& LightDesc, const NvVl::VolumeDesc& VolumeDesc)
{
	NvVl::PlatformShaderResource ShadowMapSRVs[NvVl::MAX_SHADOWMAP_ELEMENTS] = {
		NvVl::PlatformShaderResource(NULL),
		NvVl::PlatformShaderResource(NULL),
		NvVl::PlatformShaderResource(NULL),
		NvVl::PlatformShaderResource(NULL),
	};

	for (int Index = 0; Index < NvVl::MAX_SHADOWMAP_ELEMENTS; Index++)
	{
		if (Index < ShadowMapTextures.Num() && ShadowMapTextures[Index])
		{
			GDynamicRHI->GetPlatformShaderResource(ShadowMapTextures[Index], ShadowMapSRVs[Index]);
		}
	}
	
	NvVl::Status Status = NvVl::RenderVolume(Context, RenderCtx, SceneDepthSRV, ShadowMapSRVs, &ShadowMapDesc, &LightDesc, &VolumeDesc);
	check(Status == NvVl::Status::OK);
	GDynamicRHI->ClearStateCache();
}

void FNVVolumetricLightingRHI::EndAccumulation()
{
	NvVl::Status Status = NvVl::EndAccumulation(Context, RenderCtx);
	check(Status == NvVl::Status::OK);
}

void FNVVolumetricLightingRHI::ApplyLighting(FTextureRHIParamRef SceneColorSurfaceRHI, const NvVl::PostprocessDesc& PostprocessDesc)
{
	NvVl::PlatformRenderTarget SceneRTV(NULL);
	GDynamicRHI->GetPlatformRenderTarget(SceneColorSurfaceRHI, SceneRTV);
	NvVl::Status Status = NvVl::ApplyLighting(Context, RenderCtx, SceneRTV, SceneDepthSRV, &PostprocessDesc);
	check(Status == NvVl::Status::OK);
	GDynamicRHI->ClearStateCache();
}

void FNVVolumetricLightingRHI::SetSeparateTranslucencyPostprocessDesc(const NvVl::PostprocessDesc& InPostprocessDesc)
{
	bEnableSeparateTranslucency = true;
	SeparateTranslucencyPostprocessDesc = InPostprocessDesc;
}

const NvVl::PostprocessDesc* FNVVolumetricLightingRHI::GetSeparateTranslucencyPostprocessDesc()
{
	if (bEnableSeparateTranslucency)
	{
		return &SeparateTranslucencyPostprocessDesc;
	}
	else
	{
		return nullptr;
	}
}

void FNVVolumetricLightingRHI::UpdateRendering(bool Enabled)
{
	bEnableSeparateTranslucency = false;
	bEnableRendering = Enabled;
}
#endif