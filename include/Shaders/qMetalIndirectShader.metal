/*
Copyright (c) 2022 Generation Loss Interactive

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

#include "qMetalIndirectShader.h"

using namespace metal;

kernel void qMetalIndirectResetShader(
	device MTLIndirectCommandBufferExecutionRange* mtlrange	[[ buffer(0) ]],
	device uint32_t *rangeLength 							[[ buffer(1) ]])
{
	for(uint rangeIndex = 0; rangeIndex < *rangeLength; ++rangeIndex)
	{
		mtlrange[rangeIndex].location = 0;
		mtlrange[rangeIndex].length = 0;
	}
}

kernel void qMetalIndirectInitShader(
	 device MTLIndirectCommandBufferExecutionRange* range		[[ buffer(0) ]],
	 device uint32_t* rangeLength								[[ buffer(1) ]],
	 device uint32_t* rangeOffset								[[ buffer(2) ]])
{
	uint32_t spaceUsed = 0;
	for(uint rangeIndex = 0; rangeIndex < *rangeLength; ++rangeIndex)
	{
		spaceUsed += range[rangeIndex].length;
	}
	range[*rangeOffset].location = spaceUsed;
	range[*rangeOffset].length = 0;
}
