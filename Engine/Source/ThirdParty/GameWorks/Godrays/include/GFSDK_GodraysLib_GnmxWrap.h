// This code contains NVIDIA Confidential Information and is disclosed 
// under the Mutual Non-Disclosure Agreement. 
// 
// Notice 
// ALL NVIDIA DESIGN SPECIFICATIONS AND CODE ("MATERIALS") ARE PROVIDED "AS IS" NVIDIA MAKES 
// NO REPRESENTATIONS, WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO 
// THE MATERIALS, AND EXPRESSLY DISCLAIMS ANY IMPLIED WARRANTIES OF NONINFRINGEMENT, 
// MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. 
// 
// NVIDIA Corporation assumes no responsibility for the consequences of use of such 
// information or for any infringement of patents or other rights of third parties that may 
// result from its use. No license is granted by implication or otherwise under any patent 
// or patent rights of NVIDIA Corporation. No third party distribution is allowed unless 
// expressly authorized by NVIDIA.  Details are subject to change without notice. 
// This code supersedes and replaces all information previously supplied. 
// NVIDIA Corporation products are not authorized for use as critical 
// components in life support devices or systems without express written approval of 
// NVIDIA Corporation. 
// 
// Copyright © 2008- 2013 NVIDIA Corporation. All rights reserved.
//
// NVIDIA Corporation and its licensors retain all intellectual property and proprietary
// rights in and to this software and related documentation and any modifications thereto.
// Any use, reproduction, disclosure or distribution of this software and related
// documentation without an express license agreement from NVIDIA Corporation is
// strictly prohibited.
//

/*
 * This header defines ABC's for indirecting Godrays access to Gnmx
 * The raison d'etre of this header is to avoid generating linkage into Gnmx inside
 * Godrays. By generating linkage outside of the lib (i.e. in the client app), it
 * is possible for clients to use customised versions of Gnmx
 */

#ifndef _GFSDK_GODRAYS_GNM_WRAPPER_H
#define _GFSDK_GODRAYS_GNM_WRAPPER_H

#include <GFSDK_Common.h>

#include <gnm/constants.h>

namespace sce {
	namespace Gnmx {
		struct InputResourceOffsets;
		class ShaderInfo;
		class CsShader;
		class PsShader;
		class VsShader;
		class HsShader;
		class LsShader;
	}
	namespace Gnm {
		class Buffer;
		class DrawCommandBuffer;
		class RenderTarget;
		class DepthRenderTarget;
		class DepthEqaaControl;
		class Texture;
		class Sampler;
		class DbRenderControl;
		class DepthStencilControl;
		class StencilControl;
		class StencilOpControl;
		class BlendControl;
		class PrimitiveSetup;
		class PsStageRegisters;
		class CsStageRegisters;
		class VsStageRegisters;
		class HsStageRegisters;
		class LsStageRegisters;
	}
}

	struct GFSDK_GodraysLib_GnmxWrap
	{
		// BaseGfxContext Wrappers
		virtual void setCbControl(sce::Gnm::CbMode mode, sce::Gnm::RasterOp op) = 0;
		virtual void triggerEvent(sce::Gnm::EventType eventType) = 0;
		virtual void writeImmediateDwordAtEndOfPipe(sce::Gnm::EndOfPipeEventType eventType, void *dstGpuAddr, uint64_t immValue, sce::Gnm::CacheAction cacheAction) = 0;
		virtual void waitOnAddress(void * gpuAddr, uint32_t mask, sce::Gnm::WaitCompareFunc compareFunc, uint32_t refValue) = 0;
		virtual void setCmaskClearColor(uint32_t rtSlot, const uint32_t clearColor[2]) = 0;
		virtual void setDepthClearValue(float clearValue) = 0;
		virtual void copyData(void *dstGpuAddr, const void *srcGpuAddr, uint32_t numBytes, sce::Gnm::DmaDataBlockingMode isBlocking) = 0;

		// LightweightGfxContext wrappers
		virtual void setEmbeddedPsShader(sce::Gnm::EmbeddedPsShader shaderId) = 0;
		virtual void setEmbeddedVsShader(sce::Gnm::EmbeddedVsShader shaderId, uint32_t shaderModifier) = 0;
		virtual void setShaderType(sce::Gnm::ShaderType shaderType) = 0;
		virtual void setCsShader(const sce::Gnmx::CsShader *csb, const sce::Gnmx::InputResourceOffsets *table) = 0;
		virtual void setRwBuffers(sce::Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const sce::Gnm::Buffer *rwBuffers) = 0;
		virtual void setBuffers(sce::Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const sce::Gnm::Buffer *buffers) = 0;
		virtual void setConstantBuffers(sce::Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const sce::Gnm::Buffer *buffers) = 0;
		virtual void dispatch(uint32_t threadGroupX, uint32_t threadGroupY, uint32_t threadGroupZ) = 0;
		virtual void * allocateFromCommandBuffer(uint32_t sizeInBytes, sce::Gnm::EmbeddedDataAlignment alignment) = 0;
		virtual void setAaDefaultSampleLocations(sce::Gnm::NumSamples numSamples) = 0;
		virtual void setScanModeControl(sce::Gnm::ScanModeControlAa msaa, sce::Gnm::ScanModeControlViewportScissor viewportScissor) = 0;
		virtual void setDepthEqaaControl(sce::Gnm::DepthEqaaControl depthEqaa) = 0;
		virtual void setVertexBuffers(sce::Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const sce::Gnm::Buffer *buffers) = 0;
		virtual void setPrimitiveType(sce::Gnm::PrimitiveType primType) = 0;
		virtual void setIndexSize(sce::Gnm::IndexSize indexSize) = 0;
		virtual void setIndexCount(uint32_t indexCount) = 0;
		virtual void setIndexOffset(uint32_t offset) = 0;
		virtual void setIndexBuffer(const void * indexAddr) = 0;
		virtual void drawIndexOffset(uint32_t indexOffset, uint32_t indexCount) = 0;
		virtual void drawIndexAuto(uint32_t indexCount) = 0;
		virtual void drawIndex(uint32_t indexCount, const void *indexAddr) = 0;
		virtual void pushMarker(const char * debugString) = 0;
		virtual void popMarker() = 0;
		virtual void setActiveShaderStages(sce::Gnm::ActiveShaderStages activeStages) = 0;
		virtual void setPsShaderRate(sce::Gnm::PsShaderRate rate) = 0;
		virtual void setVsShader(const sce::Gnmx::VsShader *vsb, uint32_t shaderModifier, void *fetchShaderAddr, const sce::Gnmx::InputResourceOffsets *table) = 0;
		virtual void setPsShader(const sce::Gnmx::PsShader *psb, const sce::Gnmx::InputResourceOffsets *table) = 0;
		virtual void setLsHsShaders(sce::Gnmx::LsShader *lsb, uint32_t shaderModifier, void *fetchShaderAddr, const sce::Gnmx::InputResourceOffsets *lsTable, const sce::Gnmx::HsShader *hsb, const sce::Gnmx::InputResourceOffsets *hsTable, uint32_t numPatches) = 0;
		virtual void setRenderTarget(uint32_t rtSlot, sce::Gnm::RenderTarget const *target) = 0;
		virtual void setDepthRenderTarget(sce::Gnm::DepthRenderTarget const * depthTarget) = 0;
		virtual void setupScreenViewport(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom, float zScale, float zOffset) = 0;
		virtual void setTextures(sce::Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const sce::Gnm::Texture *textures) = 0;
		virtual void setSamplers(sce::Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const sce::Gnm::Sampler *samplers) = 0;
		virtual void setDepthStencilDisable() = 0;
		virtual void setDepthStencilControl(sce::Gnm::DepthStencilControl depthControl) = 0;
		virtual void setStencil(sce::Gnm::StencilControl stencilControl) = 0;
		virtual void setStencilSeparate(sce::Gnm::StencilControl front, sce::Gnm::StencilControl back) = 0;
		virtual void setStencilOpControl(sce::Gnm::StencilOpControl stencilOpControl) = 0;
		virtual void setStencilClearValue(uint8_t clearValue) = 0;
		virtual void setDbRenderControl(sce::Gnm::DbRenderControl dbRenderControl) = 0;
		virtual void setBlendControl(uint32_t rtSlot, sce::Gnm::BlendControl blendControl) = 0;
		virtual void setBlendColor(float red, float green, float blue, float alpha) = 0;
		virtual void setPrimitiveSetup(sce::Gnm::PrimitiveSetup reg) = 0;
		virtual void setRenderTargetMask(uint32_t mask) = 0;
		virtual void waitForGraphicsWrites(uint32_t baseAddr256, uint32_t sizeIn256ByteBlocks, uint32_t targetMask, sce::Gnm::CacheAction cacheAction, uint32_t extendedCacheMask, sce::Gnm::StallCommandBufferParserMode commandBufferStallMode) = 0;
		virtual void setRwTextures(sce::Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const sce::Gnm::Texture *rwTextures) = 0;
		virtual void setVgtControl(uint8_t primGroupSize, sce::Gnm::VgtPartialVsWaveMode partialVsWaveMode) = 0;
		virtual void setTessellationDataConstantBuffer(void *tcbAddr, sce::Gnm::ShaderStage domainStage) = 0;
		virtual void setTessellationFactorBuffer(void* tfAddr) = 0;

		// Shader wrappers
		virtual void patchShaderGpuAddress(sce::Gnmx::LsShader& lsShader, void *gpuAddress) = 0;
		virtual void patchShaderGpuAddress(sce::Gnmx::HsShader& hsShader, void *gpuAddress) = 0;
		virtual void patchShaderGpuAddress(sce::Gnmx::PsShader& psShader, void *gpuAddress) = 0;
		virtual void patchShaderGpuAddress(sce::Gnmx::CsShader& csShader, void *gpuAddress) = 0;
		virtual void patchShaderGpuAddress(sce::Gnmx::VsShader& vsShader, void *gpuAddress) = 0;
		virtual void applyFetchShaderModifier(sce::Gnmx::VsShader& vsShader, uint32_t shaderModifier) = 0;
		virtual void applyFetchShaderModifier(sce::Gnmx::LsShader& lsShader, uint32_t shaderModifier) = 0;
		virtual const sce::Gnm::LsStageRegisters& getLsStageRegisters(sce::Gnmx::LsShader& lsShader) = 0;
		virtual const sce::Gnm::HsStageRegisters& getHsStageRegisters(sce::Gnmx::HsShader& hsShader) = 0;
		virtual const sce::Gnm::PsStageRegisters& getPsStageRegisters(sce::Gnmx::PsShader& psShader) = 0;
		virtual const sce::Gnm::CsStageRegisters& getCsStageRegisters(sce::Gnmx::CsShader& csShader) = 0;
		virtual const sce::Gnm::VsStageRegisters& getVsStageRegisters(sce::Gnmx::VsShader& vsShader) = 0;
		virtual uint32_t computeVsFetchShaderSize(const sce::Gnmx::VsShader *vsb) = 0;
		virtual void generateVsFetchShader(void *fs, uint32_t* shaderModifier, const sce::Gnmx::VsShader *vsb, const sce::Gnm::FetchShaderInstancingMode *instancingData) = 0;
		virtual uint32_t computeLsFetchShaderSize(const sce::Gnmx::LsShader *lsb) = 0;
		virtual void generateLsFetchShader(void *fs, uint32_t* shaderModifier, const sce::Gnmx::LsShader *lsb, const sce::Gnm::FetchShaderInstancingMode *instancingData) = 0;
		virtual uint32_t computeSize(const sce::Gnmx::HsShader& psShader) = 0;
		virtual uint32_t computeSize(const sce::Gnmx::LsShader& psShader) = 0;
		virtual uint32_t computeSize(const sce::Gnmx::PsShader& psShader) = 0;
		virtual uint32_t computeSize(const sce::Gnmx::VsShader& vsShader) = 0;
		virtual uint32_t computeSize(const sce::Gnmx::CsShader& csShader) = 0;

		// ShaderInfo wrappers
		virtual void parseShader(sce::Gnmx::ShaderInfo& shaderInfo, const void* data, int shaderType) = 0;
		virtual uint32_t getGpuShaderCodeSize(sce::Gnmx::ShaderInfo& shaderInfo) = 0;
		virtual const uint32_t* getGpuShaderCode(sce::Gnmx::ShaderInfo& shaderInfo) = 0;
		virtual sce::Gnmx::LsShader* getLsShader(sce::Gnmx::ShaderInfo& shaderInfo) = 0;
		virtual sce::Gnmx::HsShader* getHsShader(sce::Gnmx::ShaderInfo& shaderInfo) = 0;
		virtual sce::Gnmx::PsShader* getPsShader(sce::Gnmx::ShaderInfo& shaderInfo) = 0;
		virtual sce::Gnmx::CsShader* getCsShader(sce::Gnmx::ShaderInfo& shaderInfo) = 0;
		virtual sce::Gnmx::VsShader* getVsShader(sce::Gnmx::ShaderInfo& shaderInfo) = 0;

		// Synthesised wrappers/accessors
		virtual sce::Gnm::DrawCommandBuffer* getDcb() = 0;

		virtual int getLsShaderType() = 0;
		virtual int getHsShaderType() = 0;
		virtual int getVsShaderType() = 0;
		virtual int getPsShaderType() = 0;
		virtual int getCsShaderType() = 0;
		virtual size_t getSizeofShaderInfo() = 0;

		virtual int validate() = 0;
	};

#endif	// _GFSDK_GODRAYS_GNM_WRAPPER_H
