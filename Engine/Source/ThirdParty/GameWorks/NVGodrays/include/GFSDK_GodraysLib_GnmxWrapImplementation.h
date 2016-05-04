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
 * This header defines a default implementation for GnmxWrap which can be
 * trivially #included in the client's compilation space to handle Gnmx calls
 */

#ifndef _GFSDK_GODRAYS_GNM_WRAPPER_IMPLEMENTATION_H
#define _GFSDK_GODRAYS_GNM_WRAPPER_IMPLEMENTATION_H

#include <GFSDK_GodraysLib_GnmxWrap.h>

#include <gnm/regs.h>
#include <gnmx/shader_parser.h>
#include <gnmx/fetchshaderhelper.h>
#include <gnmx/helpers.h>
#include <gnmx/surfacetool.h>

template <typename GfxContext>
struct GFSDK_GodraysLib_GnmxWrapImplementation : public GFSDK_GodraysLib_GnmxWrap
{
	GFSDK_GodraysLib_GnmxWrapImplementation(GfxContext& gfxc) 
		: gfxc_(gfxc) 
	{}

	////////////////////////////////////////////////////////////////
	// BaseGfxContext Wrappers
	virtual void setCbControl(sce::Gnm::CbMode mode, sce::Gnm::RasterOp op)
	{ gfxc_.setCbControl(mode, op); }

	virtual void triggerEvent(sce::Gnm::EventType eventType)
	{ gfxc_.triggerEvent(eventType); }

	virtual void writeImmediateDwordAtEndOfPipe(sce::Gnm::EndOfPipeEventType eventType, void *dstGpuAddr, uint64_t immValue, sce::Gnm::CacheAction cacheAction)
	{ gfxc_.writeImmediateDwordAtEndOfPipe(eventType, dstGpuAddr, immValue, cacheAction); }

	virtual void waitOnAddress(void * gpuAddr, uint32_t mask, sce::Gnm::WaitCompareFunc compareFunc, uint32_t refValue)
	{ gfxc_.waitOnAddress(gpuAddr, mask, compareFunc, refValue); }

	virtual void setCmaskClearColor(uint32_t rtSlot, const uint32_t clearColor[2])
	{ gfxc_.setCmaskClearColor(rtSlot, clearColor); }

	virtual void setDepthClearValue(float clearValue)
	{ gfxc_.setDepthClearValue(clearValue); }

	virtual void copyData(void *dstGpuAddr, const void *srcGpuAddr, uint32_t numBytes, sce::Gnm::DmaDataBlockingMode isBlocking)
	{ gfxc_.copyData(dstGpuAddr, srcGpuAddr, numBytes, isBlocking); }

	////////////////////////////////////////////////////////////////
	// LightweightGfxContext

	virtual void setEmbeddedPsShader(sce::Gnm::EmbeddedPsShader shaderId)
	{ gfxc_.setEmbeddedPsShader(shaderId); }

	virtual void setEmbeddedVsShader(sce::Gnm::EmbeddedVsShader shaderId, uint32_t shaderModifier)
	{ gfxc_.setEmbeddedVsShader(shaderId, shaderModifier); }

	virtual void setShaderType(sce::Gnm::ShaderType shaderType)
	{ gfxc_.setShaderType(shaderType); }

	virtual void setCsShader(const sce::Gnmx::CsShader *csb, const sce::Gnmx::InputResourceOffsets *table)
	{ gfxc_.setCsShader(csb, table); }

	virtual void setRwBuffers(sce::Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const sce::Gnm::Buffer *rwBuffers)
	{ gfxc_.setRwBuffers(stage, startSlot, numSlots, rwBuffers); }

	virtual void setBuffers(sce::Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const sce::Gnm::Buffer *buffers)
	{ gfxc_.setBuffers(stage, startSlot, numSlots, buffers); }

	virtual void setConstantBuffers(sce::Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const sce::Gnm::Buffer *buffers)
	{ gfxc_.setConstantBuffers(stage, startSlot, numSlots, buffers); }

	virtual void dispatch(uint32_t threadGroupX, uint32_t threadGroupY, uint32_t threadGroupZ)
	{ gfxc_.dispatch(threadGroupX, threadGroupY, threadGroupZ); }

	virtual void * allocateFromCommandBuffer(uint32_t sizeInBytes, sce::Gnm::EmbeddedDataAlignment alignment)
	{ return gfxc_.allocateFromCommandBuffer(sizeInBytes, alignment); }

	virtual void setAaDefaultSampleLocations(sce::Gnm::NumSamples numSamples) 
	{ gfxc_.setAaDefaultSampleLocations(numSamples); }

	virtual void setScanModeControl(sce::Gnm::ScanModeControlAa msaa, sce::Gnm::ScanModeControlViewportScissor viewportScissor)
	{ gfxc_.setScanModeControl(msaa, viewportScissor); }

	virtual void setDepthEqaaControl(sce::Gnm::DepthEqaaControl depthEqaa)
	{ gfxc_.setDepthEqaaControl(depthEqaa); }

	virtual void setVertexBuffers(sce::Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const sce::Gnm::Buffer *buffers)
	{ gfxc_.setVertexBuffers(stage, startSlot, numSlots, buffers); }

	virtual void setPrimitiveType(sce::Gnm::PrimitiveType primType)
	{ gfxc_.setPrimitiveType(primType); }

	virtual void setIndexSize(sce::Gnm::IndexSize indexSize)
	{ gfxc_.setIndexSize(indexSize); }

	virtual void setIndexCount(uint32_t indexCount)
	{ gfxc_.setIndexCount(indexCount); }

	virtual void setIndexOffset(uint32_t offset)
	{ gfxc_.setIndexOffset(offset); }

	virtual void setIndexBuffer(const void * indexAddr)
	{ gfxc_.setIndexBuffer(indexAddr); }

	virtual void drawIndexAuto(uint32_t indexCount) 
	{ gfxc_.drawIndexAuto(indexCount); }

	virtual void drawIndexOffset(uint32_t indexOffset, uint32_t indexCount)
	{ gfxc_.drawIndexOffset(indexOffset, indexCount); }

	virtual void drawIndex(uint32_t indexCount, const void *indexAddr)
	{ gfxc_.drawIndex(indexCount, indexAddr); }

	virtual void pushMarker(const char * debugString)
	{ gfxc_.pushMarker(debugString); }

	virtual void popMarker()
	{ gfxc_.popMarker(); }

	virtual void setPsShaderRate(sce::Gnm::PsShaderRate rate)
	{ gfxc_.setPsShaderRate(rate); }

	virtual void setActiveShaderStages(sce::Gnm::ActiveShaderStages activeStages)
	{ gfxc_.setActiveShaderStages(activeStages); }

	virtual void setVsShader(const sce::Gnmx::VsShader *vsb, uint32_t shaderModifier, void *fetchShaderAddr, const sce::Gnmx::InputResourceOffsets *table)
	{ gfxc_.setVsShader(vsb, shaderModifier, fetchShaderAddr, table); }

	virtual void setPsShader(const sce::Gnmx::PsShader *psb, const sce::Gnmx::InputResourceOffsets *table)
	{ gfxc_.setPsShader(psb, table); }

	virtual void setLsHsShaders(sce::Gnmx::LsShader *lsb, uint32_t shaderModifier, void *fetchShaderAddr, const sce::Gnmx::InputResourceOffsets *lsTable, const sce::Gnmx::HsShader *hsb, const sce::Gnmx::InputResourceOffsets *hsTable, uint32_t numPatches)
	{ gfxc_.setLsHsShaders(lsb, shaderModifier, fetchShaderAddr, lsTable, hsb, hsTable, numPatches); }

	virtual void setRenderTarget(uint32_t rtSlot, sce::Gnm::RenderTarget const *target)
	{ gfxc_.setRenderTarget(rtSlot, target); }

	virtual void setDepthRenderTarget(sce::Gnm::DepthRenderTarget const * depthTarget)
	{ gfxc_.setDepthRenderTarget(depthTarget); }

	virtual void setupScreenViewport(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom, float zScale, float zOffset)
	{ gfxc_.setupScreenViewport(left, top, right, bottom, zScale, zOffset); }

	virtual void setTextures(sce::Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const sce::Gnm::Texture *textures)
	{ gfxc_.setTextures(stage, startSlot, numSlots, textures); }

	virtual void setSamplers(sce::Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const sce::Gnm::Sampler *samplers)
	{ gfxc_.setSamplers(stage, startSlot, numSlots, samplers); }

	virtual void setDepthStencilControl(sce::Gnm::DepthStencilControl depthControl)
	{ gfxc_.setDepthStencilControl(depthControl); }

	virtual void setDepthStencilDisable()
	{ gfxc_.setDepthStencilDisable(); }

	virtual void setStencil(sce::Gnm::StencilControl stencilControl) 
	{ gfxc_.setStencil(stencilControl); }

	virtual void setStencilSeparate(sce::Gnm::StencilControl front, sce::Gnm::StencilControl back) 
	{ gfxc_.setStencilSeparate(front, back); }
	
	virtual void setStencilOpControl(sce::Gnm::StencilOpControl stencilOpControl)
	{ gfxc_.setStencilOpControl(stencilOpControl); }

	virtual void setStencilClearValue(uint8_t clearValue)
	{ gfxc_.setStencilClearValue(clearValue); }

	virtual void setDbRenderControl(sce::Gnm::DbRenderControl dbRenderControl)
	{ gfxc_.setDbRenderControl(dbRenderControl); }

	virtual void setBlendControl(uint32_t rtSlot, sce::Gnm::BlendControl blendControl) 
	{ gfxc_.setBlendControl(rtSlot, blendControl); }

	virtual void setBlendColor(float red, float green, float blue, float alpha)
	{ gfxc_.setBlendColor(red, green, blue, alpha); }

	virtual void setPrimitiveSetup(sce::Gnm::PrimitiveSetup reg)
	{ gfxc_.setPrimitiveSetup(reg); }

	virtual void setRenderTargetMask(uint32_t mask)
	{ gfxc_.setRenderTargetMask(mask); }

	virtual void waitForGraphicsWrites(uint32_t baseAddr256, uint32_t sizeIn256ByteBlocks, uint32_t targetMask, sce::Gnm::CacheAction cacheAction, uint32_t extendedCacheMask, sce::Gnm::StallCommandBufferParserMode commandBufferStallMode)
	{ gfxc_.waitForGraphicsWrites(baseAddr256, sizeIn256ByteBlocks, targetMask, cacheAction, extendedCacheMask, commandBufferStallMode); }

	virtual void setRwTextures(sce::Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const sce::Gnm::Texture *rwTextures)
	{ gfxc_.setRwTextures(stage, startSlot, numSlots, rwTextures); }

	virtual void setVgtControl(uint8_t primGroupSize, sce::Gnm::VgtPartialVsWaveMode partialVsWaveMode)
	{ gfxc_.setVgtControl(primGroupSize, partialVsWaveMode); }

	virtual void setTessellationDataConstantBuffer(void *tcbAddr, sce::Gnm::ShaderStage domainStage)
	{ gfxc_.setTessellationDataConstantBuffer(tcbAddr, domainStage); }

	virtual void setTessellationFactorBuffer(void* tfAddr) 
	{ gfxc_.setTessellationFactorBuffer(tfAddr); }

	virtual void patchShaderGpuAddress(sce::Gnmx::LsShader& lsShader, void *gpuAddress)
	{ lsShader.patchShaderGpuAddress(gpuAddress); }

	virtual void patchShaderGpuAddress(sce::Gnmx::HsShader& hsShader, void *gpuAddress)
	{ hsShader.patchShaderGpuAddress(gpuAddress); }

	virtual void patchShaderGpuAddress(sce::Gnmx::PsShader& psShader, void *gpuAddress)
	{ psShader.patchShaderGpuAddress(gpuAddress); }

	virtual void patchShaderGpuAddress(sce::Gnmx::CsShader& csShader, void *gpuAddress)
	{ csShader.patchShaderGpuAddress(gpuAddress); }

	virtual void patchShaderGpuAddress(sce::Gnmx::VsShader& vsShader, void *gpuAddress)
	{ vsShader.patchShaderGpuAddress(gpuAddress); }

	virtual void applyFetchShaderModifier(sce::Gnmx::VsShader& vsShader, uint32_t shaderModifier)
	{ vsShader.applyFetchShaderModifier(shaderModifier); }

	virtual void applyFetchShaderModifier(sce::Gnmx::LsShader& lsShader, uint32_t shaderModifier) 
	{ lsShader.applyFetchShaderModifier(shaderModifier); }

	virtual const sce::Gnm::LsStageRegisters& getLsStageRegisters(sce::Gnmx::LsShader& lsShader)
	{ return lsShader.m_lsStageRegisters; }

	virtual const sce::Gnm::HsStageRegisters& getHsStageRegisters(sce::Gnmx::HsShader& hsShader)
	{ return hsShader.m_hsStageRegisters; }

	virtual const sce::Gnm::PsStageRegisters& getPsStageRegisters(sce::Gnmx::PsShader& psShader)
	{ return psShader.m_psStageRegisters; }

	virtual const sce::Gnm::CsStageRegisters& getCsStageRegisters(sce::Gnmx::CsShader& csShader)
	{ return csShader.m_csStageRegisters; }

	virtual const sce::Gnm::VsStageRegisters& getVsStageRegisters(sce::Gnmx::VsShader& vsShader)
	{ return vsShader.m_vsStageRegisters; }

	virtual uint32_t computeVsFetchShaderSize(const sce::Gnmx::VsShader *vsb)
	{ return sce::Gnmx::computeVsFetchShaderSize(vsb); }

	virtual void generateVsFetchShader(void *fs, uint32_t* shaderModifier, const sce::Gnmx::VsShader *vsb, const sce::Gnm::FetchShaderInstancingMode *instancingData)
	{ return sce::Gnmx::generateVsFetchShader(fs, shaderModifier, vsb, instancingData); }

	virtual uint32_t computeLsFetchShaderSize(const sce::Gnmx::LsShader *lsb)
	{ return sce::Gnmx::computeLsFetchShaderSize(lsb); }

	virtual void generateLsFetchShader(void *fs, uint32_t* shaderModifier, const sce::Gnmx::LsShader *lsb, const sce::Gnm::FetchShaderInstancingMode *instancingData)
	{ return sce::Gnmx::generateLsFetchShader(fs, shaderModifier, lsb, instancingData); }

	virtual uint32_t computeSize(const sce::Gnmx::LsShader& lsShader)
	{ return lsShader.computeSize(); }

	virtual uint32_t computeSize(const sce::Gnmx::HsShader& hsShader)
	{ return hsShader.computeSize(); }

	virtual uint32_t computeSize(const sce::Gnmx::PsShader& psShader)
	{ return psShader.computeSize(); }

	virtual uint32_t computeSize(const sce::Gnmx::VsShader& vsShader)
	{ return vsShader.computeSize(); }

	virtual uint32_t computeSize(const sce::Gnmx::CsShader& csShader)
	{ return csShader.computeSize(); }

	virtual void parseShader(sce::Gnmx::ShaderInfo& shaderInfo, const void* data, int /* deprecated: shaderType*/)
	{ sce::Gnmx::parseShader(&shaderInfo, data); }

	virtual uint32_t getGpuShaderCodeSize(sce::Gnmx::ShaderInfo& shaderInfo)
	{ return shaderInfo.m_gpuShaderCodeSize; }

	virtual const uint32_t* getGpuShaderCode(sce::Gnmx::ShaderInfo& shaderInfo)
	{ return shaderInfo.m_gpuShaderCode; }

	virtual sce::Gnmx::LsShader* getLsShader(sce::Gnmx::ShaderInfo& shaderInfo)
	{ return shaderInfo.m_lsShader; }

	virtual sce::Gnmx::HsShader* getHsShader(sce::Gnmx::ShaderInfo& shaderInfo)
	{ return shaderInfo.m_hsShader; }

	virtual sce::Gnmx::PsShader* getPsShader(sce::Gnmx::ShaderInfo& shaderInfo)
	{ return shaderInfo.m_psShader; }

	virtual sce::Gnmx::CsShader* getCsShader(sce::Gnmx::ShaderInfo& shaderInfo)
	{ return shaderInfo.m_csShader; }

	virtual sce::Gnmx::VsShader* getVsShader(sce::Gnmx::ShaderInfo& shaderInfo)
	{ return shaderInfo.m_vsShader; }

	// Synthesised wrappers/accessors
	virtual sce::Gnm::DrawCommandBuffer* getDcb()
	{ return &gfxc_.m_dcb; }

	virtual int getLsShaderType()
	{ return sce::Gnmx::kLocalShader; }

	virtual int getHsShaderType()
	{ return sce::Gnmx::kHullShader; }

	virtual int getVsShaderType()
	{ return sce::Gnmx::kVertexShader; }

	virtual int getPsShaderType()
	{ return sce::Gnmx::kPixelShader; }

	virtual int getCsShaderType()
	{ return sce::Gnmx::kComputeShader; }

	virtual size_t getSizeofShaderInfo()
	{ return sizeof(sce::Gnmx::ShaderInfo); }

	virtual int validate()
	{ return gfxc_.validate(); }

	GfxContext& gfxc_;
};

#endif	// _GFSDK_GODRAYS_GNM_WRAPPER_IMPLEMENTATION_H
