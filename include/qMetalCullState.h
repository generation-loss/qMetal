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

#ifndef __Q_METAL_CULLSTATE_H__
#define __Q_METAL_CULLSTATE_H__

#include <Metal/Metal.h>

#define CULL_STATES \
/*              enum        winding         cull face */ \
CULL_STATE(    Disable,    CW,             None   ) \
CULL_STATE(    CW,         CCW,            Back   ) \
CULL_STATE(    CCW,        CW,             Back   )

namespace qMetal
{    
    enum eCullState
    {
#define CULL_STATE(xxenum, xxwinding, xxcullface) eCullState_ ## xxenum,
        CULL_STATES
#undef CULL_STATE
        eCullState_Count,
    };
    
    struct CullState
    {        
        enum eCullWinding
        {
            eFrontFace_CW           = MTLWindingClockwise,
            eFrontFace_CCW          = MTLWindingCounterClockwise,
        };
        
        enum eCullFace
        {
            eCullFace_Front         = MTLCullModeFront,
            eCullFace_Back          = MTLCullModeBack,
            eCullFace_None          = MTLCullModeNone,
        };
        
        eCullWinding    winding;
        eCullFace       face;
        
        CullState(
			eCullWinding    _winding,
			eCullFace       _face);
      
        void Encode(id<MTLRenderCommandEncoder> encoder);
        
        static CullState* PredefinedState(eCullState state);
    };
}

#endif //__Q_METAL_CULLSTATE_H__

