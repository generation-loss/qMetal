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

#ifndef __Q_METAL_INDIRECT_MESH_H__
#define __Q_METAL_INDIRECT_MESH_H__

#include <Metal/Metal.h>
#include "qCore.h"
#include "qMetalDevice.h"
#include "qMetalMesh.h"
#include "qMetalMaterial.h"
#include <vector>

namespace qMetal
{
    template<
		//command buffer kernel
		int ICBArgumentBufferIndex,					//where the indirect command buffer argument buffer is in the kernel shader
		int ICBComputeParamsIndex,					//where the compute params live
		int ICBVertexParamsIndex,					//where the vertex params live
		int ICBVertexTextureIndex,					//where the vertex texture live
		class ICBVertexInstanceParams,				//type of vertex instance params
		int ICBVertexInstanceParamsIndex,			//where the vertex instance params live
		int ICBFragmentParamsIndex,					//where the fragment params live
		int ICBFragmentTextureIndex,				//where the fragment texture live
		int ICBIndexArgumentBufferArrayIndex,		//where the index argument buffer array lives
		int ICBVertexArgumentBufferArrayIndex,		//where the vertex argument buffer array lives

		//command buffer argument buffers
		int IndirectIndexCountIndex,				//where the number of indices lives
		int IndirectIndexStreamIndex,				//where the index buffer lives
		int IndirectTessellationFactorBufferIndex,	//where the tesselation factor buffer lives

		//mesh details
		int MeshVertexStreamCount,
		int MeshVertexStreamIndex,
		int TessellationStreamCount = 0,
		int TessellationStreamFactorsIndex = EmptyIndex
	>
    class IndirectMesh
    {        
    public:
		
        typedef struct Config
        {
            NSString*           		name;
			Function*					function;
			std::vector<Mesh<MeshVertexStreamCount, MeshVertexStreamIndex, TessellationStreamCount, TessellationStreamFactorsIndex>*> 	meshes;
			NSUInteger					count;
			
            Config(NSString* _name)
			: name(_name)
			, function(NULL)
            {}
			
			Config(Config* config, NSString* _name)
			: name(_name)
			, function(config->function)
			, meshes(config->meshes)
			, count(config->count)
			{}
        } Config;
		
		IndirectMesh(Config *_config)
        : config(_config)
		{
			qASSERTM(config->meshes.size() > 0, "Mesh config count can can not be zero");
			qASSERTM(config->meshes.size() < 14, "Mesh config count can can not exceed %i", 29); //32 LIMIT, get based on device
			
			// INDIRECT COMMAND BUFFER
			
			MTLIndirectCommandBufferDescriptor* indirectCommandBufferDescriptor = [[MTLIndirectCommandBufferDescriptor alloc] init];
			
			NSUInteger vertexBindCount = 1; //vertex streams
			vertexBindCount += (MeshVertexStreamIndex == EmptyIndex) ? MeshVertexStreamCount : 1;
			vertexBindCount += (ICBVertexTextureIndex == EmptyIndex) ? 0 : 1;
			vertexBindCount += (ICBVertexInstanceParamsIndex == EmptyIndex) ? 0 : 1;
			
			NSUInteger fragmentBindCount = 0;
			fragmentBindCount += (ICBFragmentParamsIndex == EmptyIndex) ? 0 : 1;
			fragmentBindCount += (ICBFragmentTextureIndex == EmptyIndex) ? 0 : 1;
			
			indirectCommandBufferDescriptor.commandTypes = MTLIndirectCommandTypeDrawIndexed;
			indirectCommandBufferDescriptor.inheritPipelineState = true;
			indirectCommandBufferDescriptor.inheritBuffers = false;
			indirectCommandBufferDescriptor.maxVertexBufferBindCount = vertexBindCount;
			indirectCommandBufferDescriptor.maxFragmentBufferBindCount = fragmentBindCount;
			indirectCommandBufferDescriptor.maxKernelBufferBindCount = 0;
			
			indirectCommandBuffer = [qMetal::Device::Get() newIndirectCommandBufferWithDescriptor:indirectCommandBufferDescriptor maxCommandCount:config->count options:0];
			
			// SET UP SOME BUFFERS
			
			if (ICBVertexInstanceParamsIndex != EmptyIndex)
			{
				vertexInstanceParamsBuffer = [qMetal::Device::Get() newBufferWithLength:(sizeof(ICBVertexInstanceParams) * config->count) options:0];
				vertexInstanceParamsBuffer.label = [NSString stringWithFormat:@"%@ vertex instance params", config->name];
			}
			
			// COMPUTE PIPELINE STATE FOR INDIRECT COMMAND BUFFER CONSTRUCTION
			
			NSError *error = nil;
			computePipelineState = [qMetal::Device::Get() newComputePipelineStateWithFunction:config->function->Get()
									 													error:&error];
			if (!computePipelineState)
			{
				NSLog(@"Failed to created indirect command buffer compute pipeline state, error %@", error);
			}
			
			//COMPUTE PIPELINE STATE ARGUMENT BUFFER
			
			id <MTLArgumentEncoder> argumentEncoder = [config->function->Get() newArgumentEncoderWithBufferIndex:ICBArgumentBufferIndex];
			NSUInteger argumentBufferLength = argumentEncoder.encodedLength;
			
			commandBufferArgumentBuffer = [qMetal::Device::Get() newBufferWithLength:argumentBufferLength options:0];
			commandBufferArgumentBuffer.label = [NSString stringWithFormat:@"%@ indirect command buffer argument buffer", config->name];
			
			[argumentEncoder setArgumentBuffer:commandBufferArgumentBuffer offset:0];
			[argumentEncoder setIndirectCommandBuffer:indirectCommandBuffer atIndex:ICBArgumentBufferIndex];
			
			int meshIndex = 0;
			for(auto &it : config->meshes)
			{
				id <MTLArgumentEncoder> indexArgumentEncoder = [config->function->Get() newArgumentEncoderWithBufferIndex:(ICBIndexArgumentBufferArrayIndex + meshIndex)];
				NSUInteger argumentBufferLength = indexArgumentEncoder.encodedLength;
				
				id <MTLBuffer> indexArgumentBuffer = [qMetal::Device::Get() newBufferWithLength:argumentBufferLength options:0];
				indexArgumentBuffer.label = [NSString stringWithFormat:@"%@ indice argument buffer", config->name];
				indexArgumentBuffers.push_back(indexArgumentBuffer);
				
				[indexArgumentEncoder setArgumentBuffer:indexArgumentBuffer offset:0];
				
				uint32_t *indexCount = (uint32_t*)[indexArgumentEncoder constantDataAtIndex:IndirectIndexCountIndex];
				
				*indexCount = it->GetConfig()->indexCount;
				[indexArgumentEncoder setBuffer:it->GetIndexBuffer() offset:0 atIndex:IndirectIndexStreamIndex];
				
				if (IndirectTessellationFactorBufferIndex != EmptyIndex)
				{
					[indexArgumentEncoder setBuffer:it->GetTessellationFactorsBuffer() offset:0 atIndex:IndirectTessellationFactorBufferIndex];
				}
				
				meshIndex++;
			}
			
			//COMPUTE PIPELINE THREADGROUPS
			
			threadsPerGrid = MTLSizeMake(config->count, 1, 1);
			threadsPerThreadgroup = MTLSizeMake(computePipelineState.maxTotalThreadsPerThreadgroup, 1, 1);
		}
		
		template<class _VertexParams, int _VertexTextureIndex, int _VertexParamsIndex, class _FragmentParams, int _FragmentTextureIndex, int _FragmentParamsIndex, class _ComputeParams, int _ComputeParamsIndex, class _InstanceParams, int _InstanceParamsIndex, bool _ForIndirectCommandBuffer>
		void Encode(id<MTLComputeCommandEncoder> encoder, const Material<_VertexParams, _VertexTextureIndex, _VertexParamsIndex, _FragmentParams, _FragmentTextureIndex, _FragmentParamsIndex, _ComputeParams, _ComputeParamsIndex, _InstanceParams, _InstanceParamsIndex, _ForIndirectCommandBuffer> *material)
		{
			[encoder setComputePipelineState:computePipelineState];
			
			[encoder useResource:indirectCommandBuffer usage:MTLResourceUsageWrite];
			[encoder useResource:commandBufferArgumentBuffer usage:MTLResourceUsageRead];
			
			[encoder setBuffer:commandBufferArgumentBuffer offset:0 atIndex:ICBArgumentBufferIndex];
			
			if (ICBComputeParamsIndex != EmptyIndex)
			{
				[encoder setBuffer:material->CurrentFrameComputeParamsBuffer() offset:0 atIndex:ICBComputeParamsIndex];
			}
			
			if (ICBVertexParamsIndex != EmptyIndex)
			{
				[encoder setBuffer:material->CurrentFrameVertexParamsBuffer() offset:0 atIndex:ICBVertexParamsIndex];
			}
			
			if (ICBVertexTextureIndex != EmptyIndex)
			{
				[encoder setBuffer:material->VertexTextureBuffer() offset:0 atIndex:ICBVertexTextureIndex];
			}
			
			if (ICBVertexInstanceParamsIndex != EmptyIndex)
			{
				[encoder setBuffer:vertexInstanceParamsBuffer offset:0 atIndex:ICBVertexInstanceParamsIndex];
			}
			
			if (material->FragmentFunction() != NULL)
			{
				//we need to check the fragment function as we may be in a shadow pass with a shared material, say
				if (ICBFragmentParamsIndex != EmptyIndex)
				{
					[encoder setBuffer:material->CurrentFrameFragmentParamsBuffer() offset:0 atIndex:ICBFragmentParamsIndex];
				}
				
				if (ICBFragmentTextureIndex != EmptyIndex)
				{
					[encoder setBuffer:material->FragmentTextureBuffer() offset:0 atIndex:ICBFragmentTextureIndex];
				}
			}

			int meshIndex = 0;
			for(auto &it : config->meshes)
			{
				[encoder setBuffer:indexArgumentBuffers[meshIndex] offset:0 atIndex:(ICBIndexArgumentBufferArrayIndex + meshIndex)];
				if (MeshVertexStreamIndex == EmptyIndex)
				{
					for(int streamIndex = 0; streamIndex < MeshVertexStreamCount; ++streamIndex)
					{
						[encoder setBuffer:it->GetVertexBuffer(streamIndex) offset:0 atIndex:(ICBVertexArgumentBufferArrayIndex + meshIndex * MeshVertexStreamIndex + streamIndex)];
					}
				}
				else
				{
					[encoder setBuffer:it->GetVertexArgumentBufferForMaterial(material) offset:0 atIndex:(ICBVertexArgumentBufferArrayIndex + meshIndex)];
				}
				meshIndex++;
			}
			
			[encoder dispatchThreads:threadsPerGrid threadsPerThreadgroup:threadsPerThreadgroup];
		}
		
		void Optimize()
		{
			//TODO could pass blit encoder in to collapse
			
			id<MTLBlitCommandEncoder> blitEncoder = qMetal::Device::BlitEncoder(@"Indirect Command Buffer Optimization");
			[blitEncoder optimizeIndirectCommandBuffer:indirectCommandBuffer withRange:NSMakeRange(0, config->count)];
			[blitEncoder endEncoding];
		}
		
		template<class _VertexParams, int _VertexTextureIndex, int _VertexParamsIndex, class _FragmentParams, int _FragmentTextureIndex, int _FragmentParamsIndex, class _ComputeParams, int _ComputeParamsIndex, class _InstanceParams, int _InstanceParamsIndex, bool _ForIndirectCommandBuffer>
        void Encode(id<MTLRenderCommandEncoder> encoder, const Material<_VertexParams, _VertexTextureIndex, _VertexParamsIndex, _FragmentParams, _FragmentTextureIndex, _FragmentParamsIndex, _ComputeParams, _ComputeParamsIndex, _InstanceParams, _InstanceParamsIndex, _ForIndirectCommandBuffer> *material)
        {
			material->Encode(encoder);
			
			for(auto &it : config->meshes)
			{
				it->UseResources(encoder);
			}
			
			[encoder executeCommandsInBuffer:indirectCommandBuffer withRange:NSMakeRange(0, config->count)];
        }
		
    private:
        Config              			*config;
		
		id<MTLIndirectCommandBuffer> 	indirectCommandBuffer;
		
		id <MTLComputePipelineState> 	computePipelineState;
		MTLSize 						threadsPerGrid;
		MTLSize 						threadsPerThreadgroup;
		
		id <MTLBuffer> 					commandBufferArgumentBuffer;
		id <MTLBuffer> 					vertexInstanceParamsBuffer;
		std::vector<id <MTLBuffer> >	indexArgumentBuffers;
    };
}

#endif //__Q_METAL_INDIRECT_MESH_H__

