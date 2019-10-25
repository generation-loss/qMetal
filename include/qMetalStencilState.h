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

#ifndef __Q_METAL_STENCILSTATE_H__
#define __Q_METAL_STENCILSTATE_H__

#include <Metal/Metal.h>

#define STENCIL_STATES \
/*				enum        	test        pass        fail        zfail       read mask       write mask*/ \
STENCIL_STATE(	Disable,		Disable,	Keep,       Keep,       Keep,       0xffffffff,     0xffffffff) \
STENCIL_STATE(	Replace,		Always,		Replace,	Keep,       Keep,       0xffffffff,     0xffffffff) \
STENCIL_STATE(	CompareEqual,	Equal,		Keep,		Keep,       Keep,       0xffffffff,     0xffffffff) \

namespace qMetal
{
    enum eStencilState
    {
#define STENCIL_STATE(xxenum, xxstenciltest, xxpass, xxfail, xxzfail, xxreadmask, xxwritemask) eStencilState_ ## xxenum,
        STENCIL_STATES
#undef STENCIL_STATE   
        eStencilState_Count,
    };
    
    struct StencilState
    {
        enum eStencilTest
        {
            eStencilTest_Disable        = MTLCompareFunctionAlways,
            eStencilTest_Always         = MTLCompareFunctionAlways,
            eStencilTest_Never          = MTLCompareFunctionNever,
            eStencilTest_Equal          = MTLCompareFunctionEqual,
            eStencilTest_Less           = MTLCompareFunctionLess,
            eStencilTest_LessEqual      = MTLCompareFunctionLessEqual,
            eStencilTest_Greater        = MTLCompareFunctionGreater,
            eStencilTest_GreaterEqual   = MTLCompareFunctionGreaterEqual,
            eStencilTest_NotEqual       = MTLCompareFunctionNotEqual,
        };
      
        enum eStencilOperation
        {
            eStencilOperation_Keep           = MTLStencilOperationKeep,
            eStencilOperation_Replace        = MTLStencilOperationReplace,
            eStencilOperation_Zero           = MTLStencilOperationZero,
            eStencilOperation_Invert         = MTLStencilOperationInvert,
            eStencilOperation_IncrementClamp = MTLStencilOperationIncrementClamp,
            eStencilOperation_DecrementClamp = MTLStencilOperationDecrementClamp,
            eStencilOperation_IncrementWrap  = MTLStencilOperationIncrementWrap,
            eStencilOperation_DecrementWrap  = MTLStencilOperationDecrementWrap,
        };
      
        eStencilTest            stencilTest;
        eStencilOperation       stencilPass;
        eStencilOperation       stencilFail;
        eStencilOperation       depthFail;
        uint                    readMask;
        uint                    writeMask;
        MTLStencilDescriptor    *descriptor;
        
        StencilState(
                   eStencilTest _stencilTest,
                   eStencilOperation _stencilPass,
                   eStencilOperation _stencilFail,
                   eStencilOperation _depthFail,
                   uint32_t _readMask = 0xffffffff,
                   uint32_t _writeMask = 0xffffffff);
				   
	   static StencilState *PredefinedStates[eStencilState_Count];
    };
}

#endif //__Q_METAL_STENCILSTATE_H__

