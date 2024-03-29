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

#ifndef __Q_METAL_DEVICE_H__
#define __Q_METAL_DEVICE_H__

#include <QuartzCore/CAMetalLayer.h>
#include <Metal/Metal.h>

#define Q_METAL_FRAMES_TO_BUFFER (3)

namespace qMetal
{
    namespace Device
    {
		enum eIndirectCommandBufferPool
		{
			eIndirectCommandBufferPool_Untessellated,
			eIndirectCommandBufferPool_Tessellated,
			eIndirectCommandBufferPool_Count,
		};
    
		struct Config
		{
			typedef struct IndirectCommandBufferPoolConfig
			{
				NSUInteger maxIndirectCommands; //TODO upload max and do out of bounds access if it's out of range
				NSUInteger maxIndirectDrawRanges;
				MTLIndirectCommandBufferDescriptor* indirectCommandBufferDescriptor;
				
				IndirectCommandBufferPoolConfig()
				: maxIndirectCommands(0)
				, maxIndirectDrawRanges(0)
				, indirectCommandBufferDescriptor(nil)
				{ }
			} IndirectCommandBufferPoolConfig;
			
			CAMetalLayer* metalLayer;
			IndirectCommandBufferPoolConfig commandBufferPoolConfig[eIndirectCommandBufferPool_Count];
			
			Config()
			: metalLayer(NULL)
			{
			}
		};
    
        void Init(Config* config);
        void Destroy();
      
        id<MTLDevice> Get();

		id<MTLBlitCommandEncoder> BlitEncoder(NSString* label);
		id<MTLComputeCommandEncoder> ComputeEncoder(NSString* label);
		id<MTLRenderCommandEncoder> RenderEncoder(MTLRenderPassDescriptor* descriptor, NSString* label);
		
		void PushDebugGroup(NSString* name);
		void PopDebugGroup();
        
        uint32_t CurrentFrameIndex();
		
        void BeginOffScreen();
        void EndOffScreen();
        id<MTLRenderCommandEncoder> BeginDrawable();
        void EndAndPresentDrawable(CFTimeInterval afterMinimumDuration, bool blockUntilFrameComplete = false);
        
        id<MTLIndirectCommandBuffer> IndirectCommandBuffer(eIndirectCommandBufferPool pool);
        id<MTLBuffer> IndirectRangeBuffer(eIndirectCommandBufferPool pool);
        id<MTLBuffer> IndirectRangeLengthBuffer(eIndirectCommandBufferPool pool);
        uint32_t NextIndirectRangeOffset(eIndirectCommandBufferPool pool);
                
		void ResetIndirectCommandBuffers();
		void InitIndirectCommandBuffer(eIndirectCommandBufferPool pool, id<MTLComputeCommandEncoder> encoder, id<MTLBuffer> rangeOffsetBuffer);
		void ExecuteIndirectCommandBuffer(eIndirectCommandBufferPool pool, id<MTLRenderCommandEncoder> encoder, uint32_t indirectRangeOffset);
    }
}

#endif //__Q_METAL_DEVICE_H__

