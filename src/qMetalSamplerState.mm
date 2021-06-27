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

#include "qMetalSamplerState.h"
#include "qCore.h"

namespace qMetal
{
	SamplerState* SamplerState::PredefinedState(eSamplerState state)
	{
		SamplerState* predefinedStates[eSamplerState_Count] = {
	#define SAMPLER_STATE(xxenum, xxminfilter, xxmagfilter, xxmipfilter, xxwrapx, xxwrapy) \
		new SamplerState(SamplerState::eMinFilter_ ## xxminfilter, \
			SamplerState::eMagFilter_ ## xxmagfilter, \
			SamplerState::eMipFilter_ ## xxmipfilter, \
			SamplerState::eWrap_ ## xxwrapx, \
			SamplerState::eWrap_ ## xxwrapy),
			SAMPLER_STATES
	#undef SAMPLER_STATE
		};
		
		qASSERT(state < eSamplerState_Count);
		return predefinedStates[state];
	}
		
	SamplerState::SamplerState(
				 eMinFilter      _minFilter,
				 eMagFilter      _magFilter,
				 eMipFilter      _mipFilter,
				 eWrap           _wrapX,
				 eWrap           _wrapY
	)
	: minFilter(_minFilter)
	, magFilter(_magFilter)
	, mipFilter(_mipFilter)
	, wrapX(_wrapX)
	, wrapY(_wrapY)
	{
		MTLSamplerDescriptor *samplerDescriptor = [MTLSamplerDescriptor new];
		samplerDescriptor.minFilter = (MTLSamplerMinMagFilter)minFilter;
		samplerDescriptor.magFilter = (MTLSamplerMinMagFilter)magFilter;
		samplerDescriptor.mipFilter = (MTLSamplerMipFilter)mipFilter;
		samplerDescriptor.sAddressMode = (MTLSamplerAddressMode)wrapX;
		samplerDescriptor.tAddressMode = (MTLSamplerAddressMode)wrapY;
		samplerDescriptor.supportArgumentBuffers = YES;
		samplerState = [qMetal::Device::Get() newSamplerStateWithDescriptor:samplerDescriptor];
	}
	
	void SamplerState::Encode(id<MTLArgumentEncoder> encoder, uint32_t index) const
	{
		[encoder setSamplerState:samplerState atIndex:index];
	}
}
