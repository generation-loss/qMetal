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

#ifndef __Q_METAL_SAMPLERSTATE_H__
#define __Q_METAL_SAMPLERSTATE_H__

#include <Metal/Metal.h>
#include "qMetalDevice.h"

#define SAMPLER_STATES \
/*             	enum										min filter		mag filter		mip filter		wrap x, 		wrap y			*/ \
SAMPLER_STATE(	LinearLinearLinear_MirrorMirror,			Linear,			Linear,			Linear,			Mirror,			Mirror			) \
SAMPLER_STATE(	LinearLinearLinear_RepeatRepeat,			Linear,			Linear,			Linear,			Repeat,			Repeat			) \
SAMPLER_STATE(	LinearLinearLinear_ClampClamp,				Linear,			Linear,			Linear,			ClampToEdge,	ClampToEdge		) \
SAMPLER_STATE(	LinearLinearNone_RepeatRepeat,				Linear,			Linear,			None,			Repeat,			Repeat			) \
SAMPLER_STATE(	LinearLinearNone_ClampClamp,				Linear,			Linear,			None,			ClampToEdge,	ClampToEdge		) \
SAMPLER_STATE(	PointPointNone_ClampClamp,					Point,			Point,			None,			ClampToEdge,	ClampToEdge		) \
SAMPLER_STATE(	PointPointNone_RepeatRepeat,				Point,			Point,			None,			Repeat,			Repeat			) \
SAMPLER_STATE(	PointPointNone_ZeroZero,					Point,			Point,			None,			ClampToZero,	ClampToZero		) \

namespace qMetal
{
    enum eSamplerState
    {
#define SAMPLER_STATE(xxenum, xxminfilter, xxmagfilter, xxmipfilter, xxwrapx, xxwrapy) eSamplerState_ ## xxenum,
        SAMPLER_STATES
#undef SAMPLER_STATE
        eSamplerState_Count,
    };
    
    struct SamplerState
    {
		enum eMinFilter
		{
			eMinFilter_Point        = MTLSamplerMinMagFilterNearest,
			eMinFilter_Linear       = MTLSamplerMinMagFilterLinear,
		};
		
		enum eMagFilter
		{
			eMagFilter_Point        = MTLSamplerMinMagFilterNearest,
			eMagFilter_Linear       = MTLSamplerMinMagFilterLinear,
		};
		
		enum eMipFilter
		{
			eMipFilter_None         = MTLSamplerMipFilterNotMipmapped,
			eMipFilter_Point        = MTLSamplerMipFilterNearest,
			eMipFilter_Linear       = MTLSamplerMipFilterLinear,
		};
		
		enum eWrap
		{
			eWrap_Repeat            = MTLSamplerAddressModeRepeat,
			eWrap_Mirror		    = MTLSamplerAddressModeMirrorRepeat,
			eWrap_ClampToEdge       = MTLSamplerAddressModeClampToEdge,
			eWrap_ClampToZero       = MTLSamplerAddressModeClampToZero,
		};
		
		eMinFilter      	minFilter;
		eMagFilter      	magFilter;
		eMipFilter      	mipFilter;
		eWrap           	wrapX;
		eWrap           	wrapY;
		
        SamplerState(
					 eMinFilter      _minFilter,
					 eMagFilter      _magFilter,
					 eMipFilter      _mipFilter,
					 eWrap           _wrapX,
					 eWrap           _wrapY
		);
		
		void Encode(id<MTLArgumentEncoder> encoder, uint32_t index) const;
        
        static SamplerState *PredefinedStates[eSamplerState_Count];
		
	private:
		
		id<MTLSamplerState>	samplerState;
    };
}

#endif //__Q_METAL_SAMPLERSTATE_H__

