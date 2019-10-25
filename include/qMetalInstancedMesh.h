/*
 *  GLIndirectMesh.h
 *  qMetal
 *
 *  Created by Russell Warneboldt on 10-03-19.
 *  Copyright 2010 Russell Warneboldt. All rights reserved.
 *
 */

#ifndef __Q_METAL_INDIRECT_MESH_H__
#define __Q_METAL_INDIRECT_MESH_H__

#define INDIRECT_USE_GPU (0)

#include <Metal/Metal.h>
#include "qCore.h"
#include "qMetalDevice.h"
#include "qMetalMesh.h"
#include "qMetalMaterial.h"
#include <vector>

namespace qMetal
{
    template<
		int IndirectCommandBufferIndex,	//where the indirect command buffer is in the kernel shader
		int IndirectMeshReindexIndex,	//where we re-index all the meshes, so that we can draw them > 1 time
		int IndirectIndexCountIndex,	//count of each meshes vertices
		int IndirectVertexStreamIndex,	//the vertex stream array of all the meshes
		int IndirectIndexStreamIndex,	//the vertex stream array of all the meshes
		int MeshVertexStreamCount,		//mesh details
		int MeshVertexStreamIndex
	>
    class IndirectMesh
    {        
    public:
		
		typedef struct MeshConfig
		{
			Mesh<MeshVertexStreamCount, MeshVertexStreamIndex>	*mesh;
			uint32_t 											count;
			
			MeshConfig(Mesh<MeshVertexStreamCount, MeshVertexStreamIndex> *_mesh, uint32_t _count)
			: mesh( _mesh )
			, count( _count )
			{
			}
		} MeshConfig;
		
        typedef struct Config
        {
			Function					*function;
			std::vector<MeshConfig*> 	meshConfigs;
			
            Config()
			: function( NULL )
            {}
        } Config;
		
        IndirectMesh(Config *_config)
        : config(_config)
#if !INDIRECT_USE_GPU
		, hasOptimized( false )
#endif
		{
			qASSERTM(config->meshConfigs.size() > 0, "Mesh config count can can not be zero");
			qASSERTM(config->meshConfigs.size() < 29, "Mesh config count can can not exceed %i", 29); //32 LIMIT, get based on device
			
			totalCount = 0;
			
			for(auto &it : config->meshConfigs)
			{
				totalCount += it->count;
			}
			
			// INDIRECT COMMAND BUFFER
			
			MTLIndirectCommandBufferDescriptor* indirectCommandBufferDescriptor = [[MTLIndirectCommandBufferDescriptor alloc] init];
			
			indirectCommandBufferDescriptor.commandTypes = MTLIndirectCommandTypeDraw;
			indirectCommandBufferDescriptor.inheritBuffers = false;
			indirectCommandBufferDescriptor.maxVertexBufferBindCount = MeshVertexStreamCount;
			indirectCommandBufferDescriptor.maxFragmentBufferBindCount = 0;
			
			indirectCommandBuffer = [qMetal::Device::Get() newIndirectCommandBufferWithDescriptor:indirectCommandBufferDescriptor maxCommandCount:totalCount options:0];
			
#if INDIRECT_USE_GPU
			// SET UP SOME BUFFERS
			std::vector<uint32_t> reindex;
			std::vector<uint32_t> indexCount;
			
			int meshIndex = 0;
			for(auto &it : config->meshConfigs)
			{
				for(int i = 0; i < it->count; ++i)
				{
					reindex.push_back(meshIndex);
				}
				indexCount.push_back(it->mesh->GetConfig()->indexCount);
				meshIndex++;
			}
			
			meshReindexBuffer = [qMetal::Device::Get() newBufferWithBytes:reindex.data() length:(reindex.size() * 4) options:MTLResourceStorageModeShared];
			indexCountBuffer = [qMetal::Device::Get() newBufferWithBytes:indexCount.data() length:(indexCount.size() * 4) options:MTLResourceStorageModeShared];
			
			// COMPUTE PIPELINE STATE FOR INDIRECT COMMAND BUFFER CONSTRUCTION
			
			NSError *error = nil;
			computePipelineState = [qMetal::Device::Get() newComputePipelineStateWithFunction:config->function->Get()
									 													error:&error];
			if (!computePipelineState)
			{
				NSLog(@"Failed to created indirect command buffer compute pipeline state, error %@", error);
			}
			
			//COMPUTE PIPELINE STATE ARGUMENT BUFFER
			
			id <MTLArgumentEncoder> argumentEncoder = [config->function->Get() newArgumentEncoderWithBufferIndex:0];
			NSUInteger argumentBufferLength = argumentEncoder.encodedLength;
			
			kernelArgumentBuffer = [qMetal::Device::Get() newBufferWithLength:argumentBufferLength options:0];
			kernelArgumentBuffer.label = @"Argument buffer for indirect command buffer"; //TODO more label. Debug only?
			
			[argumentEncoder setArgumentBuffer:kernelArgumentBuffer offset:0];
			[argumentEncoder setIndirectCommandBuffer:indirectCommandBuffer atIndex:IndirectCommandBufferIndex];
			[argumentEncoder setBuffer:meshReindexBuffer offset:0 atIndex:IndirectMeshReindexIndex];
			[argumentEncoder setBuffer:indexCountBuffer offset:0 atIndex:IndirectIndexCountIndex];
			
			meshIndex = 0;
			for(auto &it : config->meshConfigs)
			{
				for(int i = 0; i < MeshVertexStreamCount; ++i)
				{
					//we can't have indirect argument buffers (e.g. an argument buffer pointing to an argument buffer), so we do the individual streams.
					//this isn't ideal, as we can only have ~31 of them
					[argumentEncoder setBuffer:it->mesh->GetVertexBuffer(i) offset:0 atIndex:(IndirectVertexStreamIndex + (meshIndex * MeshVertexStreamCount) + i)];
				}
				
				[argumentEncoder setBuffer:it->mesh->GetIndexBuffer() offset:0 atIndex:(IndirectIndexStreamIndex + meshIndex)];
				
				meshIndex++;
			}
			
			//COMPUTE PIPELINE THREADGROUPS
			
			threadgroupSize = MTLSizeMake(totalCount, 1, 1);
			threadgroupCount = MTLSizeMake(1, 1, 1); //TODO this size could be picked better
			threadgroupCount.width  = totalCount;
			threadgroupCount.height = 1;
#endif
		}
		
		template<class VertexParams, int VertexTextureIndex, int VertexParamsIndex, class FragmentParams, int FragmentTextureIndex, int FragmentParamsIndex, class ComputeParams, int ComputeParamsIndex>
		void EncodeCPU(const Material<VertexParams, VertexTextureIndex, VertexParamsIndex, FragmentParams, FragmentTextureIndex, FragmentParamsIndex, ComputeParams, ComputeParamsIndex> *material)
		{
#if !INDIRECT_USE_GPU
			id<MTLIndirectRenderCommand> indirectRenderCommand = [indirectCommandBuffer indirectRenderCommandAtIndex:0];
			
			for(auto &it : config->meshConfigs)
			{
				for(int i = 0; i < it->count; ++i)
				{
					it->mesh->Encode(indirectRenderCommand, material);
				}
			}
#endif
		}
		
		void EncodeGPU(id<MTLComputeCommandEncoder> encoder)
		{
#if INDIRECT_USE_GPU
			[encoder setComputePipelineState:computePipelineState];
			[encoder useResource:indirectCommandBuffer usage:MTLResourceUsageWrite];
			[encoder useResource:meshReindexBuffer usage:MTLResourceUsageRead];
			[encoder useResource:indexCountBuffer usage:MTLResourceUsageRead];
			for(auto &it : config->meshConfigs)
			{
				it->mesh->UseResources(encoder);
			}
			[encoder setBuffer:kernelArgumentBuffer offset:0 atIndex:0];
			[encoder dispatchThreadgroups:threadgroupCount
						   threadsPerThreadgroup:threadgroupSize];
#endif
		}
		
		void Optimize()
		{
#if !INDIRECT_USE_GPU
			//we only do this once for the CPU, as we pre-create the command buffer
			if (hasOptimized)
			{
				return;
			}
#endif
			id<MTLBlitCommandEncoder> blitEncoder = [qMetal::Device::CurrentCommandBuffer() blitCommandEncoder];
			blitEncoder.label = @"Indirect Command Buffer Optimization";
			
			[blitEncoder optimizeIndirectCommandBuffer:indirectCommandBuffer withRange:NSMakeRange(0, totalCount)];
			[blitEncoder endEncoding];
			
#if !INDIRECT_USE_GPU
//			hasOptimized = true;
#endif
		}
		
		template<class VertexParams, int VertexTextureIndex, int VertexParamsIndex, class FragmentParams, int FragmentTextureIndex, int FragmentParamsIndex, class ComputeParams, int ComputeParamsIndex>
        void Encode(id<MTLRenderCommandEncoder> encoder, const Material<VertexParams, VertexTextureIndex, VertexParamsIndex, FragmentParams, FragmentTextureIndex, FragmentParamsIndex, ComputeParams, ComputeParamsIndex> *material)
        {
			material->Encode(encoder);
			
			for(auto &it : config->meshConfigs)
			{
				it->mesh->UseResources(encoder);
			}
			
//			[encoder executeCommandsInBuffer:indirectCommandBuffer withRange:NSMakeRange(0, totalCount)];
        }
		
    private:
        Config              *config;
		
		id<MTLIndirectCommandBuffer> indirectCommandBuffer;
		uint32_t 			totalCount;
		
#if INDIRECT_USE_GPU
		id <MTLComputePipelineState> computePipelineState;
		MTLSize 			threadgroupSize;
		MTLSize 			threadgroupCount;
		
		id <MTLBuffer> 		kernelArgumentBuffer;
		id <MTLBuffer> 		meshReindexBuffer;
		id <MTLBuffer> 		indexCountBuffer;
#else
		bool 				hasOptimized;
#endif
    };
}

#endif //__Q_METAL_INDIRECT_MESH_H__

