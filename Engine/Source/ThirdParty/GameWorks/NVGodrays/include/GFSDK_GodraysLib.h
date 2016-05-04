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
// Copyright (C) 2012, NVIDIA Corporation. All rights reserved.
 
/*===========================================================================
                                  GFSDK_GodraysLib.h
=============================================================================

NVIDIA GodraysLib v1.0 by Dmitry Kozlov
-----------------------------------------

ENGINEERING CONTACT
Dmitry Kozlov (NVIDIA Devtech)
dmitryk@nvidia.com

===========================================================================*/

// defines general GFSDK includes and structs
#pragma once
#include "GFSDK_Common.h"

#if defined ( __GFSDK_DX11__ )
	struct ID3D11Device;
	typedef struct ID3D11DeviceContext*			GFSDK_GodraysLib_Type_RenderCtx;
	typedef struct ID3D11RenderTargetView*		GFSDK_GodraysLib_Type_RenderTarget;
	typedef struct ID3D11DepthStencilView*		GFSDK_GodraysLib_Type_DepthStencilTarget;
	typedef struct ID3D11ShaderResourceView*	GFSDK_GodraysLib_Type_ShaderResource;
	typedef struct ID3D11BlendState*			GFSDK_GodraysLib_Type_BlendState;
#elif defined ( __GFSDK_GL__ )
	#pragma message("GodraysLib does not currently support Open GL.  If you want it to, contact us and let us know!");
#elif defined ( __GFSDK_GNM__ )
	#include "GFSDK_GodraysLib_GnmxWrap.h"

	typedef GFSDK_GodraysLib_GnmxWrap*			GFSDK_GodraysLib_Type_RenderCtx;
	typedef class sce::Gnm::RenderTarget*		GFSDK_GodraysLib_Type_RenderTarget;
	typedef class sce::Gnm::DepthRenderTarget*	GFSDK_GodraysLib_Type_DepthStencilTarget;
	typedef class sce::Gnm::Texture*			GFSDK_GodraysLib_Type_ShaderResource;
	typedef class sce::Gnm::BlendControl*		GFSDK_GodraysLib_Type_BlendState;

	// orbis-specific malloc hooks
	struct GFSDK_GodraysLib_Malloc_Hooks
	{
		void* (*pOnionAlloc)(size_t size, size_t alignment);
		void (*pOnionFree)(void* ptr);
		void* (*pGarlicAlloc)(size_t size, size_t alignment);
		void (*pGarlicFree)(void* ptr);
	};
#endif

typedef struct GFSDK_GodraysLib_Ctx* GFSDK_GodraysLib_Handle;

#define __GFSDK_GODRAYSLIB_VERSION_MAJOR__	1
#define __GFSDK_GODRAYSLIB_VERSION_MINOR__	2
static gfsdk_U32 const GFSDK_GodraysLib_Version = (__GFSDK_GODRAYSLIB_VERSION_MAJOR__ << 16) | ( __GFSDK_GODRAYSLIB_VERSION_MINOR__ );

#define __GFSDK_GODRAYSLIB_INTERFACE__ GFSDK_GodraysLib_Status __GFSDK_CDECL__

#if defined(_DURANGO)
#define __GFSDK_GODRAYSLIB_EXTERN_INTERFACE__ __GFSDK_GODRAYSLIB_INTERFACE__
#else
#ifdef __DLL_GFSDK_GODRAYSLIB_EXPORTS__
#define __GFSDK_GODRAYSLIB_EXTERN_INTERFACE__ __GFSDK_EXPORT__ __GFSDK_GODRAYSLIB_INTERFACE__
#else
#define __GFSDK_GODRAYSLIB_EXTERN_INTERFACE__ __GFSDK_IMPORT__ __GFSDK_GODRAYSLIB_INTERFACE__
#endif
#endif

/*===========================================================================
Return codes for the lib
===========================================================================*/
enum GFSDK_GodraysLib_Status
{
    GFSDK_GodraysLib_Status_Ok											=  0,	// Success
    GFSDK_GodraysLib_Status_Fail										= -1,   // Fail
	GFSDK_GodraysLib_Status_Invalid_Version						        = -2,	// Mismatch between header and dll
	GFSDK_GodraysLib_Status_Invalid_Parameter							= -3,	// One or more invalid parameters
	GFSDK_GodraysLib_Status_Out_Of_Memory								= -4,	// Failed to allocate a resource
    GFSDK_GodraysLib_Status_UnsupportedDevice 							= -5,	// Failed to allocate a resource
};

/*===========================================================================
Context Creation Flags
===========================================================================*/

const gfsdk_U32 GFSDK_GodraysLib_OpenFlag_None				= 0x00000000;
const gfsdk_U32 GFSDK_GodraysLib_OpenFlag_ResourceOverride	= 0x00000001;

/*===========================================================================
Debug mode constants (bit flags)
===========================================================================*/
const gfsdk_U32 GFSDK_GodraysLib_Debug_Wireframe	= 0x00000001;
const gfsdk_U32 GFSDK_GodraysLib_Debug_NoBlending	= 0x00000002;

/*===========================================================================
Specifies the accepted shadow map formats
===========================================================================*/
enum GFSDK_GodraysLib_ShadowMapFormat
{
	GFSDK_GodraysLib_MapFormat_D16,
	GFSDK_GodraysLib_MapFormat_D32,
	GFSDK_GodraysLib_MapFormat_Max,
};

/*===========================================================================
Specifies the godrays buffer size
===========================================================================*/
enum GFSDK_GodraysLib_BufferSize
{
	GFSDK_GodraysLib_BufferSize_Full,
	GFSDK_GodraysLib_BufferSize_Half,
	GFSDK_GodraysLib_BufferSize_Quarter,
	GFSDK_GodraysLib_BufferSize_Max,
};

/*===========================================================================
Specifies godrays technique to use
===========================================================================*/
enum GFSDK_GodraysLib_Technique
{
	GFSDK_GodraysLib_Technique_Polygonal,
	GFSDK_GodraysLib_Technique_Raymarching,
	GFSDK_GodraysLib_Technique_Max,
};

/*===========================================================================
Specifies the shadow map type
===========================================================================*/
enum GFSDK_GodraysLib_ShadowMapType
{
	GFSDK_GodraysLib_ShadowMapType_Simple,
	GFSDK_GodraysLib_ShadowMapType_2_Cascades,
	GFSDK_GodraysLib_ShadowMapType_3_Cascades,
	GFSDK_GodraysLib_ShadowMapType_4_Cascades,
	GFSDK_GodraysLib_ShadowMapType_Max,
    GFSDK_GodraysLib_ShadowMapType_Array_1_Cascade,
    GFSDK_GodraysLib_ShadowMapType_Array_2_Cascades,
    GFSDK_GodraysLib_ShadowMapType_Array_3_Cascades,
    GFSDK_GodraysLib_ShadowMapType_Array_4_Cascades
};

/*===========================================================================
Specifies the class of light source
===========================================================================*/

enum GFSDK_GodraysLib_LightType
{
	GFSDK_GodraysLib_LightType_Directional,
	GFSDK_GodraysLib_LightType_Spot,
	GFSDK_GodraysLib_LightType_Omni,
	GFSDK_GodraysLib_LightType_Max
};

/*===========================================================================
Specifies the type of attenuation function
===========================================================================*/

enum GFSDK_GodraysLib_AttenuationMode
{
	GFSDK_GodraysLib_AttenuationMode_InversePolynomial,
	GFSDK_GodraysLib_AttenuationMode_Max
};

/*===========================================================================
Quality of upsampling
===========================================================================*/

enum GFSDK_GodraysLib_UpsampleQuality
{
	GFSDK_GodraysLib_UpsampleQuality_Point,
	GFSDK_GodraysLib_UpsampleQuality_Bilinear,
	GFSDK_GodraysLib_UpsampleQuality_Bilateral,
	GFSDK_GodraysLib_UpsampleQuality_Max
};

/*===========================================================================
Shadow Map Structural Description
===========================================================================*/

struct GFSDK_GodraysLib_ShadowMapCascadeDesc 
{
	gfsdk_U32 uOffsetX;						// X-offset within texture
	gfsdk_U32 uOffsetY;						// Y-offset within texture
	gfsdk_U32 uWidth;						// Footprint width within texture
	gfsdk_U32 uHeight;						// Footprint height within texture
	gfsdk_float4x4 mViewProj;				// View-Proj transform for cascade
    gfsdk_U32 mArrayIndex;					// Texture array uindex for this cascade (if used)
};

struct GFSDK_GodraysLib_ShadowMapDesc
{
	GFSDK_GodraysLib_ShadowMapType eType;	// Shadow map structure type
	gfsdk_U32 uWidth;						// Shadow map texture width
	gfsdk_U32 uHeight;						// Shadow map texture height
	GFSDK_GodraysLib_ShadowMapCascadeDesc Cascades[GFSDK_GodraysLib_ShadowMapType_Max];
};

/*===========================================================================
Viewer Camera/Framebuffer Description
===========================================================================*/

struct GFSDK_GodraysLib_ViewerDesc 
{
	gfsdk_float4x4 mProj;					// Camera projection transform
	gfsdk_float4x4 mViewProj;				// Camera view-proj transform
	gfsdk_float3 vViewPos;					// Camera position in world-space
	gfsdk_U32 uWidth;						// Viewport Width (may differ from framebuffer)
	gfsdk_U32 uHeight;						// Viewport Height (may differ from framebuffer)
};

/*===========================================================================
Light Description
===========================================================================*/

struct GFSDK_GodraysLib_LightDesc 
{
	GFSDK_GodraysLib_LightType eType;			// Type of light source
	gfsdk_float4x4 mLightToWorld;				// Light clip-space to world-space transform
	gfsdk_float3 vIntensity;					// Color of light
	union
	{
		// GFSDK_GodraysLib_LightType_Directional
		struct {
			gfsdk_float3 vDirection;			// Normalized light direction
			gfsdk_F32 fLightToEyeDepth;			// Optical distance from directional light-source to eye (can be greater than actual)
		} Directional;

		// GFSDK_GodraysLib_LightType_Spot
		struct {
			gfsdk_float3 vDirection;			// Normalized light direction
			gfsdk_float3 vPosition;				// Light position in worldspace
			gfsdk_F32 fFalloff_CosTheta;		// Spotlight falloff angle
			gfsdk_F32 fFalloff_Power;			// Spotlight power
			gfsdk_F32 fAttenuationFactors[4];	// Factors in the attenuation equation
		} Spotlight;

		// GFSDK_GodraysLib_LightType_Omni
		struct {
			gfsdk_float3 vPosition;				// Light position in worldspace
			gfsdk_F32 fAttenuationFactors[4];	// Factors in the attenuation equation
		} Omni;
	};
};

/*===========================================================================
Volume Medium Description
===========================================================================*/

struct GFSDK_GodraysLib_MediumDesc
{
	gfsdk_float3 vAirScatter;				// Rayleigh scattering terms
	gfsdk_float3 vFwdScatter;				// Mie scattering terms
	gfsdk_float3 vBackScatter;				// Mie scattering terms
	gfsdk_F32 fFwdScatterPhase;				// Mie scattering phase
	gfsdk_F32 fBackScatterPhase;			// Mie scattering phase
	gfsdk_float3 vAbsorption;				// Absorption term
};

/*===========================================================================
Post-Processing Behavior Description
===========================================================================*/

struct GFSDK_GodraysLib_PostProcessDesc
{
	gfsdk_float3 vFogLight;					// Light intensity to use for multi-scattering
	gfsdk_F32 vHazeIntensity;				// Multiplier for overall godray effect
	gfsdk_bool bDoFog;						// Apply fogging based on scattering
	gfsdk_bool bIgnoreSkyFog;				// Ignore depth values of (1.0f) for fogging
	gfsdk_F32 fMultiScatter;				// Multi-scattering fraction (for fogging)	
	gfsdk_F32 fTemporalFactor;				// Weight of pixel history smoothing (0.0 for off)
	gfsdk_F32 fFilterThreshold;				// Threshold of frame movement to use temporal history
	gfsdk_float4x4 mUnjitteredViewProj;		// Camera view projection without jitter
};

/*===========================================================================
                        GFSDK_GodraysLib_GetVersion
----------------------------------------------------------------------------
This method returns the version of GodraysLib library
===========================================================================*/
#if defined(__GFSDK_DX11__) || defined(__GFSDK_GNM__)
__GFSDK_GODRAYSLIB_EXTERN_INTERFACE__ GFSDK_GodraysLib_GetVersion( gfsdk_U32* pVersion );
#endif

/*===========================================================================
                        GFSDK_GodraysLib_OpenDX
----------------------------------------------------------------------------

This method initializes GodraysLib, creates required resources and returns the GFSDK_GodraysLib_Handle.

If you wish the library to use custom heap allocators, pass them in as the final parameter.

 ----------------------------------------------------------------------------

===========================================================================*/
#if defined(__GFSDK_DX11__)
__GFSDK_GODRAYSLIB_EXTERN_INTERFACE__ GFSDK_GodraysLib_OpenDX(	
	gfsdk_U32 version,
	GFSDK_GodraysLib_Handle* pHandle,
  	ID3D11Device* const	dev,
	gfsdk_U32 flags,
  	gfsdk_new_delete_t* customAllocator );
#elif defined ( __GFSDK_GNM__ )
__GFSDK_GODRAYSLIB_EXTERN_INTERFACE__ GFSDK_GodraysLib_OpenGnm(	
	gfsdk_U32 version,
	GFSDK_GodraysLib_Handle* pHandle,
	GFSDK_GodraysLib_GnmxWrap* pGnmxWrap,
	gfsdk_U32 flags,
	GFSDK_GodraysLib_Malloc_Hooks* pCustomAllocator );
#endif

/*===========================================================================
                        GFSDK_GodraysLib_CloseDX
----------------------------------------------------------------------------

===========================================================================*/
__GFSDK_GODRAYSLIB_EXTERN_INTERFACE__ GFSDK_GodraysLib_Close( GFSDK_GodraysLib_Handle handle );

/*===========================================================================
                        GFSDK_GodraysLib_BeginAccumulation
-----------------------------------------------------------------------------

This method creates/clears necessary resources for generating volume lighting

 ----------------------------------------------------------------------------

===========================================================================*/
__GFSDK_GODRAYSLIB_EXTERN_INTERFACE__ GFSDK_GodraysLib_BeginAccumulation(
	GFSDK_GodraysLib_Handle handle,
	GFSDK_GodraysLib_Type_RenderCtx renderCtx, 
	GFSDK_GodraysLib_ViewerDesc const* pViewerDesc,
	GFSDK_GodraysLib_Type_RenderTarget sceneRT,
	GFSDK_GodraysLib_Type_ShaderResource depthBuffer,
	GFSDK_GodraysLib_MediumDesc const* pMediumDesc,
	gfsdk_F32 fDistanceScale,
	GFSDK_GodraysLib_Technique eTechnique,
	GFSDK_GodraysLib_BufferSize eBufferSize,
	gfsdk_bool bUseInternalBuffer,
	gfsdk_U32 iBufferSampleCount
	);

/*===========================================================================
                        GFSDK_GodraysLib_RenderVolume
----------------------------------------------------------------------------

The method reconstructs light volumes from the shadow map and calculates
airlight integral RGB value into the currently set render target. 

 ----------------------------------------------------------------------------

===========================================================================*/
__GFSDK_GODRAYSLIB_EXTERN_INTERFACE__ GFSDK_GodraysLib_RenderVolume(
	GFSDK_GodraysLib_Handle handle,
	GFSDK_GodraysLib_Type_RenderCtx renderCtx, 
	GFSDK_GodraysLib_ShadowMapDesc const* pShadowMapDesc,
	GFSDK_GodraysLib_Type_ShaderResource shadowMap,
	GFSDK_GodraysLib_LightDesc const* pLightDesc,
	gfsdk_F32 fBlendfactor,
	gfsdk_F32 fTargetRayResolution,
    gfsdk_U32 uGridResolution,
	gfsdk_F32 fGodrayBias
	);


/*===========================================================================
                        GFSDK_GodraysLib_EndAccumulation
----------------------------------------------------------------------------

Finalizes the accumulated volumes and lets the library know it's not 
rendering any more.

 ----------------------------------------------------------------------------

===========================================================================*/
__GFSDK_GODRAYSLIB_EXTERN_INTERFACE__ GFSDK_GodraysLib_EndAccumulation(
	GFSDK_GodraysLib_Handle handle,
	GFSDK_GodraysLib_Type_RenderCtx renderCtx
	);

/*===========================================================================
                        GFSDK_GodraysLib_ApplyLighting
----------------------------------------------------------------------------

Applies the accumulated lighting to the bound render target

 ----------------------------------------------------------------------------

Created Resources: None
Device State Sideeffects: None

===========================================================================*/
__GFSDK_GODRAYSLIB_EXTERN_INTERFACE__ GFSDK_GodraysLib_ApplyLighting(
	GFSDK_GodraysLib_Handle handle,
	GFSDK_GodraysLib_Type_RenderCtx renderCtx,
	GFSDK_GodraysLib_Type_RenderTarget sceneRT,
	GFSDK_GodraysLib_Type_ShaderResource depthBuffer,
    GFSDK_GodraysLib_PostProcessDesc const* pPostProcessDesc,
	GFSDK_GodraysLib_UpsampleQuality upsampleQuality,
	gfsdk_F32  fBlendfactor,
	GFSDK_GodraysLib_Type_BlendState externalBlendState
	);

/*===========================================================================
                        GFSDK_GodraysLib_GetInternalDepth
----------------------------------------------------------------------------

Get the internal depth-buffer used by the library

 ----------------------------------------------------------------------------

Created Resources: None
Device State Sideeffects: None

===========================================================================*/
__GFSDK_GODRAYSLIB_EXTERN_INTERFACE__ GFSDK_GodraysLib_GetInternalDepth(
	GFSDK_GodraysLib_Handle handle,
	GFSDK_GodraysLib_Type_DepthStencilTarget * pDepthTarget
	);

/*===========================================================================
                        GFSDK_GodraysLib_SetDebugMode()
----------------------------------------------------------------------------

The method allows to enable debug mode, which visualizes lighting volumes in 
wireframe mode without blending

 ----------------------------------------------------------------------------

Created Resources: None
Device State Sideeffects: None

===========================================================================*/
__GFSDK_GODRAYSLIB_EXTERN_INTERFACE__ GFSDK_GodraysLib_SetDebugMode( GFSDK_GodraysLib_Handle handle, gfsdk_U32 uDebug );

#if defined(_DURANGO)
/*===========================================================================
                        GFSDK_GodraysLib_SetResourceOverride()
----------------------------------------------------------------------------

Give the library a resource allocated on the app-side to force specific
memory behavior.

 ----------------------------------------------------------------------------

Created Resources: None
Device State Sideeffects: None

===========================================================================*/

enum GFSDK_GodraysLib_Resource
{
	GFSDK_GodraysLib_Resource_RT_PhaseLUT,
    GFSDK_GodraysLib_Resource_RT_Accumulation,
	GFSDK_GodraysLib_Resource_DS_Depth,
	GFSDK_GodraysLib_Resource_RT_Resolved,
	GFSDK_GodraysLib_Resource_RT_ResolvedDepth,
	GFSDK_GodraysLib_Resource_RT_Filtered_0,
	GFSDK_GodraysLib_Resource_RT_Filtered_1,
	GFSDK_GodraysLib_Resource_RT_FilteredDepth_0,
	GFSDK_GodraysLib_Resource_RT_FilteredDepth_1,
	GFSDK_GodraysLib_Resource_Count
};

__GFSDK_GODRAYSLIB_EXTERN_INTERFACE__ GFSDK_GodraysLib_SetResourceOverride( GFSDK_GodraysLib_Handle handle, GFSDK_GodraysLib_Type_RenderCtx renderCtx, GFSDK_GodraysLib_Resource resourceID, void ** resourceData, gfsdk_U32 resourceDataCount );
#endif