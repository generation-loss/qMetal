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

#ifndef __Q_METAL_DEPTHSTENCILSTATE_H__
#define __Q_METAL_DEPTHSTENCILSTATE_H__

#include <Metal/Metal.h>
#include "qMetalDevice.h"
#include "qMetalStencilState.h"

#define DEPTHSTENCIL_STATES \
/*                  	enum                        						depth test      depth write,    stencil			*/ \
DEPTHSTENCIL_STATE(		TestDisable_WriteDisable_StencilDisable,   			Disable,        Disable,        Disable 		) \
DEPTHSTENCIL_STATE(		TestDisable_WriteDisable_StencilCompareEqual,		Disable,        Disable,        CompareEqual 	) \
DEPTHSTENCIL_STATE(		TestDisable_WriteEnable_StencilDisable,   			Always,         Enable,         Disable 		) \
DEPTHSTENCIL_STATE(		TestLessEqual_WriteEnable_StencilDisable,  			LessEqual,      Enable,         Disable 		) \
DEPTHSTENCIL_STATE(		TestLessEqual_WriteDisable_StencilDisable,  		LessEqual,      Disable,		Disable 		) \
DEPTHSTENCIL_STATE(		TestLessEqual_WriteEnable_StencilReplace,  			LessEqual,      Enable,         Replace 		) \
DEPTHSTENCIL_STATE(		TestEqual_WriteDisable_StencilDisable,  			Equal,     		Disable,		Disable 		) \

namespace qMetal
{
    enum eDepthStencilState
    {
#define DEPTHSTENCIL_STATE(xxenum, xxdepthtest, xxdepthwrite, xxstencil) eDepthStencilState_ ## xxenum,
        DEPTHSTENCIL_STATES
#undef DEPTHSTENCIL_STATE   
        eDepthStencilState_Count,
    };
    
    struct DepthStencilState
    {
       enum eDepthTest
        {
            eDepthTest_Disable      = MTLCompareFunctionAlways,
            eDepthTest_Always       = MTLCompareFunctionAlways,
            eDepthTest_Never        = MTLCompareFunctionNever,
            eDepthTest_Equal        = MTLCompareFunctionEqual,
            eDepthTest_Less         = MTLCompareFunctionLess,
            eDepthTest_LessEqual    = MTLCompareFunctionLessEqual,
            eDepthTest_Greater      = MTLCompareFunctionGreater,
            eDepthTest_GreaterEqual = MTLCompareFunctionGreaterEqual,
            eDepthTest_NotEqual     = MTLCompareFunctionNotEqual,
        };
        
        enum eDepthWrite
        {
            eDepthWrite_Disable     = 0,
            eDepthWrite_Enable      = 1,
        };
        
        eDepthTest                  depthTest;
        eDepthWrite                 depthWrite;
        StencilState*               frontStencil;
        StencilState*               backStencil;
        MTLDepthStencilDescriptor*  descriptor;
        id<MTLDepthStencilState>    state;
        
        DepthStencilState(
			eDepthTest _depthTest,
			eDepthWrite _depthWrite,
			eStencilState _stencil);
        
        DepthStencilState(
			eDepthTest _depthTest,
			eDepthWrite _depthWrite,
			eStencilState _frontStencil,
			eStencilState _backStencil);
		
        void Encode(id<MTLRenderCommandEncoder> encoder);
    
        static DepthStencilState *PredefinedState(eDepthStencilState state);
    };
}

#endif //__Q_METAL_DEPTHSTENCILSTATE_H__

