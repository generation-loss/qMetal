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

#include "qMetalBlendState.h"
#include "qCore.h"

namespace qMetal
{
	BlendState* BlendState::PredefinedState(eBlendState state)
	{
		BlendState* predefinedStates[eBlendState_Count] = {
	#define BLEND_STATE(xxenum, xxblend, xxrgbop, xxrgbsrc, xxrgbdst, xxalphaop, xxalphasrc, xxalphadst) \
		new BlendState(xxblend, \
			BlendState::eBlendOperation_ ## xxrgbop, \
			BlendState::eBlendFactor_ ## xxrgbsrc, \
			BlendState::eBlendFactor_ ## xxrgbdst, \
			BlendState::eBlendOperation_ ## xxalphaop, \
			BlendState::eBlendFactor_ ## xxalphasrc, \
			BlendState::eBlendFactor_ ## xxalphadst),
			BLEND_STATES
	#undef BLEND_STATE
		};
		
		qASSERT(state < eBlendState_Count);
		return predefinedStates[state];
	}
	
	BlendState::BlendState(
		bool _blendEnabled,
		eBlendOperation _rgbBlendOperation,
		eBlendFactor _rgbSrcBlendFactor,
		eBlendFactor _rgbDstBlendFactor,
		eBlendOperation _alphaBlendOperation,
		eBlendFactor _alphaSrcBlendFactor,
		eBlendFactor _alphaDstBlendFactor)
	: blendEnabled(_blendEnabled)
	, rgbBlendOperation(_rgbBlendOperation)
	, rgbSrcBlendFactor(_rgbSrcBlendFactor)
	, rgbDstBlendFactor(_rgbDstBlendFactor)
	, alphaBlendOperation(_alphaBlendOperation)
	, alphaSrcBlendFactor(_alphaSrcBlendFactor)
	, alphaDstBlendFactor(_alphaDstBlendFactor)
	{
	}
}
