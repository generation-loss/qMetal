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

#ifndef __Q_METAL_RENDER_TARGET_H__
#define __Q_METAL_RENDER_TARGET_H__

#include <Metal/Metal.h>
#include "qMetalTexture.h"

namespace qMetal
{
	class RenderTarget
	{
	public:
		RenderTarget();
		~RenderTarget();
		
		enum eColorAttachment
		{
			//TODO define based on implementation MRT limit
			eColorAttachment_0 = 0,
			eColorAttachment_1 = 1,
			eColorAttachment_2 = 2,
			eColorAttachment_3 = 3,
			eColorAttachment_4 = 4,
			eColorAttachment_5 = 5,
			eColorAttachment_6 = 6,
			eColorAttachment_7 = 7,
			eColorAttachment_8 = 8,
			eColorAttachment_Count = eColorAttachment_8
		};
		
		enum eClearAction
		{
			eClearAction_Clear      = MTLLoadActionClear,
			eClearAction_Load       = MTLLoadActionLoad,
			eClearAction_Nothing    = MTLLoadActionDontCare
		};
		
		struct Config
		{
			Config(NSString *_name)
			: name([_name retain])
			, colorAttachmentCount(eColorAttachment_1)
			, depthTextureConfig(NULL)
			, depthTextureSamplerState(NULL)
			, depthTexture(NULL)
			, depthResolveTextureConfig(NULL)
			, depthResolveTextureSamplerState(NULL)
			, depthResolveTexture(NULL)
			, depthClear(1.0)
			, depthClearAction(eClearAction_Clear)
			, stencilTextureConfig(NULL)
			, stencilTextureSamplerState(NULL)
			, stencilClearAction(eClearAction_Clear)
			, stencilTexture(NULL)
			, stencilClear(0)
			, slice(0)
			, level(0)
			{
				memset(colourTextureConfig, 0, sizeof(colourTextureConfig));
				memset(colourTexture, 0, sizeof(colourTexture));
				memset(colourTextureSamplerState, 0, sizeof(colourTextureSamplerState));
				memset(colourResolveTextureConfig, 0, sizeof(colourResolveTextureConfig));
				memset(colourResolveTexture, 0, sizeof(colourResolveTexture));
				memset(colourResolveTextureSamplerState, 0, sizeof(colourResolveTextureSamplerState));
				for (int i = 0; i < eColorAttachment_Count; ++i)
				{
					clearAction[i] = eClearAction_Nothing;
				}
			}
			
			Config(Config* config, NSString *name)
            : name(name)
			, colorAttachmentCount(config->colorAttachmentCount)
			, depthTextureConfig(config->depthTextureConfig)
			, depthTextureSamplerState(config->depthTextureSamplerState)
			, depthTexture(config->depthTexture)
			, depthResolveTextureConfig(config->depthResolveTextureConfig)
			, depthResolveTextureSamplerState(config->depthResolveTextureSamplerState)
			, depthResolveTexture(config->depthResolveTexture)
			, depthClear(config->depthClear)
			, depthClearAction(config->depthClearAction)
			, stencilTextureConfig(config->stencilTextureConfig)
			, stencilTextureSamplerState(config->stencilTextureSamplerState)
			, stencilClearAction(config->stencilClearAction)
			, stencilTexture(config->stencilTexture)
			, stencilClear(config->stencilClear)
			, slice(config->slice)
			, level(config->level)
			{
				memcpy(colourTextureConfig, config->colourTextureConfig, sizeof(colourTextureConfig));
				memcpy(colourTexture, config->colourTexture, sizeof(colourTexture));
				memcpy(colourTextureSamplerState, config->colourTextureSamplerState, sizeof(colourTextureSamplerState));
				memcpy(colourResolveTextureConfig, config->colourResolveTextureConfig, sizeof(colourResolveTextureConfig));
				memcpy(colourResolveTexture, config->colourResolveTexture, sizeof(colourResolveTexture));
				memcpy(colourResolveTextureSamplerState, config->colourResolveTextureSamplerState, sizeof(colourResolveTextureSamplerState));
				for (int i = 0; i < eColorAttachment_Count; ++i)
				{
					clearAction[i] = config->clearAction[i];
				}
			}
			
			NSString *name;
			
			//number of color attachments
			eColorAttachment colorAttachmentCount;
			
			//colour attachments
			Texture::Config *colourTextureConfig[eColorAttachment_Count];
			Texture *colourTexture[eColorAttachment_Count];
			SamplerState *colourTextureSamplerState[eColorAttachment_Count];
			
			//colour resolve attachments (if colour has MSAA)
			Texture::Config *colourResolveTextureConfig[eColorAttachment_Count];
			Texture *colourResolveTexture[eColorAttachment_Count];
			SamplerState *colourResolveTextureSamplerState[eColorAttachment_Count];
			
			//depth
			Texture::Config *depthTextureConfig;
			SamplerState *depthTextureSamplerState;
			Texture *depthTexture;
			
			//depth resolve texture (if depth has MSAA)
			Texture::Config *depthResolveTextureConfig;
			SamplerState *depthResolveTextureSamplerState;
			Texture *depthResolveTexture;
			
			//stencil
			Texture::Config *stencilTextureConfig;
			SamplerState *stencilTextureSamplerState;
			Texture *stencilTexture;
			
			qRGBA32f clearColour[eColorAttachment_Count];
			double depthClear;
			uint32_t stencilClear;
			
			eClearAction clearAction[eColorAttachment_Count];
			eClearAction depthClearAction;
			eClearAction stencilClearAction;
			
			NSUInteger slice;
			NSUInteger level;
		};
		
		RenderTarget(const Config *config);
		
		id<MTLRenderCommandEncoder> Begin();
		void End();
		
		Texture* ColourTexture(eColorAttachment attachment) const { return mColourTexture[(int)attachment]; }
		Texture* ColourResolveTexture(eColorAttachment attachment) const { return mColourResolveTexture[(int)attachment]; }
		Texture* DepthTexture() const { return mDepthTexture; }
		Texture* DepthResolveTexture() const { return mDepthResolveTexture; }
		Texture* StencilTexture() const { return mStencilTexture; }
		
		const Config* GetConfig() const { return config; }
		const Texture::ePixelFormat* GetPixelFormat() const { return &mColourTexturePixelFormat[0]; }
		
	private:
		const Config              	*config;
		MTLRenderPassDescriptor   	*renderPassDescriptor;
		
		Texture           			*mColourTexture[eColorAttachment_Count];
		Texture           			*mColourResolveTexture[eColorAttachment_Count];
		Texture::ePixelFormat		mColourTexturePixelFormat[eColorAttachment_Count];
		Texture           			*mDepthTexture;
		Texture           			*mDepthResolveTexture;
		Texture           			*mStencilTexture;
		
		//current encoder
		id<MTLRenderCommandEncoder> mEncoder;
	};
}

#endif //__Q_METAL_RENDER_TARGET_H__

