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

#include "qMetalTexture.h"
#include "qMetalDevice.h"
#include <MetalKit/MetalKit.h>
#include <map>

namespace qMetal
{
	MTKTextureLoader* sMTKTextureLoader = NULL;
	
	typedef std::map<const NSString*, Texture* > loadedTextureMap_t;
	static loadedTextureMap_t sLoadedTextureMap;

	Texture::Texture(id<MTLTexture> _texture, const SamplerState* _samplerState)
	: texture(_texture)
	, config(NULL)
	, samplerState(_samplerState)
	{
	}
	
	Texture::Texture(Config* _config, SamplerState* _samplerState)
	: texture(nil)
	, config(_config)
	, samplerState(_samplerState)
	{
		MTLTextureDescriptor* textureDescriptor = NULL;
	  
		switch (config->type) {
			case eType_2D:
			case eType_2DArray:
				qASSERTM(config->depth == 1, "eType_2D textures must have depth of 1");
				qASSERTM((config->type == eType_2DArray) || (config->arrayLength == 1), "eType_2D textures must have an arrayLength of 1");
				textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:(MTLPixelFormat)config->pixelFormat width:config->width height:config->height mipmapped:config->mipMaps];
				textureDescriptor.textureType = (MTLTextureType)config->type;
				textureDescriptor.arrayLength = config->arrayLength;
				if (config->msaa != eMSAA_1)
				{
					qASSERTM(config->type == eType_2D, "eType_2DArray textures do not support MSAA on iOS");
					textureDescriptor.textureType = MTLTextureType2DMultisample;
					textureDescriptor.sampleCount = config->msaa;
				}
				break;
				
			case eType_Cube:
				qASSERTM(config->width == config->height, "eType_Cube textures must have the same width and height");
				qASSERTM(config->arrayLength == 1, "eType_Cube textures must have an arrayLength of 1");
				qASSERTM(config->depth == 1, "eType_Cube textures must have depth of 1");
				qASSERTM(config->msaa == eMSAA_1, "eType_Cube textures must not use MSAA");
				textureDescriptor = [MTLTextureDescriptor textureCubeDescriptorWithPixelFormat:(MTLPixelFormat)config->pixelFormat size:config->width mipmapped:config->mipMaps];
				break;
			
			case eType_3D:
				qASSERTM(config->arrayLength == 1, "eType_3D textures must have an arrayLength of 1");
				qASSERTM(config->msaa == eMSAA_1, "eType_3D textures must not use MSAA");
				textureDescriptor = [MTLTextureDescriptor new];
				textureDescriptor.textureType = MTLTextureType3D;
				textureDescriptor.width = config->width;
				textureDescriptor.height = config->height;
				textureDescriptor.depth = config->depth;
				textureDescriptor.pixelFormat = (MTLPixelFormat)config->pixelFormat;
				textureDescriptor.mipmapLevelCount = config->mipMaps ? qFloor(log2f((float)(qMax(config->width, qMax(config->height, config->depth))))) + 1 : 1;
				break;

		default:
				qBREAK("Unknown texture format");
				break;
		}
		
		textureDescriptor.usage = (MTLTextureUsage)config->usage;
		textureDescriptor.storageMode = (MTLStorageMode)config->storage;
		
		if ((config->type != eType_2D) && (config->storage == eStorage_Memoryless))
		{
			//only 2D textures support memoryless, so we'll pick the next best
			qSPAM("Warning: only 2D render targets (with optional MSAA) support memoryless, so we'll pick the next best of GPU-only storage");
			textureDescriptor.storageMode = (MTLStorageMode)eStorage_GPUOnly;
		}
		
		texture = [qMetal::Device::Get() newTextureWithDescriptor: textureDescriptor];
		texture.label = config->name;
	}
	
	Texture::~Texture()
	{
		if (config != NULL)
		{
			delete(config);
		}
		if (samplerState != NULL)
		{
			delete(samplerState);
		}
	}
			
	void Texture::EncodeCompute(id<MTLComputeCommandEncoder> encoder, eUnit textureIndex) const
	{
		qASSERT(texture != nil);
		[encoder setTexture:texture atIndex:textureIndex];
	}
	
	void Texture::EncodeArgumentBuffer(id<MTLArgumentEncoder> encoder, eUnit textureIndex) const
	{
		qASSERT(texture != nil);
		[encoder setTexture:texture atIndex:textureIndex];
	}
	
	void Texture::EncodeArgumentBuffer(id<MTLArgumentEncoder> encoder, eUnit textureIndex, eUnit samplerIndex) const
	{
		EncodeArgumentBuffer(encoder, textureIndex);
		samplerState->Encode(encoder, samplerIndex);
	}
	
	void Texture::EncodeUsage(id<MTLRenderCommandEncoder> encoder, MTLRenderStages stages) const
	{
		qASSERT(texture != nil);
		[encoder useResource:texture usage:MTLResourceUsageSample stages:stages];
	}
	
	float Texture::BytesPerPixel() const
	{
		switch(config->pixelFormat)
		{
#define TEXTURE_FORMAT(xxenum, xxmetalname, xxbytesperpixel) case(ePixelFormat_ ## xxenum): return xxbytesperpixel;
				TEXTURE_FORMATS
#undef TEXTURE_FORMAT
			default:
				qBREAK("Unknown pixel format");
				return 0;
		}
	}
	
	id<MTLTexture> Texture::MTLTexture() const
	{
		return texture;
	}
	
	void Texture::Fill(uint8_t* data)
	{
		qASSERT(config->pixelFormat == ePixelFormat_R8);
		Fill((void*)data);
	}
	
	void Texture::Fill(float* data)
	{
		qASSERT(config->pixelFormat == ePixelFormat_R32f);
		Fill((void*)data);
	}
	
	void Texture::Fill(half* data)
	{
		qASSERT(config->pixelFormat == ePixelFormat_R16f);
		Fill((void*)data);
	}
	
	void Texture::Fill(qRGBA8* data)
	{
		qASSERT(config->pixelFormat == ePixelFormat_RGBA8);
		Fill((void*)data);
	}
	
	void Texture::Fill(qRGBA16f* data)
	{
		qASSERT(config->pixelFormat == ePixelFormat_RGBA16f);
		Fill((void*)data);
	}
	
	void Texture::Fill(qRGBA32f* data)
	{
		qASSERT(config->pixelFormat == ePixelFormat_RGBA32f);
		Fill((void*)data);
	}
	
	float Texture::SampleFloat(qVector2 uv, NSUInteger channel) const
	{
		switch(config->pixelFormat)
		{
			case ePixelFormat_R16f:
				return Sample<half>(uv, 2);
				break;
			case ePixelFormat_RG16f:
				return Sample<half2>(uv, 2)[channel];
				break;
			case ePixelFormat_R32f:
				return Sample<float>(uv, 4);
				break;
			default:
				qASSERTM(false, "Trying to sample invalid texture type");
				return 0.0f;
				break;
		}
	}
	
	qRGBA8 Texture::SampleRGBA8(qVector2 uv) const
	{
		switch(config->pixelFormat)
		{
			case ePixelFormat_RGBA8:
				return Sample<qRGBA8>(uv, 4);
				break;
			default:
				qASSERTM(false, "Trying to sample invalid texture type");
				return 0.0f;
				break;
		}
	}
	
	const Texture::Config* Texture::GetConfig() const { return config; }
	const NSString* Texture::GetName() const { return texture.label; }

	Texture* Texture::LoadByName(const NSString* nameWithExtension, const bool CPUReadable, const SamplerState* samplerState)
	{
		if (sLoadedTextureMap.find(nameWithExtension) != sLoadedTextureMap.end())
		{
			return sLoadedTextureMap.find(nameWithExtension)->second;
		}
		
		NSError* error = nil;
		
		if (sMTKTextureLoader == NULL)
		{
			sMTKTextureLoader = [[MTKTextureLoader alloc] initWithDevice:qMetal::Device::Get()];
		}
		
		NSString* path = [[NSBundle mainBundle] pathForResource:[nameWithExtension stringByDeletingPathExtension] ofType:[nameWithExtension pathExtension]];
		NSURL* url = [NSURL fileURLWithPath:path];
		
		id<MTLTexture> texture = [sMTKTextureLoader newTextureWithContentsOfURL:url options:@{
																							  MTKTextureLoaderOptionTextureStorageMode:@(CPUReadable ? MTLStorageModeShared : MTLStorageModePrivate)
																							  } error:&error];
		qASSERTM(error == nil, "Texture::LoadByName: error loading texture %s, %s", [nameWithExtension UTF8String], [[error description] UTF8String]);
		
		qASSERTM(texture != nil, "Texture::LoadByName: texture is nil %s", [nameWithExtension UTF8String]);
		
		Texture* qTexture = new Texture(texture, samplerState);
		
		loadedTextureMap_t::value_type KV(nameWithExtension, qTexture);
			
		#if DEBUG
		std::pair<loadedTextureMap_t::iterator, bool> result =
		#endif
		sLoadedTextureMap.insert(KV);
		qASSERTM(result.second == true, "Failed to insert loaded texture");
		
		return qTexture;
	}
	
	void Texture::Fill(void* data)
	{
		MTLRegion region = MTLRegionMake2D(0, 0, config->width, config->height);
		NSUInteger bytesPerRow = (NSUInteger)(float(config->width) * BytesPerPixel());
		[texture replaceRegion:region mipmapLevel:0 withBytes:data bytesPerRow:bytesPerRow];
	}
	
	template<typename T>
	T Texture::Sample(qVector2 uv, float bytesPerPixel) const
	{
		//quantize it to an exact pixel
		float UVxFloor = qFloor(uv.x * (float)config->width) / (float)config->width;
		float UVyFloor = qFloor(uv.y * (float)config->height) / (float)config->height;
		
		NSUInteger textureX = (NSUInteger)(UVxFloor * (float)config->width);
		NSUInteger textureY = (NSUInteger)(UVyFloor * (float)config->height);
		
		NSUInteger bytesPerRow = NSUInteger(float(config->width) * bytesPerPixel);
		
		T bytesTop[2];
		MTLRegion regionTop = MTLRegionMake2D(textureX, textureY, 2, 1);
		[MTLTexture() getBytes:bytesTop bytesPerRow:bytesPerRow fromRegion:regionTop mipmapLevel:0];
		
		T bytesBot[2];
		MTLRegion regionBot = MTLRegionMake2D(textureX, textureY + 1, 2, 1);
		[MTLTexture() getBytes:bytesBot bytesPerRow:bytesPerRow fromRegion:regionBot mipmapLevel:0];
		
		float UVxFraction = (uv.x - UVxFloor) * (float)config->width;
		float UVyFraction = (uv.y - UVyFloor) * (float)config->height;
		
		//bilinear filter
		T xTop = qLerp(bytesTop[0], bytesTop[1], UVxFraction);
		T xBot = qLerp(bytesBot[0], bytesBot[1], UVxFraction);
		return qLerp(xTop, xBot, UVyFraction);
	}
}
