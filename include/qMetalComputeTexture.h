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

#ifndef __Q_METAL_COMPUTE_TEXTURE_H__
#define __Q_METAL_COMPUTE_TEXTURE_H__

#include <Metal/Metal.h>
#include "qCore.h"
#include "qMetalDevice.h"
#include "qMetalTexture.h"
#include "qRGBA.h"

namespace qMetal
{
    class ComputeTexture : public Texture
    {        
    public:
		
        typedef struct Config : public Texture::Config
        {
			Config(
				   NSString*		_name,
				   NSUInteger		_width,
				   NSUInteger		_height,
				   ePixelFormat    _pixelFormat)
			: Texture::Config(
							  _name,
							  _width,
							  _height,
							  1,
							  1,
							  Texture::eType_2D,
							  _pixelFormat,
							  false,
							  (Texture::eUsage)(Texture::eUsage_ShaderWrite | Texture::eUsage_ShaderRead),
							  Texture::eMSAA_1,
							  Texture::eStorage_GPUOnly
							 )
			{}
			
			Config(
				   NSString*		_name,
				   NSUInteger		_width,
				   NSUInteger		_height,
				   NSUInteger		_depth,
				   ePixelFormat    _pixelFormat)
			: Texture::Config(
							  _name,
							  _width,
							  _height,
							  _depth,
							  1,
							  _depth == 1 ? Texture::eType_2D : Texture::eType_3D,
							  _pixelFormat,
							  false,
							  (Texture::eUsage)(Texture::eUsage_ShaderWrite | Texture::eUsage_ShaderRead),
							  Texture::eMSAA_1,
							  Texture::eStorage_GPUOnly
							 )
			{}
        } Config;
        
		ComputeTexture(Config *_config, SamplerState *_samplerState = SamplerState::PredefinedState(eSamplerState_LinearLinearNone_ClampClamp))
        : Texture((Texture::Config*)_config, _samplerState)
        , config(_config)
        , samplerState(_samplerState)
        {
        }
		
		const Config* GetConfig() const { return config; }
        
    private:
		
        const Config		*config;
		const SamplerState 	*samplerState;
    };
}

#endif //__Q_METAL_COMPUTE_TEXTURE_H__

