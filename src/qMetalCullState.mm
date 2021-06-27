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

#include "qMetalCullState.h"
#include "qCore.h"

namespace qMetal
{
	CullState* CullState::PredefinedState(eCullState state)
	{
		CullState* predefinedStates[eCullState_Count] = {
	#define CULL_STATE(xxenum, xxfrontface, xxcullface) \
		new CullState(CullState::eFrontFace_ ## xxfrontface, \
		CullState::eCullFace_ ## xxcullface),
			CULL_STATES
	#undef CULL_STATE
		};
		
		qASSERT(state < eCullState_Count);
		return predefinedStates[state];
	}
	  
	CullState::CullState(
		eCullWinding    _winding,
		eCullFace       _face)
	: winding(_winding)
	, face(_face)
	{
	}

	void CullState::Encode(id<MTLRenderCommandEncoder> encoder)
	{
		[encoder setFrontFacingWinding:(MTLWinding)winding];
		[encoder setCullMode:(MTLCullMode)face];
	}
}
