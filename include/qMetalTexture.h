/*
Copyright (c) 2019 Generation Loss Interactive

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef __Q_METAL_TEXTURE_H__
#define __Q_METAL_TEXTURE_H__

#include <Metal/Metal.h>
#include "qCore.h"
#include "qMetalSamplerState.h"
#include "qMath.h"

#define TEXTURE_FORMATS \
/*              enum            metal name                  		bytes per pixel    */ \
/* single channel */ \
TEXTURE_FORMAT(R8,					MTLPixelFormatR8Unorm,			1.0f   	) \
TEXTURE_FORMAT(R16f,				MTLPixelFormatR16Float,			2.0f   	) \
TEXTURE_FORMAT(R32f,				MTLPixelFormatR32Float,			4.0f   	) \
/* double channel */ \
TEXTURE_FORMAT(RG16u,				MTLPixelFormatRG16Uint,			4.0f   	) \
TEXTURE_FORMAT(RG16f,				MTLPixelFormatRG16Float,		4.0f   	) \
TEXTURE_FORMAT(RG32f,				MTLPixelFormatRG32Float,		8.0f   	) \
/* triple channel */ \
TEXTURE_FORMAT(RG11B10f,			MTLPixelFormatRG11B10Float,		4.0f   	) \
TEXTURE_FORMAT(RGB9E5f,				MTLPixelFormatRGB9E5Float,		4.0f   	) \
/* quadruple channel */ \
TEXTURE_FORMAT(BGRA8,				MTLPixelFormatBGRA8Unorm,		4.0f   	) \
TEXTURE_FORMAT(RGBA8,				MTLPixelFormatRGBA8Unorm,		4.0f   	) \
TEXTURE_FORMAT(RGBA8u,				MTLPixelFormatRGBA8Uint,		4.0f   	) \
TEXTURE_FORMAT(RGB10A2,				MTLPixelFormatRGB10A2Unorm,		4.0f   	) \
TEXTURE_FORMAT(RGBA16,				MTLPixelFormatRGBA16Unorm,		8.0f   	) \
TEXTURE_FORMAT(RGBA16s,				MTLPixelFormatRGBA16Snorm,		8.0f   	) \
TEXTURE_FORMAT(RGBA16f,				MTLPixelFormatRGBA16Float,		8.0f   	) \
TEXTURE_FORMAT(RGBA32f,				MTLPixelFormatRGBA32Float,		16.0f  	) \
/* compressed */ \
TEXTURE_FORMAT(RGBA_PVRTC_4BPP,		MTLPixelFormatPVRTC_RGBA_4BPP, 	0.5f   	) \
TEXTURE_FORMAT(ASTC_4x4,			MTLPixelFormatASTC_4x4_LDR, 	1.0f   	) \
/* depth */ \
TEXTURE_FORMAT(Depth32f,			MTLPixelFormatDepth32Float,		4.0f   	) \
/* stencil */ \
TEXTURE_FORMAT(Stencil8u,			MTLPixelFormatStencil8,			1.0f   	) \
/* misc */ \
TEXTURE_FORMAT(Invalid,				MTLPixelFormatInvalid,			0.0f   	) \
	
namespace qMetal
{
    class Texture
    {        
    public:
		enum eType
		{
			eType_2D                = MTLTextureType2D,
			eType_2DArray			= MTLTextureType2DArray,
			eType_Cube              = MTLTextureTypeCube,
			eType_3D              	= MTLTextureType3D,
		};
		
		enum ePixelFormat
		{
#define TEXTURE_FORMAT(xxenum, xxmetalname, xxbytesperpixel) ePixelFormat_ ## xxenum = xxmetalname,
			TEXTURE_FORMATS
#undef TEXTURE_FORMAT
		};
		
		enum eUnit
		{
			eUnit_Count = 32
		};
		
		enum eUsage
		{
			eUsage_Unknown 			= MTLTextureUsageUnknown,
			eUsage_ShaderRead		= MTLTextureUsageShaderRead,
			eUsage_ShaderWrite		= MTLTextureUsageShaderWrite,
			eUsage_RenderTarget		= MTLTextureUsageRenderTarget
		};
		
		enum eStorage
		{
			eStorage_Memoryless 	= MTLStorageModeMemoryless,
			eStorage_CPUandGPU 		= MTLStorageModeShared,
			eStorage_GPUOnly		= MTLStorageModePrivate
		};
		
		enum eMSAA
		{
			eMSAA_1					= 1,
			eMSAA_2					= 2,
			eMSAA_4					= 4
		};
		
        typedef struct Config
        {
			NSString*		name;
            NSUInteger		width;
			NSUInteger		height;
			NSUInteger		depth;
			NSUInteger		arrayLength;
            eType           type;
            ePixelFormat    pixelFormat;
            bool            mipMaps;
            eUsage			usage;
			eMSAA			msaa;
			eStorage		storage;
			
            Config(NSString *_name)
            : name([_name retain])
			, width(8)
            , height(8)
			, depth(1)
			, arrayLength(1)
            , type(eType_2D)
            , pixelFormat(ePixelFormat_RGBA8)
            , mipMaps(false)
            , usage(eUsage_ShaderRead)
			, msaa(eMSAA_1)
			, storage(eStorage_GPUOnly)
            {}
			
            Config(Config* config, NSString *_name)
            : name([_name retain])
			, width(config->width)
            , height(config->height)
			, depth(config->depth)
			, arrayLength(config->arrayLength)
            , type(config->type)
            , pixelFormat(config->pixelFormat)
            , mipMaps(config->mipMaps)
            , usage(config->usage)
			, msaa(config->msaa)
			, storage(config->storage)
            {}
			
            Config(
				   NSString*		_name,
				   NSUInteger		_width,
				   NSUInteger		_height,
				   NSUInteger		_depth,
				   NSUInteger		_arrayLength,
				   eType           	_type,
				   ePixelFormat    	_pixelFormat,
				   bool            	_mipMaps,
				   eUsage			_usage,
				   eMSAA			_msaa,
				   eStorage			_storage
			)
			: name([_name retain])
            , width(_width)
            , height(_height)
			, depth(_depth)
			, arrayLength(_arrayLength)
            , type(_type)
            , pixelFormat(_pixelFormat)
            , mipMaps(_mipMaps)
            , usage(_usage)
			, msaa(_msaa)
			, storage(_storage)
            {}
			
        } Config;
        
        Texture(id<MTLTexture> _texture, const SamplerState *_samplerState); //TODO this is really only for use by the device as you end up with a config-less texture; should consider making it private and device a friend
        
        Texture(Config *_config, SamplerState *_samplerState);
		
		~Texture();
		
        void EncodeCompute(id<MTLComputeCommandEncoder> encoder, eUnit textureIndex) const;
		void EncodeArgumentBuffer(id<MTLArgumentEncoder> encoder, eUnit textureIndex, eUnit samplerIndex) const;
		void EncodeUsage(id<MTLRenderCommandEncoder> encoder, MTLRenderStages stages) const;
		
		float BytesPerPixel() const;
		
        id<MTLTexture> MTLTexture() const;
        
        void Fill(uint8_t *data);
		void Fill(float *data);
		void Fill(half *data);
        void Fill(qRGBA8 *data);
        void Fill(qRGBA16f *data);
        void Fill(qRGBA32f *data);
        
		float SampleFloat(qVector2 uv, NSUInteger channel = 0) const;
		qRGBA8 SampleRGBA8(qVector2 uv) const;
		
		const Config* GetConfig() const;
		const NSString* GetName() const;
		static Texture* LoadByName(const NSString *name, const bool CPUReadable = false, const SamplerState *samplerState = SamplerState::PredefinedState(eSamplerState_LinearLinearLinear_RepeatRepeat));
        
    private:
		
		void Fill(void *data);
		
		template<typename T>
		T Sample(qVector2 uv, float bytesPerPixel) const;
		
		id<MTLTexture>    	texture;
        const Config		*config;
        const SamplerState	*samplerState;
    };
}

#endif //__Q_METAL_TEXTURE_H__

