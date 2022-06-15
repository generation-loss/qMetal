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

#define Q_METAL_FRAMES_BETWEEN_PRINT 30

namespace qMetal
{
    namespace Device
    {
		static Config*						config							= NULL;
    			
        static bool                     	sInited                 		= false;
        static id <MTLDevice>           	sDevice                 		= nil;
        static id <MTLCommandQueue>     	sCommandQueue           		= nil;
        static RenderTarget*            	sRenderTarget           		= NULL;
        static RenderTarget::Config*    	sRenderTargetConfig     		= NULL;
		static dispatch_semaphore_t     	sInflightSemaphore;
		static dispatch_semaphore_t     	sSingleFrameSemaphore;
		
		//indirect command buffer pool
		static id<MTLIndirectCommandBuffer>	sIndirectCommandBuffer;
		static id<MTLBuffer>				sIndirectRangeBuffer;
		static id<MTLBuffer>				sIndirectLengthBuffer;
		static uint32_t						sNextIndirectRangeOffset = 0;
		static Function*					sIndirectResetFunction;
		static id<MTLComputePipelineState>	sIndirectResetComputePiplineState;
		static Function*					sIndirectInitFunction;
		static id<MTLComputePipelineState>	sIndirectInitComputePiplineState;
      
        //current frame
        static uint32_t                 	sFrameIndex            			= 0;
        static uint32_t						sFramePrintIndex				= 0;
        static id<MTLCommandBuffer>     	sCommandBuffer         			= nil;
        static id<CAMetalDrawable>      	sDrawable              			= nil;
        
        void Init(Config *_config)
        {
			config = _config;
        
			sDevice = MTLCreateSystemDefaultDevice();
                
            config->metalLayer.device = Get();
            config->metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
            
            sCommandQueue = [sDevice newCommandQueue];
            sCommandQueue.label = @"qMetal Command Queue";
            		
            sRenderTargetConfig = new RenderTarget::Config(@"Framebuffer");
			
			//TODO do we need to clear?
            sRenderTargetConfig->clearAction[RenderTarget::eColorAttachment_0] = RenderTarget::eClearAction_Clear;
            sRenderTargetConfig->clearColour[RenderTarget::eColorAttachment_0] = qRGBA32f_White;
			
			sInflightSemaphore = dispatch_semaphore_create(Q_METAL_FRAMES_TO_BUFFER - 1);
			sSingleFrameSemaphore = dispatch_semaphore_create(0);
			
			NSUInteger dimension = ceilf(sqrtf(config->maxIndirectCommands));
			config->maxIndirectCommands = dimension * dimension;
			
			qASSERTM(config->maxIndirectCommands == 0 || config->maxIndirectDrawRanges != 0, "Specified a value for one of maxIndirectCommands and maxIndirectDraw commands without sepecifying the other");
		
			if (config->maxIndirectCommands > 0)
			{
				sIndirectCommandBuffer = [qMetal::Device::Get() newIndirectCommandBufferWithDescriptor:config->indirectCommandBufferDescriptor maxCommandCount:config->maxIndirectCommands options:MTLResourceStorageModePrivate];
				sIndirectCommandBuffer.label = @"Global Indirect Command Buffer";
			}
			
			if (config->maxIndirectDrawRanges > 0)
			{
				sIndirectRangeBuffer = [qMetal::Device::Get() newBufferWithLength:(sizeof(MTLIndirectCommandBufferExecutionRange) * config->maxIndirectDrawRanges) options:MTLResourceStorageModePrivate];
				sIndirectRangeBuffer.label = @"Global Indirect Range Buffer";
			}
			
			{
				NSError* error;
				
				sIndirectLengthBuffer = nil;
            
				sNextIndirectRangeOffset = 0;
            
				sIndirectResetFunction = new Function(@"qMetalIndirectResetShader");
				sIndirectResetComputePiplineState = [qMetal::Device::Get() newComputePipelineStateWithFunction:sIndirectResetFunction->Get()
																									 error:&error];
				qASSERT(sIndirectResetComputePiplineState != nil);
            
				sIndirectInitFunction = new Function(@"qMetalIndirectInitShader");
				sIndirectInitComputePiplineState = [qMetal::Device::Get() newComputePipelineStateWithFunction:sIndirectInitFunction->Get()
																									 error:&error];
				qASSERT(sIndirectInitComputePiplineState != nil);
			}
            
            sInited = true;
        }
      
        id <MTLDevice> Get()
        {
            return sDevice;
        }
        
		id<MTLBlitCommandEncoder> BlitEncoder(NSString *label)
		{
			qASSERTM(sCommandBuffer != nil, "Device CommandBuffer is nil; did you call BeginOffScreen()/BeginRenderable()?")
			id<MTLBlitCommandEncoder> encoder = [sCommandBuffer blitCommandEncoder];
			encoder.label = label;
			return encoder;
		}
		
		id<MTLComputeCommandEncoder> ComputeEncoder(NSString *label)
		{
			qASSERTM(sCommandBuffer != nil, "Device CommandBuffer is nil; did you call BeginOffScreen()/BeginRenderable()?")
			id<MTLComputeCommandEncoder> encoder = [sCommandBuffer computeCommandEncoder];
			encoder.label = label;
			return encoder;
		}
		
		id<MTLRenderCommandEncoder> RenderEncoder(MTLRenderPassDescriptor *descriptor, NSString *label)
		{
			qASSERTM(sCommandBuffer != nil, "Device CommandBuffer is nil; did you call BeginOffScreen()/BeginRenderable()?")
			id<MTLRenderCommandEncoder> encoder = [sCommandBuffer renderCommandEncoderWithDescriptor:descriptor];
			encoder.label = label;
			return encoder;
		}
		
		void PushDebugGroup(NSString* label)
		{
		#if DEBUG
			qASSERTM(sCommandBuffer != nil, "Device CommandBuffer is nil; did you call BeginOffScreen()/BeginRenderable()?")
			[sCommandBuffer pushDebugGroup:label];
		#endif
		}
		
		void PopDebugGroup()
		{
		#if DEBUG
			qASSERTM(sCommandBuffer != nil, "Device CommandBuffer is nil; did you call BeginOffScreen()/BeginRenderable()?")
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
		
        void BeginOffScreen()
        {
            qASSERTM(sInited, "Device isn't inited");
			qASSERTM(sCommandBuffer == nil, "Device CommandBuffer isn't nil; did you call EndOffScreen()?");
			
            sCommandBuffer = [[sCommandQueue commandBuffer] retain];
            sCommandBuffer.label = [NSString stringWithFormat:@"qMetal Off Screen Command Buffer for frame %i", sFrameIndex];
            
            ResetIndirectCommandBuffer(); //TODO make sure this is only called once per frame
		}
		
		void EndOffScreen()
		{
            qASSERTM(sInited, "Device isn't inited");
			qASSERTM(sCommandBuffer != nil, "Device CommandBuffer is nil; did you call StartOffScreen()?");
			
			#if DEBUG
			if (sFramePrintIndex == 0)
			{
				[sCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
					NSTimeInterval CPUtime = buffer.kernelEndTime - buffer.kernelStartTime;
					NSTimeInterval GPUtime = buffer.GPUEndTime - buffer.GPUStartTime;
					NSLog(@"Offscreen GPU: %0.3f, CPU: %0.3f", GPUtime * 1000.0, CPUtime * 1000.0); //TODO monitor
				}];
			}
			#endif
			
            [sCommandBuffer commit];
            [sCommandBuffer release];
            sCommandBuffer = nil;
		}
        
        id<MTLRenderCommandEncoder> BeginDrawable()
        {
            qASSERTM(sInited, "Device isn't inited");
			qASSERTM(sCommandBuffer == nil, "Device CommandBuffer isn't nil; did you call EndOffScreen()?");
			
            sCommandBuffer = [[sCommandQueue commandBuffer] retain];
            sCommandBuffer.label = [NSString stringWithFormat:@"qMetal Drawable Command Buffer for frame %i", sFrameIndex];
            
            dispatch_semaphore_wait(sInflightSemaphore, DISPATCH_TIME_FOREVER);
			
            sDrawable = [config->metalLayer nextDrawable];
            
            //update our render target config with the current drawable colour texture, then create a render target (lightweight as no textures are allocated) to get our renderPassDescriptor / encoder
            //TODO this is a leak
			sRenderTargetConfig->colourTexture[RenderTarget::eColorAttachment_0] = new Texture([sDrawable texture], SamplerState::PredefinedState(eSamplerState_PointPointNone_ClampClamp));
            sRenderTarget = new RenderTarget(sRenderTargetConfig);
            
            return sRenderTarget->Begin();
        }
        
        void EndAndPresentDrawable(CFTimeInterval afterMinimumDuration, bool blockUntilFrameComplete)
        {        
            qASSERTM(sInited, "Device isn't inited");
			qASSERTM(sCommandBuffer != nil, "Device CommandBuffer is nil; did you call StartOffScreen()?");
			
            sRenderTarget->End();
			
			__block dispatch_semaphore_t blockSemaphore = blockUntilFrameComplete ? sSingleFrameSemaphore : sInflightSemaphore;
			[sCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
				#if DEBUG
				if (sFramePrintIndex == 0)
				{
					NSTimeInterval CPUtime = buffer.kernelEndTime - buffer.kernelStartTime;
					NSTimeInterval GPUtime = buffer.GPUEndTime - buffer.GPUStartTime;
					NSLog(@"Onscreen GPU: %0.3f, CPU: %0.3f", GPUtime * 1000.0, CPUtime * 1000.0); //TODO monitor
				}
				sFramePrintIndex = (sFramePrintIndex + 1) % Q_METAL_FRAMES_BETWEEN_PRINT;
				#endif
				dispatch_semaphore_signal(blockSemaphore);
			}];
            
            [sCommandBuffer presentDrawable:sDrawable afterMinimumDuration:afterMinimumDuration];
            [sCommandBuffer commit];
            [sCommandBuffer release];
            sCommandBuffer = nil;
            sFrameIndex = (sFrameIndex + 1) % Q_METAL_FRAMES_TO_BUFFER;
            delete(sRenderTarget);
            sRenderTarget = NULL;
			
			if (blockUntilFrameComplete)
			{
				dispatch_semaphore_wait(sSingleFrameSemaphore, DISPATCH_TIME_FOREVER);
			}
        }
        
        id<MTLIndirectCommandBuffer> IndirectCommandBuffer()
        {
			return sIndirectCommandBuffer;
		}
        
        id<MTLBuffer> IndirectRangeBuffer()
        {
			return sIndirectRangeBuffer;
		}
		
        id<MTLBuffer> IndirectRangeLengthBuffer()
        {
			assert(sNextIndirectRangeOffset != 0);
			assert(sIndirectLengthBuffer != nil);
			return sIndirectLengthBuffer;
		}
        
        uint32_t NextIndirectRangeOffset()
        {
			assert(sNextIndirectRangeOffset < config->maxIndirectDrawRanges);
			
			//Update indirect length buffer
			[sIndirectLengthBuffer release];
			sIndirectLengthBuffer = [qMetal::Device::Get() newBufferWithBytes:&sNextIndirectRangeOffset length:sizeof(sNextIndirectRangeOffset) options:0];
			sIndirectLengthBuffer.label = @"Global Indirect Length Buffer";
			
			return sNextIndirectRangeOffset++;
		}
        
        void ResetIndirectCommandBuffer()
        {
			id<MTLComputeCommandEncoder> encoder =  ComputeEncoder(@"Indirect Command Buffer Reset");
			[encoder setComputePipelineState:sIndirectResetComputePiplineState];
			[encoder setBuffer:IndirectRangeBuffer() offset:0 atIndex:0];
			[encoder setBuffer:IndirectRangeLengthBuffer() offset:0 atIndex:1];
			[encoder dispatchThreads:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
			[encoder endEncoding];
		}
        
        void InitIndirectCommandBuffer(id<MTLComputeCommandEncoder> encoder, id<MTLBuffer> rangeOffsetBuffer)
        {
			[encoder pushDebugGroup:@"Indirect Command Buffer Init"];
			[encoder setComputePipelineState:sIndirectInitComputePiplineState];
			[encoder setBuffer:IndirectRangeBuffer() 		offset:0 atIndex:0];
			[encoder setBuffer:IndirectRangeLengthBuffer() 	offset:0 atIndex:1];
			[encoder setBuffer:rangeOffsetBuffer 			offset:0 atIndex:2];
			[encoder dispatchThreads:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
			[encoder popDebugGroup];
		}
		
		void ExecuteIndirectCommandBuffer(id<MTLRenderCommandEncoder> encoder, uint32_t indirectRangeOffset)
		{
			[encoder executeCommandsInBuffer:IndirectCommandBuffer() indirectBuffer:IndirectRangeBuffer() indirectBufferOffset:(indirectRangeOffset * sizeof(MTLIndirectCommandBufferExecutionRange))];
		}
    }
}
