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

#include "qMetal.h"

namespace qMetal
{
    namespace Device
    {   
        static bool                     sInited                 = false;
        static CAMetalLayer*            sMetalLayer             = NULL;
        static id <MTLDevice>           sDevice                 = nil;
        static id <MTLCommandQueue>     sCommandQueue           = nil;
        static RenderTarget*            sRenderTarget           = NULL;
        static RenderTarget::Config*    sRenderTargetConfig     = NULL;
		static dispatch_semaphore_t     sInflightSemaphore;
		static dispatch_semaphore_t     sSingleFrameSemaphore;
      
        //current frame
        static uint32_t                 sFrameIndex             = 0;
        static id<MTLCommandBuffer>     sCommandBuffer          = nil;
        static id<CAMetalDrawable>      sDrawable               = nil;
        
        void Init(CAMetalLayer *metalLayer)
        {
            sMetalLayer             = metalLayer;
            sMetalLayer.device      = Get();
            sMetalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
            //TODO?
            //_metalLayer.drawableSize = ???
            
            sCommandQueue           = [sDevice newCommandQueue];
            sCommandQueue.label 	= @"qMetal Command Queue";
            
            sRenderTargetConfig 	= new RenderTarget::Config(@"Framebuffer");
			
			//TODO do we need to clear?
            sRenderTargetConfig->clearAction[RenderTarget::eColorAttachment_0] = RenderTarget::eClearAction_Clear;
            sRenderTargetConfig->clearColour[RenderTarget::eColorAttachment_0] = qRGBA32f_White;
			
			sInflightSemaphore 		= dispatch_semaphore_create(Q_METAL_FRAMES_TO_BUFFER - 1);
			sSingleFrameSemaphore 	= dispatch_semaphore_create(0);
            
            sInited = true;
        }
      
        id <MTLDevice> Get()
        {
            if (sDevice == NULL)
            {
                sDevice = MTLCreateSystemDefaultDevice();
            }
            return sDevice;
        }
        
		id<MTLBlitCommandEncoder> BlitEncoder(NSString *label)
		{
			id<MTLBlitCommandEncoder> encoder = [sCommandBuffer blitCommandEncoder];
			encoder.label = label;
			return encoder;
		}
		
		id<MTLComputeCommandEncoder> ComputeEncoder(NSString *label)
		{
			id<MTLComputeCommandEncoder> encoder = [sCommandBuffer computeCommandEncoder];
			encoder.label = label;
			return encoder;
		}
		
		id<MTLRenderCommandEncoder> RenderEncoder(MTLRenderPassDescriptor *descriptor, NSString *label)
		{
			id<MTLRenderCommandEncoder> encoder = [sCommandBuffer renderCommandEncoderWithDescriptor:descriptor];
			encoder.label = label;
			return encoder;
		}
		
		void PushDebugGroup(NSString* label)
		{
		#if DEBUG
			[sCommandBuffer pushDebugGroup:label];
		#endif
		}
		
		void PopDebugGroup()
		{
		#if DEBUG
			[sCommandBuffer popDebugGroup];
		#endif
		}
		
        uint32_t CurrentFrameIndex()
        {
			return sFrameIndex;
		}
        
        void Destroy()
        {
            qASSERTM(sInited, "Device isn't inited");
        }
		
        void StartFrame()
        {
			qASSERTM(sCommandBuffer == nil, "Device CommandBuffer isn't nil; did you call Present()?")
            sCommandBuffer = [sCommandQueue commandBuffer];
            sCommandQueue.label = [NSString stringWithFormat:@"qMetal Command Buffer for frame %i", sFrameIndex];
		}
        
        id<MTLRenderCommandEncoder> Begin()
        {
            qASSERTM(sInited, "Device isn't inited");
            dispatch_semaphore_wait(sInflightSemaphore, DISPATCH_TIME_FOREVER);
			
            sDrawable = [sMetalLayer nextDrawable];
            
            //update our render target config with the current drawable colour texture, then create a render target (lightweight as no textures are allocated) to get our renderPassDescriptor / encoder
            //TODO this is a leak
			sRenderTargetConfig->colourTexture[RenderTarget::eColorAttachment_0] = new Texture([sDrawable texture], SamplerState::PredefinedStates[eSamplerState_PointPointNone_ClampClamp]);
            sRenderTarget = new RenderTarget(sRenderTargetConfig);
            
            return sRenderTarget->Begin();
        }
        
        void Present(bool blockUntilFrameComplete)
        {        
            qASSERTM(sInited, "Device isn't inited");
            sRenderTarget->End();
			
			__block dispatch_semaphore_t blockSemaphore = blockUntilFrameComplete ? sSingleFrameSemaphore : sInflightSemaphore;
			[sCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
//				NSTimeInterval CPUtime = buffer.kernelStartTime - buffer.kernelStartTime;
//				NSTimeInterval GPUtime = buffer.GPUEndTime - buffer.GPUStartTime;
//				NSLog(@"GPU: %0.3f, CPU: %0.3f", GPUtime * 1000.0, CPUtime * 1000.0); //TODO monitor
				dispatch_semaphore_signal(blockSemaphore);
			}];
            
            [sCommandBuffer presentDrawable:sDrawable];
            [sCommandBuffer commit];
            sCommandBuffer = NULL;
            sFrameIndex = (sFrameIndex + 1) % Q_METAL_FRAMES_TO_BUFFER;
            delete(sRenderTarget);
            sRenderTarget = NULL;
			
			if (blockUntilFrameComplete)
			{
				dispatch_semaphore_wait(sSingleFrameSemaphore, DISPATCH_TIME_FOREVER);
			}
        }
    }
}
