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
		//length kernel
		int ICBExecuationRangeIndex,				//where the structure with the MTLIndirectCommandBufferExecutionRange array lives
		int ICBExecuationRangeOffsetIndex,			//the uint offset into the MTLIndirectCommandBufferExecutionRange
    
		//command buffer kernel
		int ICBArgumentBufferIndex,					//where the indirect command buffer argument buffer is in the kernel shader
		int ICBComputeParamsIndex,					//where the compute params live
		int ICBTessellationFactorsRingBufferIndex,	//where the tessellation factors ring buffer index lives
		int ICBVertexParamsIndex,					//where the vertex params live
		int ICBVertexTextureIndex,					//where the vertex texture live
		class ICBVertexInstanceParams,				//type of vertex instance params
		int ICBVertexInstanceParamsIndex,			//where the vertex instance params live
		int ICBFragmentParamsIndex,					//where the fragment params live
		int ICBFragmentTextureIndex,				//where the fragment texture live
		int ICBInstanceArgumentBufferArrayIndex,	//where the index and tessellation argument buffer array lives
		int ICBVertexArgumentBufferArrayIndex,		//where the vertex argument buffer array lives

		//command buffer argument buffers
		int IndirectVertexIndexCountIndex,			//where the number of indices lives
		int IndirectIndexStreamIndex,				//where the index buffer lives
		int IndirectTessellationFactorBufferIndex,	//where the tessellation factor buffer lives
		int IndirectTessellationFactorCountIndex,	//where the tessellation factor limit lives

		//mesh details
		int MeshVertexStreamCount,
		int MeshVertexStreamIndex,
		int TessellationStreamCount = 0,
		int TessellationFactorsIndex = EmptyIndex
	>
    class IndirectMesh
    {        
    public:
		
        typedef struct Config
        {
            NSString*           		name;
			Function*					function;
			Function*					ringClearFunction;
			std::vector<Mesh<MeshVertexStreamCount, MeshVertexStreamIndex, TessellationStreamCount, TessellationFactorsIndex>*> 	meshes;
			NSUInteger					count;
			
            Config(NSString* _name)
			: name([_name retain])
			, function(NULL)
			, ringClearFunction(NULL)
            {}
			
			Config(Config* config, NSString* _name)
			: name([_name retain])
			, function(config->function)
			, ringClearFunction(config->ringClearFunction)
			, meshes(config->meshes)
			, count(config->count)
			{}
        } Config;
		
		IndirectMesh(Config *_config)
        : config(_config)
        , ringClearComputePipelineState(nil)
        , rangeInitComputePipelineState(nil)
		{
			qASSERTM(config->meshes.size() > 0, "Mesh config count can can not be zero");
			qASSERTM(config->meshes.size() < 14, "Mesh config count can can not exceed %i", 29); //32 LIMIT, get based on device
			qASSERTM(config->meshes[0]->GetConfig()->IsIndexed() || (IndirectIndexStreamIndex == EmptyIndex), "Mesh isn't indexed but you supplied an index stream index");
			qASSERTM(!config->meshes[0]->GetConfig()->IsIndexed() || (IndirectIndexStreamIndex != EmptyIndex), "Mesh is indexed but you didn't supply an index stream index");
			qASSERTM(config->ringClearFunction || (ICBTessellationFactorsRingBufferIndex == EmptyIndex), "Ring buffer clear function isn't provided but you supplied a ring buffer index");
			qASSERTM(!config->ringClearFunction || (ICBTessellationFactorsRingBufferIndex != EmptyIndex), "Ring buffer clear function provided but you didn't supply a ring buffer index");
			
			//TODO is there a more optimal size (multiple of threadgroups?) than this?
			dimension = ceilf(sqrtf(config->count));
			config->count = dimension * dimension;
			
			// SET UP SOME BUFFERS
			
			if (ICBTessellationFactorsRingBufferIndex != EmptyIndex)
			{
				uint buffer = 0;
				tessellationFactorsRingBuffer = [qMetal::Device::Get() newBufferWithBytes:&buffer length:sizeof(uint) options:0];
				tessellationFactorsRingBuffer.label = [NSString stringWithFormat:@"%@ tessellation factors ring buffer", config->name];
			}
			
			indirectRangeOffset = qMetal::Device::NextIndirectRangeOffset();
			indirectRangeOffsetBuffer = [qMetal::Device::Get() newBufferWithBytes:&indirectRangeOffset length:sizeof(uint) options:0];
			indirectRangeOffsetBuffer.label = [NSString stringWithFormat:@"%@ indirect range offset buffer", config->name];
				
			if (ICBVertexInstanceParamsIndex != EmptyIndex)
			{
				vertexInstanceParamsBuffer = [qMetal::Device::Get() newBufferWithLength:(sizeof(ICBVertexInstanceParams) * (dimension * dimension)) options:0];
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
			
			// COMPUTE PIPELINE STATE FOR CLEARING RING BUFFER
			
			if (config->ringClearFunction != nil)
			{
				ringClearComputePipelineState = [qMetal::Device::Get() newComputePipelineStateWithFunction:config->ringClearFunction->Get()
																									 error:&error];
				if (!ringClearComputePipelineState)
				{
					NSLog(@"Failed to created ring alloc compute pipeline state, error %@", error);
				}
			}
			
			//COMPUTE PIPELINE STATE ARGUMENT BUFFER
			
			id <MTLArgumentEncoder> argumentEncoder = [config->function->Get() newArgumentEncoderWithBufferIndex:ICBArgumentBufferIndex];
			NSUInteger argumentBufferLength = argumentEncoder.encodedLength;
			
			commandBufferArgumentBuffer = [qMetal::Device::Get() newBufferWithLength:argumentBufferLength options:0];
			commandBufferArgumentBuffer.label = [NSString stringWithFormat:@"%@ ICB argument buffer", config->name];
			
			[argumentEncoder setArgumentBuffer:commandBufferArgumentBuffer offset:0];
			[argumentEncoder setIndirectCommandBuffer:qMetal::Device::IndirectCommandBuffer() atIndex:ICBArgumentBufferIndex];
			
			int meshIndex = 0;
			for(auto &it : config->meshes)
			{
				id <MTLArgumentEncoder> instanceArgumentEncoder = [config->function->Get() newArgumentEncoderWithBufferIndex:(ICBInstanceArgumentBufferArrayIndex + meshIndex)];
				NSUInteger argumentBufferLength = instanceArgumentEncoder.encodedLength;
				
				id <MTLBuffer> instanceArgumentBuffer = [qMetal::Device::Get() newBufferWithLength:argumentBufferLength options:0];
				instanceArgumentBuffer.label = [NSString stringWithFormat:@"%@ instance argument buffer", config->name];
				indexArgumentBuffers.push_back(instanceArgumentBuffer);
				
				[instanceArgumentEncoder setArgumentBuffer:instanceArgumentBuffer offset:0];
				
				if (IndirectIndexStreamIndex != EmptyIndex)
				{
					const bool isQuad = it->GetConfig()->IsQuadIndexed() && (IndirectTessellationFactorBufferIndex != EmptyIndex);
				
					uint32_t *indexCount = (uint32_t*)[instanceArgumentEncoder constantDataAtIndex:IndirectVertexIndexCountIndex];
					*indexCount = isQuad ? it->GetConfig()->quadIndexCount : it->GetConfig()->indexCount;
					
					[instanceArgumentEncoder setBuffer:(isQuad ? it->GetQuadIndexBuffer() : it->GetIndexBuffer()) offset:0 atIndex:IndirectIndexStreamIndex];
				}
				else
				{
					uint32_t *vertexCount = (uint32_t*)[instanceArgumentEncoder constantDataAtIndex:IndirectVertexIndexCountIndex];
					*vertexCount = it->GetConfig()->vertexCount;
				}
				
				if (IndirectTessellationFactorBufferIndex != EmptyIndex)
				{
					[instanceArgumentEncoder setBuffer:it->GetTessellationFactorsBuffer() offset:0 atIndex:IndirectTessellationFactorBufferIndex];
				}
				
				if (IndirectTessellationFactorCountIndex != EmptyIndex)
				{
					uint32_t *factorCount = (uint32_t*)[instanceArgumentEncoder constantDataAtIndex:IndirectTessellationFactorCountIndex];
					*factorCount = it->GetTessellationFactorsCount();
				}
				
				meshIndex++;
			}
			
			//COMPUTE PIPELINE THREADGROUPS
			
			threadsPerGrid = MTLSizeMake(dimension, dimension, 1);
			threadsPerThreadgroup = MTLSizeMake(computePipelineState.threadExecutionWidth, computePipelineState.maxTotalThreadsPerThreadgroup / computePipelineState.threadExecutionWidth, 1);
		}
		
		template<class _VertexParams, int _VertexTextureIndex, int _VertexParamsIndex, class _FragmentParams, int _FragmentTextureIndex, int _FragmentParamsIndex, class _ComputeParams, int _ComputeParamsIndex, class _InstanceParams, int _InstanceParamsIndex>
		void Encode(id<MTLComputeCommandEncoder> encoder, const Material<_VertexParams, _VertexTextureIndex, _VertexParamsIndex, _FragmentParams, _FragmentTextureIndex, _FragmentParamsIndex, _ComputeParams, _ComputeParamsIndex, _InstanceParams, _InstanceParamsIndex> *material)
		{
			if (ringClearComputePipelineState != nil)
			{
				NSString *debugName = [NSString stringWithFormat:@"%@ Tessellation Ring Clear", config->name];
				[encoder pushDebugGroup:debugName];
				[encoder setComputePipelineState:ringClearComputePipelineState];
				[encoder setBuffer:tessellationFactorsRingBuffer offset:0 atIndex:ICBTessellationFactorsRingBufferIndex];
				[encoder dispatchThreads:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
				[encoder popDebugGroup];
			}
			
			qMetal::Device::InitIndirectCommandBuffer(encoder, indirectRangeOffsetBuffer);
			
			[encoder memoryBarrierWithScope:MTLBarrierScopeBuffers];
			
			NSString *debugName = [NSString stringWithFormat:@"%@ ICB Compute Encode", config->name];
			[encoder pushDebugGroup:debugName];
		
			[encoder setComputePipelineState:computePipelineState];
			
			material->EncodeTextures(encoder);
			
			[encoder useResource:qMetal::Device::IndirectCommandBuffer() usage:MTLResourceUsageWrite];
			
			[encoder setBuffer:commandBufferArgumentBuffer offset:0 atIndex:ICBArgumentBufferIndex];
			
			[encoder setBuffer:qMetal::Device::IndirectRangeBuffer() offset:0 atIndex:ICBExecuationRangeIndex];
			
			[encoder setBuffer:indirectRangeOffsetBuffer offset:0 atIndex:ICBExecuationRangeOffsetIndex];
			
			if (ICBComputeParamsIndex != EmptyIndex)
			{
				[encoder setBuffer:material->CurrentFrameComputeParamsBuffer() offset:0 atIndex:ICBComputeParamsIndex];
			}
			
			if (ICBTessellationFactorsRingBufferIndex != EmptyIndex)
			{
				[encoder setBuffer:tessellationFactorsRingBuffer offset:0 atIndex:ICBTessellationFactorsRingBufferIndex];
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

			const bool useVertexArgumentBuffers = MeshVertexStreamIndex != EmptyIndex;
			int meshIndex = 0;
			for(auto &it : config->meshes)
			{
				it->UseResources(encoder, useVertexArgumentBuffers);
				
				if (IndirectIndexStreamIndex != EmptyIndex || IndirectTessellationFactorBufferIndex != EmptyIndex)
				{
					[encoder setBuffer:indexArgumentBuffers[meshIndex] offset:0 atIndex:(ICBInstanceArgumentBufferArrayIndex + meshIndex)];
				}
				
				if (useVertexArgumentBuffers)
				{
					[encoder setBuffer:it->GetVertexArgumentBufferForMaterial(material) offset:0 atIndex:(ICBVertexArgumentBufferArrayIndex + meshIndex)];
				}
				else
				{
					for(int streamIndex = 0; streamIndex < MeshVertexStreamCount; ++streamIndex)
					{
						[encoder setBuffer:it->GetVertexBuffer(streamIndex) offset:0 atIndex:(ICBVertexArgumentBufferArrayIndex + meshIndex * MeshVertexStreamCount + streamIndex)];
					}
				}
				meshIndex++;
			}
			
			[encoder dispatchThreads:threadsPerGrid threadsPerThreadgroup:threadsPerThreadgroup];
			[encoder popDebugGroup];
		}
		
		template<class _VertexParams, int _VertexTextureIndex, int _VertexParamsIndex, class _FragmentParams, int _FragmentTextureIndex, int _FragmentParamsIndex, class _ComputeParams, int _ComputeParamsIndex, class _InstanceParams, int _InstanceParamsIndex>
        void Encode(id<MTLRenderCommandEncoder> encoder, const Material<_VertexParams, _VertexTextureIndex, _VertexParamsIndex, _FragmentParams, _FragmentTextureIndex, _FragmentParamsIndex, _ComputeParams, _ComputeParamsIndex, _InstanceParams, _InstanceParamsIndex> *material)
        {
			NSString *debugName = [NSString stringWithFormat:@"%@ ICB render encode", config->name];
			[encoder pushDebugGroup:debugName];
			if (ICBVertexInstanceParamsIndex != EmptyIndex)
			{
				[encoder useResource:vertexInstanceParamsBuffer usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
			}
			
			material->Encode(encoder);
			
			for(auto &it : config->meshes)
			{
				it->UseResources(encoder);
			}
			
			qMetal::Device::ExecuteIndirectCommandBuffer(encoder, indirectRangeOffset);
			
			[encoder popDebugGroup];
        }
		
    private:
        Config              			*config;
		
		id <MTLComputePipelineState> 	computePipelineState;
		id <MTLComputePipelineState> 	ringClearComputePipelineState;
		id <MTLComputePipelineState> 	rangeInitComputePipelineState;
        NSInteger						dimension;
		MTLSize 						threadsPerGrid;
		MTLSize 						threadsPerThreadgroup;
		
		id <MTLBuffer> 					commandBufferArgumentBuffer;
		id <MTLBuffer> 					tessellationFactorsRingBuffer;
		id <MTLBuffer> 					vertexInstanceParamsBuffer;
		std::vector<id <MTLBuffer> >	indexArgumentBuffers;
		
		uint32_t						indirectRangeOffset;
		id <MTLBuffer> 					indirectRangeOffsetBuffer;
    };
}

#endif //__Q_METAL_INDIRECT_MESH_H__

