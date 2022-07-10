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
    template<class ICBVertexInstanceParams>
    class IndirectMesh
    {        
    public:
		
        typedef struct Config
        {
            NSString*           	name;
			Function*				function;
			Function*				ringClearFunction;
			std::vector<Mesh*> 		meshes;
			NSUInteger				count;
			
			//length kernel
			int32_t executionRangeIndex;					//where the structure with the MTLIndirectCommandBufferExecutionRange array lives
			int32_t executionRangeOffsetIndex;				//the uint offset into the MTLIndirectCommandBufferExecutionRange
			int32_t executionTessellationRangeIndex;		//where the structure with the MTLIndirectCommandBufferExecutionRange array lives for tessellated draws
			int32_t executionTessellationRangeOffsetIndex;	//the uint offset into the MTLIndirectCommandBufferExecutionRange for tessellated draws

			//command buffer kernel
			int32_t argumentBufferIndex; 					//where the indirect command buffer argument buffer is in the kernel shader
			int32_t computeParamsIndex;						//where the compute params live
			int32_t tessellationFactorsRingBufferIndex;		//where the tessellation factors ring buffer index lives
			int32_t vertexParamsIndex;						//where the vertex params live
			int32_t vertexTextureIndex;						//where the vertex texture live
			int32_t vertexInstanceParamsIndex;				//where the vertex instance params live
			int32_t fragmentParamsIndex;					//where the fragment params live
			int32_t fragmentTextureIndex;					//where the fragment texture live
			int32_t instanceArgumentBufferArrayIndex;		//where the index and tessellation argument buffer array lives
			int32_t vertexArgumentBufferArrayIndex;			//where the vertex argument buffer array lives
	
			//command buffer argument buffers
			int32_t indirectVertexIndexCountIndex;			//where the number of tri indices lives
			int32_t indirectVertexIndexCountQuadIndex;		//where the number of quad indices lives
			int32_t indirectIndexStreamIndex;				//where the tri index buffer lives
			int32_t indirectIndexStreamQuadIndex;			//where the quad index buffer lives
			int32_t indirectTessellationFactorBufferIndex;	//where the tessellation factor buffer lives
			int32_t indirectTessellationFactorCountIndex;	//where the tessellation factor limit lives
			
            Config(NSString* _name)
			: name([_name retain])
			, function(NULL)
			, ringClearFunction(NULL)
			, executionRangeIndex(EmptyIndex)
			, executionRangeOffsetIndex(EmptyIndex)
			, executionTessellationRangeIndex(EmptyIndex)
			, executionTessellationRangeOffsetIndex(EmptyIndex)
			, argumentBufferIndex(EmptyIndex)
			, computeParamsIndex(EmptyIndex)
			, tessellationFactorsRingBufferIndex(EmptyIndex)
			, vertexParamsIndex(EmptyIndex)
			, vertexTextureIndex(EmptyIndex)
			, vertexInstanceParamsIndex(EmptyIndex)
			, fragmentParamsIndex(EmptyIndex)
			, fragmentTextureIndex(EmptyIndex)
			, instanceArgumentBufferArrayIndex(EmptyIndex)
			, vertexArgumentBufferArrayIndex(EmptyIndex)
			, indirectVertexIndexCountIndex(EmptyIndex)
			, indirectVertexIndexCountQuadIndex(EmptyIndex)
			, indirectIndexStreamIndex(EmptyIndex)
			, indirectIndexStreamQuadIndex(EmptyIndex)
			, indirectTessellationFactorBufferIndex(EmptyIndex)
			, indirectTessellationFactorCountIndex(EmptyIndex)
            { }
			
			Config(Config* config, NSString* _name)
			: name([_name retain])
			, function(config->function)
			, ringClearFunction(config->ringClearFunction)
			, meshes(config->meshes)
			, count(config->count)
			, executionRangeIndex(config->executionRangeIndex)
			, executionRangeOffsetIndex(config->executionRangeOffsetIndex)
			, executionTessellationRangeIndex(config->executionTessellationRangeIndex)
			, executionTessellationRangeOffsetIndex(config->executionTessellationRangeOffsetIndex)
			, argumentBufferIndex(config->argumentBufferIndex)
			, computeParamsIndex(config->computeParamsIndex)
			, tessellationFactorsRingBufferIndex(config->tessellationFactorsRingBufferIndex)
			, vertexParamsIndex(config->vertexParamsIndex)
			, vertexTextureIndex(config->vertexTextureIndex)
			, vertexInstanceParamsIndex(config->vertexInstanceParamsIndex)
			, fragmentParamsIndex(config->fragmentParamsIndex)
			, fragmentTextureIndex(config->fragmentTextureIndex)
			, instanceArgumentBufferArrayIndex(config->instanceArgumentBufferArrayIndex)
			, vertexArgumentBufferArrayIndex(config->vertexArgumentBufferArrayIndex)
			, indirectVertexIndexCountIndex(config->indirectVertexIndexCountIndex)
			, indirectVertexIndexCountQuadIndex(config->indirectVertexIndexCountQuadIndex)
			, indirectIndexStreamIndex(config->indirectIndexStreamIndex)
			, indirectIndexStreamQuadIndex(config->indirectIndexStreamQuadIndex)
			, indirectTessellationFactorBufferIndex(config->indirectTessellationFactorBufferIndex)
			, indirectTessellationFactorCountIndex(config->indirectTessellationFactorCountIndex)
			{ }
        } Config;
		
		IndirectMesh(Config *_config)
        : config(_config)
        , ringClearComputePipelineState(nil)
        , rangeInitComputePipelineState(nil)
		{
			qASSERTM(config->meshes.size() > 0, "Mesh config count can can not be zero");
			qASSERTM(config->meshes.size() < 14, "Mesh config count can can not exceed %i", 29); //32 LIMIT, get based on device
			qASSERTM(config->meshes[0]->GetConfig()->IsIndexed() || (config->indirectIndexStreamIndex == EmptyIndex), "Mesh isn't indexed but you supplied an index stream index");
			qASSERTM(!config->meshes[0]->GetConfig()->IsIndexed() || (config->indirectIndexStreamIndex != EmptyIndex), "Mesh is indexed but you didn't supply an index stream index");
			qASSERTM(config->ringClearFunction || (config->tessellationFactorsRingBufferIndex == EmptyIndex), "Ring buffer clear function isn't provided but you supplied a ring buffer index");
			qASSERTM(!config->ringClearFunction || (config->tessellationFactorsRingBufferIndex != EmptyIndex), "Ring buffer clear function provided but you didn't supply a ring buffer index");
			
			//TODO is there a more optimal size (multiple of threadgroups?) than this?
			dimension = ceilf(sqrtf(config->count));
			config->count = dimension * dimension;
			
			// SET UP SOME BUFFERS
			
			if (config->tessellationFactorsRingBufferIndex != EmptyIndex)
			{
				uint buffer = 0;
				tessellationFactorsRingBuffer = [qMetal::Device::Get() newBufferWithBytes:&buffer length:sizeof(uint) options:0];
				tessellationFactorsRingBuffer.label = [NSString stringWithFormat:@"%@ tessellation factors ring buffer", config->name];
			}
			
			indirectRangeOffset = qMetal::Device::NextIndirectRangeOffset(Device::eIndirectCommandBufferPool_Untessellated);
			indirectRangeOffsetBuffer = [qMetal::Device::Get() newBufferWithBytes:&indirectRangeOffset length:sizeof(uint) options:0];
			indirectRangeOffsetBuffer.label = [NSString stringWithFormat:@"%@ indirect range offset buffer", config->name];
			
			if (config->tessellationFactorsRingBufferIndex != EmptyIndex)
			{
				indirectTessellationRangeOffset = qMetal::Device::NextIndirectRangeOffset(Device::eIndirectCommandBufferPool_Tessellated);
				indirectTessellationRangeOffsetBuffer = [qMetal::Device::Get() newBufferWithBytes:&indirectTessellationRangeOffset length:sizeof(uint) options:0];
				indirectTessellationRangeOffsetBuffer.label = [NSString stringWithFormat:@"%@ indirect tesselation range offset buffer", config->name];
			}
				
			if (config->vertexInstanceParamsIndex != EmptyIndex)
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
				
			id <MTLArgumentEncoder> argumentEncoder = [config->function->Get() newArgumentEncoderWithBufferIndex:config->argumentBufferIndex];
			NSUInteger argumentBufferLength = argumentEncoder.encodedLength;
			
			commandBufferArgumentBuffer = [qMetal::Device::Get() newBufferWithLength:argumentBufferLength options:0];
			commandBufferArgumentBuffer.label = [NSString stringWithFormat:@"%@ ICB argument buffer pool", config->name];
			
			[argumentEncoder setArgumentBuffer:commandBufferArgumentBuffer offset:0];
			
			//COMPUTE PIPELINE STATE ARGUMENT BUFFER
			for(uint32_t poolIndex = 0; poolIndex < Device::eIndirectCommandBufferPool_Count; ++poolIndex)
			{
				if (((Device::eIndirectCommandBufferPool)poolIndex == Device::eIndirectCommandBufferPool_Tessellated) && (config->tessellationFactorsRingBufferIndex == EmptyIndex))
				{
					continue;
				}
				[argumentEncoder setIndirectCommandBuffer:qMetal::Device::IndirectCommandBuffer((Device::eIndirectCommandBufferPool)poolIndex) atIndex:poolIndex];
			}
			
			int meshIndex = 0;
			for(auto &it : config->meshes)
			{
				id <MTLArgumentEncoder> instanceArgumentEncoder = [config->function->Get() newArgumentEncoderWithBufferIndex:(config->instanceArgumentBufferArrayIndex + meshIndex)];
				NSUInteger argumentBufferLength = instanceArgumentEncoder.encodedLength;
				
				id <MTLBuffer> instanceArgumentBuffer = [qMetal::Device::Get() newBufferWithLength:argumentBufferLength options:0];
				instanceArgumentBuffer.label = [NSString stringWithFormat:@"%@ instance argument buffer", config->name];
				indexArgumentBuffers.push_back(instanceArgumentBuffer);
				
				[instanceArgumentEncoder setArgumentBuffer:instanceArgumentBuffer offset:0];
				
				if (config->indirectIndexStreamIndex != EmptyIndex || config->indirectIndexStreamQuadIndex != EmptyIndex)
				{
					if (config->indirectIndexStreamIndex != EmptyIndex)
					{
						uint32_t *indexCount = (uint32_t*)[instanceArgumentEncoder constantDataAtIndex:config->indirectVertexIndexCountIndex];
						*indexCount = it->GetConfig()->indexCount;
						
						[instanceArgumentEncoder setBuffer:it->GetIndexBuffer() offset:0 atIndex:config->indirectIndexStreamIndex];
					}
					if (config->indirectIndexStreamQuadIndex != EmptyIndex)
					{
						uint32_t *indexCount = (uint32_t*)[instanceArgumentEncoder constantDataAtIndex:config->indirectVertexIndexCountQuadIndex];
						*indexCount = it->GetConfig()->quadIndexCount;
						
						[instanceArgumentEncoder setBuffer:it->GetQuadIndexBuffer() offset:0 atIndex:config->indirectIndexStreamQuadIndex];
					}
				}
				else
				{
					uint32_t *vertexCount = (uint32_t*)[instanceArgumentEncoder constantDataAtIndex:config->indirectVertexIndexCountIndex];
					*vertexCount = it->GetConfig()->vertexCount;
				}
				
				if (config->indirectTessellationFactorBufferIndex != EmptyIndex)
				{
					[instanceArgumentEncoder setBuffer:it->GetTessellationFactorsBuffer() offset:0 atIndex:config->indirectTessellationFactorBufferIndex];
				}
				
				if (config->indirectTessellationFactorCountIndex != EmptyIndex)
				{
					uint32_t *factorCount = (uint32_t*)[instanceArgumentEncoder constantDataAtIndex:config->indirectTessellationFactorCountIndex];
					*factorCount = it->GetTessellationFactorsCount();
				}
				
				meshIndex++;
			}
			
			//COMPUTE PIPELINE THREADGROUPS
			
			threadsPerGrid = MTLSizeMake(dimension, dimension, 1);
			threadsPerThreadgroup = MTLSizeMake(computePipelineState.threadExecutionWidth, computePipelineState.maxTotalThreadsPerThreadgroup / computePipelineState.threadExecutionWidth, 1);
		}
		
		template<class _VertexParams, class _FragmentParams, class _ComputeParams, class _InstanceParams>
		void Encode(id<MTLComputeCommandEncoder> encoder, const Material<_VertexParams, _FragmentParams, _ComputeParams, _InstanceParams> *material)
		{
			if (ringClearComputePipelineState != nil)
			{
				NSString *debugName = [NSString stringWithFormat:@"%@ Tessellation Ring Clear", config->name];
				[encoder pushDebugGroup:debugName];
				[encoder setComputePipelineState:ringClearComputePipelineState];
				[encoder setBuffer:tessellationFactorsRingBuffer offset:0 atIndex:config->tessellationFactorsRingBufferIndex];
				[encoder dispatchThreads:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
				[encoder popDebugGroup];
			}
			
			qMetal::Device::InitIndirectCommandBuffer(Device::eIndirectCommandBufferPool_Untessellated, encoder, indirectRangeOffsetBuffer);
			
			if (config->tessellationFactorsRingBufferIndex != EmptyIndex)
			{
				qMetal::Device::InitIndirectCommandBuffer(Device::eIndirectCommandBufferPool_Tessellated, encoder, indirectTessellationRangeOffsetBuffer);
			}
			
			[encoder memoryBarrierWithScope:MTLBarrierScopeBuffers];
			
			NSString *debugName = [NSString stringWithFormat:@"%@ ICB Compute Encode", config->name];
			[encoder pushDebugGroup:debugName];
		
			[encoder setComputePipelineState:computePipelineState];
			
			material->EncodeTextures(encoder);
			
			[encoder setBuffer:commandBufferArgumentBuffer offset:0 atIndex:config->argumentBufferIndex];
			
			[encoder useResource:qMetal::Device::IndirectCommandBuffer(Device::eIndirectCommandBufferPool_Untessellated) usage:MTLResourceUsageWrite];
			[encoder setBuffer:qMetal::Device::IndirectRangeBuffer(Device::eIndirectCommandBufferPool_Untessellated) offset:0 atIndex:config->executionRangeIndex];
			[encoder setBuffer:indirectRangeOffsetBuffer offset:0 atIndex:config->executionRangeOffsetIndex];
			
			if (config->tessellationFactorsRingBufferIndex != EmptyIndex)
			{
				[encoder useResource:qMetal::Device::IndirectCommandBuffer(Device::eIndirectCommandBufferPool_Tessellated) usage:MTLResourceUsageWrite];
				[encoder setBuffer:qMetal::Device::IndirectRangeBuffer(Device::eIndirectCommandBufferPool_Tessellated) offset:0 atIndex:config->executionTessellationRangeIndex];
				[encoder setBuffer:indirectTessellationRangeOffsetBuffer offset:0 atIndex:config->executionTessellationRangeOffsetIndex];
			}
			
			if (config->computeParamsIndex != EmptyIndex)
			{
				[encoder setBuffer:material->CurrentFrameComputeParamsBuffer() offset:0 atIndex:config->computeParamsIndex];
			}
			
			if (config->tessellationFactorsRingBufferIndex != EmptyIndex)
			{
				[encoder setBuffer:tessellationFactorsRingBuffer offset:0 atIndex:config->tessellationFactorsRingBufferIndex];
			}
			
			if (config->vertexParamsIndex != EmptyIndex)
			{
				[encoder setBuffer:material->CurrentFrameVertexParamsBuffer() offset:0 atIndex:config->vertexParamsIndex];
			}
			
			if (config->vertexTextureIndex != EmptyIndex)
			{
				[encoder setBuffer:material->VertexTextureBuffer() offset:0 atIndex:config->vertexTextureIndex];
			}
			
			if (config->vertexInstanceParamsIndex != EmptyIndex)
			{
				[encoder setBuffer:vertexInstanceParamsBuffer offset:0 atIndex:config->vertexInstanceParamsIndex];
			}
			
			if (material->FragmentFunction() != NULL)
			{
				//we need to check the fragment function as we may be in a shadow pass with a shared material, say
				if (config->fragmentParamsIndex != EmptyIndex)
				{
					[encoder setBuffer:material->CurrentFrameFragmentParamsBuffer() offset:0 atIndex:config->fragmentParamsIndex];
				}
				
				if (config->fragmentTextureIndex != EmptyIndex)
				{
					[encoder setBuffer:material->FragmentTextureBuffer() offset:0 atIndex:config->fragmentTextureIndex];
				}
			}

			const bool useVertexArgumentBuffers = config->meshes[0]->GetConfig()->vertexStreamIndex != EmptyIndex;
			int meshIndex = 0;
			for(auto &it : config->meshes)
			{
				it->UseResources(encoder, useVertexArgumentBuffers);
				
				if (config->indirectIndexStreamIndex != EmptyIndex || config->indirectTessellationFactorBufferIndex != EmptyIndex)
				{
					[encoder setBuffer:indexArgumentBuffers[meshIndex] offset:0 atIndex:(config->instanceArgumentBufferArrayIndex + meshIndex)];
				}
				
				if (useVertexArgumentBuffers)
				{
					[encoder setBuffer:it->GetVertexArgumentBufferForMaterial(material) offset:0 atIndex:(config->vertexArgumentBufferArrayIndex + meshIndex)];
				}
				else
				{
					const uint32_t vertexStreamCount = config->meshes[0]->GetConfig()->vertexStreamCount;
					for(int streamIndex = 0; streamIndex < vertexStreamCount; ++streamIndex)
					{
						[encoder setBuffer:it->GetVertexBuffer(streamIndex) offset:0 atIndex:(config->vertexArgumentBufferArrayIndex + meshIndex * vertexStreamCount + streamIndex)];
					}
				}
				meshIndex++;
			}
			
			[encoder dispatchThreads:threadsPerGrid threadsPerThreadgroup:threadsPerThreadgroup];
			[encoder popDebugGroup];
		}
		
		template<class _VertexParams, class _FragmentParams, class _ComputeParams, class _InstanceParams>
        void Encode(id<MTLRenderCommandEncoder> encoder,
			const Material<_VertexParams, _FragmentParams, _ComputeParams, _InstanceParams> *material,
			const Material<_VertexParams, _FragmentParams, _ComputeParams, _InstanceParams> *tessellatedMaterial = NULL)
        {
			NSString *debugName = [NSString stringWithFormat:@"%@ ICB render encode", config->name];
			[encoder pushDebugGroup:debugName];
			if (config->vertexInstanceParamsIndex != EmptyIndex)
			{
				[encoder useResource:vertexInstanceParamsBuffer usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
			}
			
			for(auto &it : config->meshes)
			{
				it->UseResources(encoder);
			}
			
			material->Encode(encoder);
			qMetal::Device::ExecuteIndirectCommandBuffer(Device::eIndirectCommandBufferPool_Untessellated, encoder, indirectRangeOffset);
			
			if (tessellatedMaterial)
			{
				qASSERTM(config->tessellationFactorsRingBufferIndex != EmptyIndex, "Trying to render with a tessellated material without providing tessellation set-up parameters")
				tessellatedMaterial->Encode(encoder);
				qMetal::Device::ExecuteIndirectCommandBuffer(Device::eIndirectCommandBufferPool_Tessellated, encoder, indirectTessellationRangeOffset);
			}
			
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
		
		uint32_t						indirectTessellationRangeOffset;
		id <MTLBuffer> 					indirectTessellationRangeOffsetBuffer;
    };
}

#endif //__Q_METAL_INDIRECT_MESH_H__

