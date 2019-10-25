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

#ifndef __Q_METAL_BLENDSTATE_H__
#define __Q_METAL_BLENDSTATE_H__

#include <Metal/Metal.h>

#define BLEND_STATES \
/*              enum    				blend       rgbOp       		rgbSrc      rgbDest         	alphaOp     		alphaSrc    alphaDst        */ \
BLEND_STATE(   Off,    					false,      Add,        		One,        Zero,           	Add,        		One,        Zero           	) \
BLEND_STATE(   Keep,    				true,      	Add,        		Zero,		One,           		Add,        		Zero,		One				) \
BLEND_STATE(   Alpha,  					true,       Add,        		SrcAlpha,   OneMinusSrcAlpha, 	Add,				Zero,		One            	) \
BLEND_STATE(   Add,  					true,       Add,        		One,   		One, 				Add,        		One,		One            	) \
BLEND_STATE(   AddAlpha,				true,       Add,        		SrcAlpha,	One, 				Add,        		One,		One            	) \
BLEND_STATE(   ReverseSubtract,			true, 		ReverseSubtract,   	One,   		One, 				ReverseSubtract,	One,		One        	   	) \
BLEND_STATE(   ReverseSubtractAlpha, 	true, 		ReverseSubtract,   	SrcAlpha,	One, 				ReverseSubtract,	One,		One        	   	) \

namespace qMetal
{
    enum eBlendState
    {
#define BLEND_STATE(xxenum, xxblend, xxrgbop, xxrgbsrc, xxrgbdest, xxalphaop, xxalphasrc, xxalphadst) eBlendState_ ## xxenum,
        BLEND_STATES
#undef BLEND_STATE   
        eBlendState_Count,
    };
    
    struct BlendState
    {
        enum eBlendOperation
        {
            eBlendOperation_Add             	= MTLBlendOperationAdd,
            eBlendOperation_Subtract        	= MTLBlendOperationSubtract,
            eBlendOperation_ReverseSubtract 	= MTLBlendOperationReverseSubtract,
            eBlendOperation_Min             	= MTLBlendOperationMin,
            eBlendOperation_Max             	= MTLBlendOperationMax,
        };
        
        enum eBlendFactor
        {
            eBlendFactor_Zero                 	= MTLBlendFactorZero,
            eBlendFactor_One                  	= MTLBlendFactorOne,
            eBlendFactor_SrcColour            	= MTLBlendFactorSourceColor,
            eBlendFactor_OneMinusSrcColour      = MTLBlendFactorOneMinusSourceColor,
            eBlendFactor_SrcAlpha             	= MTLBlendFactorSourceAlpha,
            eBlendFactor_OneMinusSrcAlpha       = MTLBlendFactorOneMinusSourceAlpha,
            eBlendFactor_DstColour            	= MTLBlendFactorDestinationColor,
            eBlendFactor_OneMinusDstColour      = MTLBlendFactorOneMinusDestinationColor,
            eBlendFactor_DstAlpha             	= MTLBlendFactorDestinationAlpha,
            eBlendFactor_OneMinusDstAlpha		= MTLBlendFactorOneMinusDestinationAlpha,
            eBlendFactor_SrcAlphaSat          	= MTLBlendFactorSourceAlphaSaturated,
        };
      
        bool                    blendEnabled;
        eBlendOperation         rgbBlendOperation;
        eBlendFactor            rgbSrcBlendFactor;
        eBlendFactor            rgbDstBlendFactor;
        
        eBlendOperation         alphaBlendOperation;
        eBlendFactor            alphaSrcBlendFactor;
        eBlendFactor            alphaDstBlendFactor;
        
        BlendState(
			bool _blendEnabled,
			eBlendOperation _rgbBlendOperation,
			eBlendFactor _rgbSrcBlendFactor,
			eBlendFactor _rgbDstBlendFactor,
			eBlendOperation _alphaBlendOperation,
			eBlendFactor _alphaSrcBlendFactor,
			eBlendFactor _alphaDstBlendFactor);
        
        static BlendState *PredefinedStates[eBlendState_Count];
    };
}

#endif //__Q_METAL_BLENDSTATE_H__

