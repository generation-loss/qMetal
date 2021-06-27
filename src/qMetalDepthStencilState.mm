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

#include "qMetalDepthStencilState.h"

namespace qMetal
{        
    DepthStencilState *DepthStencilState::PredefinedStates[eDepthStencilState_Count] = {
#define DEPTHSTENCIL_STATE(xxenum, xxdepthtest, xxdepthwrite, xxstencil) \
    new DepthStencilState(DepthStencilState::eDepthTest_ ## xxdepthtest, \
    DepthStencilState::eDepthWrite_ ## xxdepthwrite, \
    eStencilState_ ## xxstencil),
        DEPTHSTENCIL_STATES
#undef DEPTHSTENCIL_STATE
    };
        
	DepthStencilState::DepthStencilState(
			   eDepthTest _depthTest,
			   eDepthWrite _depthWrite,
			   eStencilState _stencil)
	: DepthStencilState::DepthStencilState(_depthTest, _depthWrite, _stencil, _stencil)
	{
	}
	
	DepthStencilState::DepthStencilState(
			   eDepthTest _depthTest,
			   eDepthWrite _depthWrite,
			   eStencilState _frontStencil,
			   eStencilState _backStencil)
	: depthTest(_depthTest)
	, depthWrite(_depthWrite)
	, frontStencil(StencilState::PredefinedState(_frontStencil))
	, backStencil(StencilState::PredefinedState(_backStencil))
	{
		descriptor = [[MTLDepthStencilDescriptor alloc] init];
		descriptor.depthWriteEnabled = depthWrite == eDepthWrite_Enable;
		descriptor.depthCompareFunction = (MTLCompareFunction)depthTest;
		descriptor.frontFaceStencil = frontStencil->descriptor;
		descriptor.backFaceStencil = backStencil->descriptor;
		
		state = [qMetal::Device::Get() newDepthStencilStateWithDescriptor:descriptor];
	}
	
	void DepthStencilState::Encode(id<MTLRenderCommandEncoder> encoder)
	{
	  [encoder setDepthStencilState:state];
	}
}
