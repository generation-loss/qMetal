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

#ifndef __Q_METAL_MATERIAL_H__
#define __Q_METAL_MATERIAL_H__

#include <Metal/Metal.h>
#include "qMetalFunction.h"
#include "qMetalDevice.h"
#include "qMetalBlendState.h"
#include "qMetalDepthStencilState.h"
#include "qMetalTexture.h"
#include "qMetalCullState.h"
#include "qMetalRenderTarget.h"

namespace qMetal
{
	typedef struct EmptyParams
	{
	} EmptyParams;
	
	typedef int32_t ParamIndex;
	
	const ParamIndex EmptyIndex = -1;
	
	//RPW TODO this should live on qMetalMesh or somewhere more common
	enum eTessellationFactorMode
	{
		eTessellationFactorMode_Constant,
		eTessellationFactorMode_PerPatch,
		eTessellationFactorMode_PerPatchMultiplied
	};

    template<class _VertexParams, class _FragmentParams, class _ComputeParams = EmptyParams, class _InstanceParams = EmptyParams>
	class Material
    {             
    public:
		
		//RPW TODO this should live on qMetalMesh or somewhere more common
		enum eTessellationIndexBufferType
		{
			//enum value = bytes
			eTessellationIndexBufferType_16 = MTLTessellationControlPointIndexTypeUInt16,
			eTessellationIndexBufferType_32 = MTLTessellationControlPointIndexTypeUInt32
		};
		
        typedef struct Config
        {
			NSString* name;
            const Function* computeFunction;
            const Function* vertexFunction;
            const Function* fragmentFunction;
            ParamIndex vertexTextureIndex;
            ParamIndex vertexParamsIndex;
            ParamIndex fragmentTextureIndex;
            ParamIndex fragmentParamsIndex;
            ParamIndex computeParamsIndex;
            ParamIndex instanceParamsIndex;
            MTLVertexDescriptor* vertexDescriptor;
            BlendState* blendStates[RenderTarget::eColorAttachment_Count];
            DepthStencilState* depthStencilState;
			uint32_t stencilReferenceValue;
            CullState* cullState;
            const Texture* computeTextures[Texture::eUnit_Count];
            const Texture* vertexTextures[Texture::eUnit_Count];
            const Texture* fragmentTextures[Texture::eUnit_Count];
			uint32_t vertexSamplers[Texture::eUnit_Count];
			uint32_t fragmentSamplers[Texture::eUnit_Count];
			NSUInteger instanceCount;
			eTessellationIndexBufferType tessellationIndexBufferType;
			eTessellationFactorMode tessellationFactorMode;
			bool alphaToCoverage;
			bool forIndirectCommandBuffer;
			bool tessellated;
			
            Config(NSString* _name)
            : name([_name retain])
            , computeFunction(NULL)
            , vertexFunction(NULL)
            , fragmentFunction(NULL)
            , vertexTextureIndex(EmptyIndex)
            , vertexParamsIndex(EmptyIndex)
            , fragmentTextureIndex(EmptyIndex)
            , fragmentParamsIndex(EmptyIndex)
            , computeParamsIndex(EmptyIndex)
            , instanceParamsIndex(EmptyIndex)
            , vertexDescriptor(NULL)
            , depthStencilState(NULL)
			, stencilReferenceValue(0)
            , cullState(NULL)
			, instanceCount(1)
            , tessellationIndexBufferType(eTessellationIndexBufferType_16)
			, tessellationFactorMode(eTessellationFactorMode_Constant)
			, alphaToCoverage(false)
			, forIndirectCommandBuffer(false)
			, tessellated(false)
            {
              memset(blendStates, 0, sizeof(blendStates));
              memset(computeTextures, 0, sizeof(vertexTextures));
              memset(vertexTextures, 0, sizeof(vertexTextures));
              memset(fragmentTextures, 0, sizeof(fragmentTextures));
              memset(vertexSamplers, 0, sizeof(vertexSamplers));
              memset(fragmentSamplers, 0, sizeof(fragmentSamplers));
            }
			
			Config(Config* config, NSString* name)
            : name(name)
            , computeFunction(config->computeFunction)
            , vertexFunction(config->vertexFunction)
            , fragmentFunction(config->fragmentFunction)
            , vertexTextureIndex(config->vertexTextureIndex)
            , vertexParamsIndex(config->vertexParamsIndex)
            , fragmentTextureIndex(config->fragmentTextureIndex)
            , fragmentParamsIndex(config->fragmentParamsIndex)
            , computeParamsIndex(config->computeParamsIndex)
            , instanceParamsIndex(config->instanceParamsIndex)
            , vertexDescriptor(config->vertexDescriptor)
            , depthStencilState(config->depthStencilState)
			, stencilReferenceValue(config->stencilReferenceValue)
            , cullState(config->cullState)
			, instanceCount(config->instanceCount)
            , tessellationIndexBufferType(config->tessellationIndexBufferType)
			, tessellationFactorMode(config->tessellationFactorMode)
			, alphaToCoverage(config->alphaToCoverage)
			, forIndirectCommandBuffer(config->forIndirectCommandBuffer)
			, tessellated(config->tessellated)
			{
				memcpy(&blendStates, &config->blendStates, sizeof(blendStates));
				memcpy(&computeTextures, &config->computeTextures, sizeof(vertexTextures));
				memcpy(&vertexTextures, &config->vertexTextures, sizeof(vertexTextures));
				memcpy(&fragmentTextures, &config->fragmentTextures, sizeof(fragmentTextures));
				memcpy(&vertexSamplers, &config->vertexSamplers, sizeof(vertexSamplers));
				memcpy(&fragmentSamplers, &config->fragmentSamplers, sizeof(fragmentSamplers));
			}
        } Config;
		
		Material(const Config* _config) :
		Material(_config, Texture::ePixelFormat_Invalid, Texture::ePixelFormat_Invalid, Texture::ePixelFormat_Invalid, Texture::eMSAA_1)
		{
			//compute only
			qASSERTM(config->computeFunction != NULL, 	"Material without render target settings must be compute");
			qASSERTM(config->vertexFunction == NULL, 	"Material without render target settings must be compute");
			qASSERTM(config->fragmentFunction == NULL, 	"Material without render target settings must be compute");
		}
		
        Material(const Config* _config, const RenderTarget* renderTarget) :
			Material(_config,
					 renderTarget->GetPixelFormat(),
					 (renderTarget->DepthTexture() == NULL) ? Texture::ePixelFormat_Invalid : renderTarget->DepthTexture()->GetConfig()->pixelFormat,
					 (renderTarget->StencilTexture() == NULL) ? Texture::ePixelFormat_Invalid : renderTarget->StencilTexture()->GetConfig()->pixelFormat,
					 (renderTarget->GetConfig()->colorAttachmentCount != RenderTarget::eColorAttachment_0) ? renderTarget->ColourTexture(RenderTarget::eColorAttachment_0)->GetConfig()->msaa : (renderTarget->DepthTexture() != NULL ? renderTarget->DepthTexture()->GetConfig()->msaa : Texture::eMSAA_1))
		 {}
		
		Material(const Config* _config, const Texture::ePixelFormat colourFormat, const Texture::ePixelFormat depthFormat, const Texture::ePixelFormat stencilFormat, const Texture::eMSAA msaa) :
		Material(_config,
				(Texture::ePixelFormat[]){
					colourFormat,
					Texture::ePixelFormat_Invalid,
					Texture::ePixelFormat_Invalid,
					Texture::ePixelFormat_Invalid,
					Texture::ePixelFormat_Invalid,
					Texture::ePixelFormat_Invalid,
					Texture::ePixelFormat_Invalid,
					Texture::ePixelFormat_Invalid
				},
				depthFormat,
				stencilFormat,
				msaa)
		{}
      
		Material(const Config* _config, const Texture::ePixelFormat colourFormat[], const Texture::ePixelFormat depthFormat, const Texture::ePixelFormat stencilFormat, const Texture::eMSAA msaa)
        : config(_config)
        {	
            NSError* error = nil;
			
			//COMPUTE PIPELINE
			
            if (config->computeFunction != NULL)
            {
				MTLComputePipelineDescriptor* computeDesc = [MTLComputePipelineDescriptor new];
				
				computeDesc.label = config->name;
				
				computeDesc.computeFunction = config->computeFunction->Get();
				
				computePipelineState 	= [qMetal::Device::Get() newComputePipelineStateWithDescriptor:computeDesc options:MTLPipelineOptionNone reflection:nil error:&error];
				
				qASSERTM((computePipelineState != nil) && (error == nil), "qMetalMaterial Failed to create compute pipeline %s with error %s", [config->name UTF8String], [[error description] UTF8String]);
			}
			
			if (config->computeParamsIndex != EmptyIndex)
			{
				//we may not have a function (e.g. indirect command buffers) but still want compute params buffer
				for (uint32_t i = 0; i < Q_METAL_FRAMES_TO_BUFFER; ++i)
				{
					computeParamsBuffer[i] = [qMetal::Device::Get() newBufferWithLength:sizeof(_ComputeParams) options:0];
					computeParamsBuffer[i].label = [NSString stringWithFormat:@"%@ compute params (frame %i)", config->name, i];
				}
			}
			
            //RENDER PIPELINE
			
            if (config->vertexFunction != NULL)
            {
				MTLRenderPipelineDescriptor* renderDesc = [MTLRenderPipelineDescriptor new];

				renderDesc.label = config->name;
				
				renderDesc.vertexFunction = config->vertexFunction->Get();
				
				//this can be null in the case of a depth-only render
				if (config->fragmentFunction != NULL)
				{
					renderDesc.fragmentFunction = config->fragmentFunction->Get();
				}
				
				for (int i = 0; i < (int)RenderTarget::eColorAttachment_Count; ++i)
				{
					if (config->blendStates[i] != NULL)
					{
						renderDesc.colorAttachments[i].blendingEnabled = config->blendStates[i]->blendEnabled;
						renderDesc.colorAttachments[i].rgbBlendOperation = (MTLBlendOperation)config->blendStates[i]->rgbBlendOperation;
						renderDesc.colorAttachments[i].sourceRGBBlendFactor = (MTLBlendFactor)config->blendStates[i]->rgbSrcBlendFactor;
						renderDesc.colorAttachments[i].destinationRGBBlendFactor = (MTLBlendFactor)config->blendStates[i]->rgbDstBlendFactor;
						renderDesc.colorAttachments[i].alphaBlendOperation = (MTLBlendOperation)config->blendStates[i]->alphaBlendOperation;
						renderDesc.colorAttachments[i].sourceAlphaBlendFactor = (MTLBlendFactor)config->blendStates[i]->alphaSrcBlendFactor;
						renderDesc.colorAttachments[i].destinationAlphaBlendFactor = (MTLBlendFactor)config->blendStates[i]->alphaDstBlendFactor;
					}
					
					renderDesc.colorAttachments[i].pixelFormat = (MTLPixelFormat)colourFormat[i];
				}

				renderDesc.depthAttachmentPixelFormat = (MTLPixelFormat)depthFormat;
				renderDesc.stencilAttachmentPixelFormat = (MTLPixelFormat)stencilFormat;
				renderDesc.sampleCount = msaa;
				renderDesc.alphaToCoverageEnabled = config->alphaToCoverage;
				
				//TESSELLATION / COMPUTE
				if (config->tessellated)
				{
					renderDesc.tessellationControlPointIndexType = (MTLTessellationControlPointIndexType)config->tessellationIndexBufferType;
					renderDesc.tessellationFactorStepFunction = MTLTessellationFactorStepFunctionPerPatchAndPerInstance;
					renderDesc.tessellationOutputWindingOrder = MTLWindingCounterClockwise;
					renderDesc.tessellationPartitionMode = MTLTessellationPartitionModeFractionalEven; 			//TODO is this what we want?
					renderDesc.tessellationFactorFormat = MTLTessellationFactorFormatHalf; 						//This is the only option
					renderDesc.maxTessellationFactor = 64; 														//The limit on A11 and below is 16
					renderDesc.tessellationFactorStepFunction = (config->tessellationFactorMode == eTessellationFactorMode_Constant) ?
						(IsInstanced() ? MTLTessellationFactorStepFunctionPerInstance : MTLTessellationFactorStepFunctionConstant) :
						(IsInstanced() ? MTLTessellationFactorStepFunctionPerPatchAndPerInstance : MTLTessellationFactorStepFunctionPerPatch);
				}
				
				if (config->vertexDescriptor != NULL)
				{
					renderDesc.vertexDescriptor = config->vertexDescriptor;
				}
				
				renderDesc.supportIndirectCommandBuffers = config->forIndirectCommandBuffer;
				
				renderPipelineState 	= [qMetal::Device::Get() newRenderPipelineStateWithDescriptor: renderDesc error: &error];
				
				qASSERTM((renderPipelineState != nil) && (error == nil), "qMetalMaterial Failed to create render pipeline %s with error %s", [config->name UTF8String], [[error description] UTF8String]);
				
				for (uint32_t i = 0; i < Q_METAL_FRAMES_TO_BUFFER; ++i)
				{
					if (config->vertexParamsIndex != EmptyIndex)
					{
						vertexParamsBuffer[i] = [qMetal::Device::Get() newBufferWithLength:sizeof(_VertexParams) options:0];
						vertexParamsBuffer[i].label = [NSString stringWithFormat:@"%@ vertex params (frame %i)", config->name, i];
					}
					
					if (config->fragmentParamsIndex != EmptyIndex)
					{
						fragmentParamsBuffer[i] = [qMetal::Device::Get() newBufferWithLength:sizeof(_FragmentParams) options:0];
						fragmentParamsBuffer[i].label = [NSString stringWithFormat:@"%@ fragment params (frame %i)", config->name, i];
					}
					
					if (IsInstanced() && (sizeof(_InstanceParams) > 0))
					{
						instanceParamsBuffer[i] = [qMetal::Device::Get() newBufferWithLength:(sizeof(_InstanceParams) * config->instanceCount) options:0];
						instanceParamsBuffer[i].label = [NSString stringWithFormat:@"%@ instance params (frame %i)", config->name, i];
					}
				}
				
				//TEXTURES
				//note that "Writable textures are not supported within an argument buffer" (https://developer.apple.com/documentation/metal/resource_objects/about_argument_buffers?language=objc)
				//so we don't make one for Compute Shaders, as our whole goal there is outputting one (or more) textures
				
				if (config->vertexTextureIndex != EmptyIndex)
				{
					id <MTLArgumentEncoder> vertexTextureEncoder
					= [config->vertexFunction->Get() newArgumentEncoderWithBufferIndex:config->vertexTextureIndex];
					vertexTextureBuffer = [qMetal::Device::Get() newBufferWithLength:vertexTextureEncoder.encodedLength options:0];
					vertexTextureBuffer.label = [NSString stringWithFormat:@"%@ vertex textures", config->name];
					[vertexTextureEncoder setArgumentBuffer:vertexTextureBuffer offset:0];
					
					for (int i = 0; i < (int)Texture::eUnit_Count; ++i)
					{
						if (config->vertexTextures[i] != NULL)
						{
							config->vertexTextures[i]->EncodeArgumentBuffer(vertexTextureEncoder, (Texture::eUnit)i, (Texture::eUnit)config->vertexSamplers[i]);
						}
					}
				}

				if ((config->fragmentTextureIndex != EmptyIndex) && (config->fragmentFunction != NULL)) //we might be sharing a material description with fragment textures but not actually have a fragment funtion
				{
					id <MTLArgumentEncoder> fragmentTextureEncoder = [config->fragmentFunction->Get() newArgumentEncoderWithBufferIndex:config->fragmentTextureIndex];
					fragmentTextureBuffer = [qMetal::Device::Get() newBufferWithLength:fragmentTextureEncoder.encodedLength options:0];
					fragmentTextureBuffer.label = [NSString stringWithFormat:@"%@ fragment textures", config->name];
					[fragmentTextureEncoder setArgumentBuffer:fragmentTextureBuffer offset:0];
					
					for (int i = 0; i < (int)Texture::eUnit_Count; ++i)
					{
						if (config->fragmentTextures[i] != NULL)
						{
							config->fragmentTextures[i]->EncodeArgumentBuffer(fragmentTextureEncoder, (Texture::eUnit)i, (Texture::eUnit)config->fragmentSamplers[i]);
						}
					}
				}
			}
			else
			{
				qASSERTM(config->fragmentFunction == NULL, "Vertex function is null but fragment function isn't");
			}
        }
		
		void EncodeCompute(NSUInteger width, NSUInteger height, NSUInteger depth = 1) const
		{
			id<MTLComputeCommandEncoder> computeEncoder = qMetal::Device::ComputeEncoder(config->name);
			[computeEncoder pushDebugGroup:config->name];
			
			EncodeCompute(computeEncoder, width, height, depth);
			
			[computeEncoder popDebugGroup];
			[computeEncoder endEncoding];
		}
		
		void EncodeCompute(id<MTLComputeCommandEncoder> encoder, NSUInteger width, NSUInteger height, NSUInteger depth = 1) const
		{
			Encode(encoder);
			
			//TODO this could likely be optimized in case where width is much wider than height, say, and height is < maxTotalThreadsPerThreadgroupSqrt
			
			NSUInteger maxTotalThreadsPerThreadgroupSqrt = (NSUInteger)sqrt((double)computePipelineState.maxTotalThreadsPerThreadgroup);
			
			MTLSize threadsPerGrid = MTLSizeMake(width, height, depth);
			MTLSize threadsPerThreadgroup = MTLSizeMake(maxTotalThreadsPerThreadgroupSqrt, maxTotalThreadsPerThreadgroupSqrt, 1);
			
			[encoder dispatchThreads:threadsPerGrid threadsPerThreadgroup:threadsPerThreadgroup];
		}
		
        void Encode(id<MTLComputeCommandEncoder> encoder) const
		{
			[encoder setComputePipelineState:computePipelineState];
			
			if (config->computeParamsIndex != EmptyIndex)
			{
				[encoder setBuffer:computeParamsBuffer[qMetal::Device::CurrentFrameIndex()] offset:0 atIndex:config->computeParamsIndex];
			}
		
			//TODO textures into an argument buffer
			for (int i = 0; i < (int)Texture::eUnit_Count; ++i)
            {
                if (config->computeTextures[i] != NULL)
                {
					config->computeTextures[i]->EncodeCompute(encoder, (Texture::eUnit)i);
				}
			}
        }
		
        void EncodeTextures(id<MTLComputeCommandEncoder> encoder) const
		{
			//TODO put this into an argument buffer
			for (int i = 0; i < (int)Texture::eUnit_Count; ++i)
            {
                if (config->computeTextures[i] != NULL)
                {
					config->computeTextures[i]->EncodeCompute(encoder, (Texture::eUnit)i);
				}
			}
        }
      
        void Encode(id<MTLRenderCommandEncoder> encoder) const
		{
			[encoder setRenderPipelineState:renderPipelineState];
			
			if (config->vertexTextureIndex != EmptyIndex)
			{
				for (int i = 0; i < (int)Texture::eUnit_Count; ++i)
				{
					if (config->vertexTextures[i] != NULL)
					{
						config->vertexTextures[i]->EncodeUsage(encoder, MTLRenderStageVertex);
					}
				}
			}
			
			if ((config->fragmentFunction != NULL) && (config->fragmentTextureIndex != EmptyIndex))
			{
				//we may share a material description with + without fragment function (shadow / depth pass), so we also check for the function
				
				for (int i = 0; i < (int)Texture::eUnit_Count; ++i)
				{
					if (config->fragmentTextures[i] != NULL)
					{
						config->fragmentTextures[i]->EncodeUsage(encoder, MTLRenderStageFragment);
					}
				}
			}
			
			
			if (!config->forIndirectCommandBuffer)
			{
				if (config->vertexParamsIndex != EmptyIndex)
				{
					[encoder setVertexBuffer:vertexParamsBuffer[qMetal::Device::CurrentFrameIndex()] offset:0 atIndex:config->vertexParamsIndex];
				}
				
				if (config->vertexTextureIndex != EmptyIndex)
				{
					[encoder setVertexBuffer:vertexTextureBuffer offset:0 atIndex:config->vertexTextureIndex];
				}
				
				if (config->instanceParamsIndex != EmptyIndex)
				{
					[encoder setVertexBuffer:instanceParamsBuffer[qMetal::Device::CurrentFrameIndex()] offset:0 atIndex:config->instanceParamsIndex];
				}
				
				if (config->fragmentFunction != NULL)
				{
					if (config->fragmentParamsIndex != EmptyIndex)
					{
						[encoder setFragmentBuffer:fragmentParamsBuffer[qMetal::Device::CurrentFrameIndex()] offset:0 atIndex:config->fragmentParamsIndex];
					}
					
					if (config->fragmentTextureIndex != EmptyIndex)
					{
						[encoder setFragmentBuffer:fragmentTextureBuffer offset:0 atIndex:config->fragmentTextureIndex];
					}
				}
			}
			else
			{
				if (config->vertexParamsIndex != EmptyIndex)
				{
					[encoder useResource:vertexParamsBuffer[qMetal::Device::CurrentFrameIndex()] usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
				}
				
				if (config->vertexTextureIndex != EmptyIndex)
				{
					[encoder useResource:vertexTextureBuffer usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
				}
				
				if (config->instanceParamsIndex != EmptyIndex)
				{
					[encoder useResource:instanceParamsBuffer[qMetal::Device::CurrentFrameIndex()] usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
				}
				
				if (config->fragmentFunction != NULL)
				{
					if (config->fragmentParamsIndex != EmptyIndex)
					{
						[encoder useResource:fragmentParamsBuffer[qMetal::Device::CurrentFrameIndex()] usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
					}
					
					if (config->fragmentTextureIndex != EmptyIndex)
					{
						[encoder useResource:fragmentTextureBuffer usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
					}
				}
			}
			
			config->depthStencilState->Encode(encoder);
			[encoder setStencilReferenceValue:config->stencilReferenceValue];
			config->cullState->Encode(encoder);
        }
		
        _ComputeParams* CurrentFrameComputeParams() const
        {
			return (_ComputeParams*)[CurrentFrameComputeParamsBuffer() contents];
        }
		
		id<MTLBuffer> CurrentFrameComputeParamsBuffer() const
		{
			qASSERTM(config->computeParamsIndex != EmptyIndex, "No compute params index set, check material definition");
			return computeParamsBuffer[qMetal::Device::CurrentFrameIndex()];
		}
		
		_VertexParams* CurrentFrameVertexParams() const
		{
			return (_VertexParams*)[CurrentFrameVertexParamsBuffer() contents];
		}
		
		id<MTLBuffer> CurrentFrameVertexParamsBuffer() const
		{
			qASSERTM(config->vertexParamsIndex != EmptyIndex, "No vertex params index set, check material definition");
			return vertexParamsBuffer[qMetal::Device::CurrentFrameIndex()];
		}
		
		id<MTLBuffer> VertexTextureBuffer() const
		{
			qASSERTM(config->vertexTextureIndex != EmptyIndex, "No vertex texture index set, check material definition");
			return vertexTextureBuffer;
		}
		
		_InstanceParams* CurrentFrameInstanceParams(uint32_t instanceIndex) const
		{
			qASSERTM(IsInstanced(), "Asking for instance %i but with a material that isn't instanced", instanceIndex);
			qASSERTM(instanceIndex < config->instanceCount, "Asking for instance %i but with a material that only supports %i", instanceIndex, config->instanceCount);
			return (_InstanceParams*)([instanceParamsBuffer[qMetal::Device::CurrentFrameIndex()] contents]) + instanceIndex;
		}
		
		_FragmentParams* CurrentFrameFragmentParams() const
		{
			return (_FragmentParams*)[CurrentFrameFragmentParamsBuffer() contents];
		}
		
		id<MTLBuffer> CurrentFrameFragmentParamsBuffer() const
		{
			qASSERTM(config->fragmentParamsIndex != EmptyIndex, "No fragment params index set, check material definition");
			return fragmentParamsBuffer[qMetal::Device::CurrentFrameIndex()];
		}
		
		id<MTLBuffer> FragmentTextureBuffer() const
		{
			qASSERTM(config->fragmentTextureIndex != EmptyIndex, "No fragment texture index set, check material definition");
			return fragmentTextureBuffer;
		}
		
        const Function* ComputeFunction() const
        {
        	return config->computeFunction;
		}
		
        const Function* VertexFunction() const
        {
        	return config->vertexFunction;
		}
		
        const Function* FragmentFunction() const
        {
        	return config->fragmentFunction;
		}
		
		const bool IsInstanced() const
		{
			return config->instanceCount > 1;
		}
		
		const NSUInteger InstanceCount() const
		{
			return config->instanceCount;
		}

    private:
    
      	const Config* config;
		
		id<MTLComputePipelineState> computePipelineState;
		id<MTLRenderPipelineState> renderPipelineState;
		
		id<MTLBuffer> computeParamsBuffer[Q_METAL_FRAMES_TO_BUFFER];
		id<MTLBuffer> vertexParamsBuffer[Q_METAL_FRAMES_TO_BUFFER];
		id<MTLBuffer> instanceParamsBuffer[Q_METAL_FRAMES_TO_BUFFER];
		id<MTLBuffer> fragmentParamsBuffer[Q_METAL_FRAMES_TO_BUFFER];
		
		id<MTLBuffer> vertexTextureBuffer;
		id<MTLBuffer> fragmentTextureBuffer;
    };
}

#endif //__Q_METAL_MATERIAL_H__

