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

#include "qMetalRenderTarget.h"

namespace qMetal
{
	RenderTarget::RenderTarget(const Config* _config)
	: config(_config)
	, mEncoder(nil)
	{
		renderPassDescriptor = [MTLRenderPassDescriptor new];
		
		for (int i = 0; i < (int)config->colorAttachmentCount; ++i)
		{
			if (config->colourTexture[i] != NULL)
			{
				mColourTexture[i] = config->colourTexture[i];
			}
			else if (config->colourTextureConfig[i] != NULL)
			{
				config->colourTextureConfig[i]->usage = (Texture::eUsage)(config->colourTextureConfig[i]->usage | Texture::eUsage_RenderTarget);
				mColourTexture[i] = new qMetal::Texture(config->colourTextureConfig[i], config->colourTextureSamplerState[i]);
			}
			else
			{
				qBREAK("No colour texture or config provided");
			}
			
			const Texture::Config* textureConfig = mColourTexture[i]->GetConfig();
			
			mColourTexturePixelFormat[i] = textureConfig == NULL ? Texture::ePixelFormat_BGRA8 : textureConfig->pixelFormat;
			
			MTLRenderPassColorAttachmentDescriptor* attachment = renderPassDescriptor.colorAttachments[i];
			attachment.texture = mColourTexture[i]->MTLTexture();
			attachment.loadAction = (MTLLoadAction)config->clearAction[i];
			attachment.clearColor = MTLClearColorMake(config->clearColour[i].r, config->clearColour[i].g, config->clearColour[i].b, config->clearColour[i].a);
			attachment.storeAction = (textureConfig != NULL && (textureConfig->storage == Texture::eStorage_Memoryless)) ? MTLStoreActionDontCare : MTLStoreActionStore; //this works even if we're a cube, as we'll set the texture storage to GPU only, but then a discard as "Don't care" here
			attachment.slice = config->slice;
			attachment.level = config->level;
			
			if (config->colourResolveTexture[i] != NULL)
			{
				qASSERTM(mColourTexture[i]->GetConfig()->msaa != Texture::eMSAA_1, "Setting up a resove texture without MSAA on the main texture");
				qASSERTM(config->colourResolveTexture[i]->GetConfig()->msaa == Texture::eMSAA_1, "Setting up a resove texture that itself has MSAA");
				mColourResolveTexture[i] = config->colourResolveTexture[i];
			}
			else if (config->colourResolveTextureConfig[i] != NULL)
			{
				qASSERTM(mColourTexture[i]->GetConfig()->msaa != Texture::eMSAA_1, "Setting up a resove texture without MSAA on the main texture");
				qASSERTM(config->colourResolveTextureConfig[i]->msaa == Texture::eMSAA_1, "Setting up a resove texture that itself has MSAA")
				config->colourResolveTextureConfig[i]->usage = (Texture::eUsage)(config->colourResolveTextureConfig[i]->usage | Texture::eUsage_RenderTarget);
				mColourResolveTexture[i] = new qMetal::Texture(config->colourResolveTextureConfig[i], config->colourResolveTextureSamplerState[i]);
			}
			else
			{
				mColourResolveTexture[i] = NULL;
			}
			
			if (mColourResolveTexture[i] != NULL)
			{
				qWARNING(mColourTexture[i]->GetConfig()->storage == Texture::eStorage_Memoryless, "Colour texture %i resolves out, but isn't memoryless", i);
				attachment.resolveTexture = mColourResolveTexture[i]->MTLTexture();
				attachment.storeAction = MTLStoreActionMultisampleResolve;
			}
		}
		
		for (int i = config->colorAttachmentCount; i < eColorAttachment_Count; ++i)
		{
			mColourTexturePixelFormat[i] = Texture::ePixelFormat_Invalid;
		}
		
		if (config->depthTexture != NULL || config->depthTextureConfig != NULL)
		{
			if (config->depthTexture != NULL)
			{
				mDepthTexture = config->depthTexture;
			}
			else if (config->depthTextureConfig != NULL)
			{
				config->depthTextureConfig->usage = (Texture::eUsage)(config->depthTextureConfig->usage | Texture::eUsage_RenderTarget);
				mDepthTexture = new qMetal::Texture(config->depthTextureConfig, config->depthTextureSamplerState);
			}
			
			const Texture::Config* textureConfig = mDepthTexture->GetConfig();
			
			MTLRenderPassDepthAttachmentDescriptor* attachment = renderPassDescriptor.depthAttachment;
			attachment.texture = mDepthTexture->MTLTexture();
			attachment.loadAction = (MTLLoadAction)config->depthClearAction;
			attachment.clearDepth = config->depthClear;
			attachment.storeAction = (textureConfig != NULL && (textureConfig->storage == Texture::eStorage_Memoryless)) ? MTLStoreActionDontCare : MTLStoreActionStore;
			attachment.slice = config->slice;
			attachment.level = config->level;
		
			if (config->depthResolveTexture != NULL)
			{
				qASSERTM(mDepthTexture->GetConfig()->msaa != Texture::eMSAA_1, "Setting up a depth resove texture without MSAA on the depth texture");
				mDepthResolveTexture = config->depthResolveTexture;
			}
			else if (config->depthResolveTextureConfig != NULL)
			{
				qASSERTM(mDepthTexture->GetConfig()->msaa != Texture::eMSAA_1, "Setting up a depth resove texture without MSAA on the depth texture");
				qASSERTM(config->depthResolveTextureConfig->msaa == Texture::eMSAA_1, "Setting up a depth resove texture that itself has MSAA")
				config->depthResolveTextureConfig->usage = (Texture::eUsage)(config->depthTextureConfig->usage | Texture::eUsage_RenderTarget);
				mDepthResolveTexture = new qMetal::Texture(config->depthResolveTextureConfig, config->depthResolveTextureSamplerState);
			}
			else
			{
				 mDepthResolveTexture = NULL;
			}
			
			if (mDepthResolveTexture != NULL)
			{
				qWARNING(mDepthTexture->GetConfig()->storage == Texture::eStorage_Memoryless, "Depth texture resolves out, but isn't memoryless");
				attachment.resolveTexture = mDepthResolveTexture->MTLTexture();
				attachment.storeAction = MTLStoreActionMultisampleResolve;
				attachment.depthResolveFilter = MTLMultisampleDepthResolveFilterMin;
			}
		}
		else
		{
			mDepthTexture = NULL;
		}
		
		if (config->stencilTexture != NULL || config->stencilTextureConfig != NULL)
		{
			if (config->stencilTexture != NULL)
			{
				mStencilTexture = config->stencilTexture;
			}
			else if (config->stencilTextureConfig != NULL)
			{
				config->stencilTextureConfig->usage = (Texture::eUsage)(config->stencilTextureConfig->usage | Texture::eUsage_RenderTarget);
				mStencilTexture = new qMetal::Texture(config->stencilTextureConfig, config->stencilTextureSamplerState);
			}
			
			const Texture::Config* textureConfig = mStencilTexture->GetConfig();
			
			MTLRenderPassStencilAttachmentDescriptor* attachment = renderPassDescriptor.stencilAttachment;
			attachment.texture = mStencilTexture->MTLTexture();
			attachment.loadAction = (MTLLoadAction)config->stencilClearAction;
			attachment.clearStencil = config->stencilClear;
			attachment.storeAction = (textureConfig != NULL && (textureConfig->storage == Texture::eStorage_Memoryless)) ? MTLStoreActionDontCare : MTLStoreActionStore;
			attachment.slice = config->slice;
			attachment.level = config->level;
		}
		else
		{
			mStencilTexture = NULL;
		}
	}
	
	RenderTarget::~RenderTarget()
	{
		[renderPassDescriptor release];
		renderPassDescriptor = nil;
	}
	
	id<MTLRenderCommandEncoder> RenderTarget::Begin()
	{
		qASSERTM(mEncoder == nil, "RenderTexture encoder is set; did you forget to call End()?");
		mEncoder = qMetal::Device::RenderEncoder(renderPassDescriptor, config->name);
		[mEncoder pushDebugGroup:config->name];
		return mEncoder;
	}
	
	void RenderTarget::End()
	{
		qASSERTM(mEncoder != nil, "RenderTexture encoder is nil; did you call Begin()?");
		[mEncoder popDebugGroup];
		[mEncoder endEncoding];
		mEncoder = nil;
	}
}
