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
		
		typedef struct IndirectCommandBufferPool
		{
			id<MTLIndirectCommandBuffer>	indirectCommandBuffer;
			id<MTLBuffer>					indirectRangeBuffer;
			id<MTLBuffer>					indirectLengthBuffer;
			uint32_t						nextIndirectRangeOffset = 0;
		} IndirectCommandBufferPool;
		
		static IndirectCommandBufferPool	sIndirectCommandBufferPool[eIndirectCommandBufferPool_Count];
		
		static Function*					sIndirectResetFunction;
		static id<MTLComputePipelineState>	sIndirectResetComputePiplineState;
		static Function*					sIndirectInitFunction;
		static id<MTLComputePipelineState>	sIndirectInitComputePiplineState;
      
        //current frame
        static uint32_t                 	sFrameIndex            			= 0;
        static uint32_t						sFramePrintIndex				= 0;
        static id<MTLCommandBuffer>     	sCommandBuffer         			= nil;
        static id<CAMetalDrawable>      	sDrawable              			= nil;
        
        void Init(Config* _config)
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
			
			for(uint32_t poolIndex = 0; poolIndex < eIndirectCommandBufferPool_Count; ++poolIndex)
			{
				Config::IndirectCommandBufferPoolConfig& poolConfig = config->commandBufferPoolConfig[poolIndex];
				if (poolConfig.indirectCommandBufferDescriptor == NULL)
				{
					continue;
				}
			
				NSUInteger dimension = ceilf(sqrtf(poolConfig.maxIndirectCommands));
				poolConfig.maxIndirectCommands = dimension * dimension;
				
				qASSERTM(poolConfig.maxIndirectCommands == 0 || poolConfig.maxIndirectDrawRanges != 0, "Specified a value for one of maxIndirectCommands and maxIndirectDraw commands without sepecifying the other");
			
				if (poolConfig.maxIndirectCommands > 0)
				{
					sIndirectCommandBufferPool[poolIndex].indirectCommandBuffer = [qMetal::Device::Get() newIndirectCommandBufferWithDescriptor:poolConfig.indirectCommandBufferDescriptor maxCommandCount:poolConfig.maxIndirectCommands options:MTLResourceStorageModePrivate];
					sIndirectCommandBufferPool[poolIndex].indirectCommandBuffer.label = [NSString stringWithFormat:@"Global Indirect Command Buffer for pool %i", poolIndex];
				}
				
				if (poolConfig.maxIndirectDrawRanges > 0)
				{
					sIndirectCommandBufferPool[poolIndex].indirectRangeBuffer = [qMetal::Device::Get() newBufferWithLength:(sizeof(MTLIndirectCommandBufferExecutionRange) * poolConfig.maxIndirectDrawRanges) options:MTLResourceStorageModePrivate];
					sIndirectCommandBufferPool[poolIndex].indirectRangeBuffer.label = [NSString stringWithFormat:@"Global Indirect Range Buffer for pool %i", poolIndex];
				}
				
				sIndirectCommandBufferPool[poolIndex].indirectLengthBuffer = nil;
            
				sIndirectCommandBufferPool[poolIndex].nextIndirectRangeOffset = 0;
			}
			
			{
				NSError* error;
            
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
        
		id<MTLBlitCommandEncoder> BlitEncoder(NSString* label)
		{
			qASSERTM(sCommandBuffer != nil, "Device CommandBuffer is nil; did you call BeginOffScreen()/BeginRenderable()?")
			id<MTLBlitCommandEncoder> encoder = [sCommandBuffer blitCommandEncoder];
			encoder.label = label;
			return encoder;
		}
		
		id<MTLComputeCommandEncoder> ComputeEncoder(NSString* label)
		{
			qASSERTM(sCommandBuffer != nil, "Device CommandBuffer is nil; did you call BeginOffScreen()/BeginRenderable()?")
			id<MTLComputeCommandEncoder> encoder = [sCommandBuffer computeCommandEncoder];
			encoder.label = label;
			return encoder;
		}
		
		id<MTLRenderCommandEncoder> RenderEncoder(MTLRenderPassDescriptor* descriptor, NSString* label)
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
            
            ResetIndirectCommandBuffers(); //TODO make sure this is only called once per frame
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
        
        id<MTLIndirectCommandBuffer> IndirectCommandBuffer(eIndirectCommandBufferPool pool)
        {
			return sIndirectCommandBufferPool[pool].indirectCommandBuffer;
		}
        
        id<MTLBuffer> IndirectRangeBuffer(eIndirectCommandBufferPool pool)
        {
			return sIndirectCommandBufferPool[pool].indirectRangeBuffer;
		}
		
        id<MTLBuffer> IndirectRangeLengthBuffer(eIndirectCommandBufferPool pool)
        {
			assert(sIndirectCommandBufferPool[pool].nextIndirectRangeOffset != 0);
			assert(sIndirectCommandBufferPool[pool].indirectLengthBuffer != nil);
			return sIndirectCommandBufferPool[pool].indirectLengthBuffer;
		}
        
        uint32_t NextIndirectRangeOffset(eIndirectCommandBufferPool pool)
        {
			assert(sIndirectCommandBufferPool[pool].nextIndirectRangeOffset < config->commandBufferPoolConfig[pool].maxIndirectDrawRanges);
			
			//Update indirect length buffer
			[sIndirectCommandBufferPool[pool].indirectLengthBuffer release];
			sIndirectCommandBufferPool[pool].indirectLengthBuffer = [qMetal::Device::Get() newBufferWithBytes:&sIndirectCommandBufferPool[pool].nextIndirectRangeOffset length:sizeof(sIndirectCommandBufferPool[pool].nextIndirectRangeOffset) options:0];
			sIndirectCommandBufferPool[pool].indirectLengthBuffer.label = [NSString stringWithFormat:@"Global Indirect Length Buffer for pool %i", pool];
			
			return sIndirectCommandBufferPool[pool].nextIndirectRangeOffset++;
		}
        
        void ResetIndirectCommandBuffers()
        {
			id<MTLComputeCommandEncoder> encoder =  ComputeEncoder(@"Indirect Command Buffer Reset");
			[encoder setComputePipelineState:sIndirectResetComputePiplineState];
			for(uint32_t poolIndex = 0; poolIndex < eIndirectCommandBufferPool_Count; ++poolIndex)
			{
				if( (config->commandBufferPoolConfig[poolIndex].indirectCommandBufferDescriptor == NULL) || (sIndirectCommandBufferPool[poolIndex].nextIndirectRangeOffset == 0) )
				{
					continue;
				}
				eIndirectCommandBufferPool pool = (eIndirectCommandBufferPool)poolIndex;
				[encoder setBuffer:IndirectRangeBuffer(pool) offset:0 atIndex:0];
				[encoder setBuffer:IndirectRangeLengthBuffer(pool) offset:0 atIndex:1];
				[encoder dispatchThreads:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
			}
			[encoder endEncoding];
		}
        
        void InitIndirectCommandBuffer(eIndirectCommandBufferPool pool, id<MTLComputeCommandEncoder> encoder, id<MTLBuffer> rangeOffsetBuffer)
        {
			[encoder pushDebugGroup:@"Indirect Command Buffer Init"];
			[encoder setComputePipelineState:sIndirectInitComputePiplineState];
			[encoder setBuffer:IndirectRangeBuffer(pool) 		offset:0 atIndex:0];
			[encoder setBuffer:IndirectRangeLengthBuffer(pool) 	offset:0 atIndex:1];
			[encoder setBuffer:rangeOffsetBuffer 				offset:0 atIndex:2];
			[encoder dispatchThreads:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
			[encoder popDebugGroup];
		}
		
		void ExecuteIndirectCommandBuffer(eIndirectCommandBufferPool pool, id<MTLRenderCommandEncoder> encoder, uint32_t indirectRangeOffset)
		{
			[encoder executeCommandsInBuffer:IndirectCommandBuffer(pool) indirectBuffer:IndirectRangeBuffer(pool) indirectBufferOffset:(indirectRangeOffset * sizeof(MTLIndirectCommandBufferExecutionRange))];
		}
    }
}
