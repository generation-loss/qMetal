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

#ifndef __Q_METAL_MESH_H__
#define __Q_METAL_MESH_H__

#include <Metal/Metal.h>
#include "qCore.h"
#include "qMetalDevice.h"
#include "qMetalMaterial.h"
#include <map>

namespace qMetal
{
    template<int VertexStreamCount, int VertexStreamIndex = EmptyIndex, NSUInteger LODCount = 1, int TessellationStreamCount = 0, int TessellationStreamFactorsIndex = EmptyIndex>
    class Mesh
    {        
    public:
		
		enum ePrimitiveType
		{
			ePrimitiveType_Triangle = MTLPrimitiveTypeTriangle,
			ePrimitiveType_Point = MTLPrimitiveTypePoint
		};
		
		enum eVertexStreamType
		{
			//enum value = bytes
			eVertexStreamType_Unset = -1,
			eVertexStreamType_UHalfInt = 2,
			eVertexStreamType_UInt = 4,
			eVertexStreamType_UInt2 = 8,
			eVertexStreamType_UInt3 = 12, //WARNING alignment differs between metal and SIMD (unpacked on CPU, packed on GPU), need to augument with qMath qVectorU?
			eVertexStreamType_UInt4 = 16,
			eVertexStreamType_Float	= 4,
			eVertexStreamType_Float2 = 8,
			eVertexStreamType_Float3 = 12,
			eVertexStreamType_Float4 = 16,
			eVertexStreamType_Half = 2,
			eVertexStreamType_Half2 = 4,
			eVertexStreamType_Half3 = 6,
			eVertexStreamType_Half4 = 8,
		};
		
		typedef struct VertexStream
		{
			eVertexStreamType type;
			void* data;
			NSUInteger count;
			
			VertexStream()
			: type(eVertexStreamType_Unset)
			, data(NULL)
			, count(0)
			{}
			
		} VertexStream;
		
        typedef struct Config
        {
            char*           			name;
			ePrimitiveType				primitiveType;
            VertexStream				tessellationStreams[TessellationStreamCount];
            VertexStream				vertexStreams[VertexStreamCount];
            NSUInteger	      			vertexCount;
            uint16_t					*indices16[LODCount];
            uint32_t					*indices32[LODCount];
            NSUInteger	      			indexCount[LODCount];
			bool 						tessellated;
			eTessellationFactorMode 	tessellationFactorMode;
			NSUInteger					tessellationInstanceCount;
            
            Config()
            : name(NULL)
			, primitiveType(ePrimitiveType_Triangle)
            , vertexCount(0)
            , tessellated(false)
			, tessellationInstanceCount(0)
            {
				memset(indices16, 0, sizeof(indices16));
				memset(indices32, 0, sizeof(indices32));
				memset(indexCount, 0, sizeof(indexCount));
			}
			
			bool IsIndexed(NSUInteger lod) const
			{
				return (indices16[lod] != NULL) || (indices32[lod] != NULL);
			}
            
        } Config;
		
        Mesh(Config *_config)
        : config(_config)
        {
			qASSERTM(LODCount > 0, "LOD count of mesh %s can can not be zero", config->name);
			qASSERTM(config->vertexCount > 0, "Vertex count of mesh %s can can not be zero", config->name);
			for (NSUInteger lod = 0; lod < LODCount; ++lod)
			{
				qASSERTM(!config->IsIndexed(lod) || config->indexCount[lod] > 0, "Index count of mesh %s can can not be zero", config->name);
				qASSERTM(!config->IsIndexed(lod) || config->indices16[lod] != NULL || config->indices32[lod] != NULL, "One of uint16_t or uint32_t indices of mesh %s must be set", config->name);
				qASSERTM(!config->IsIndexed(lod) || config->indices16[lod] == NULL || config->indices32[lod] == NULL, "Only one of uint16_t or uint32_t indices of mesh %s must be set, not both", config->name);
			}
			
			qASSERTM(LODCount == 1 || !config->tessellated, "Support for tessellated meshes with LODs hasn't been implemented", config->name);
			
			for (int i = 0; i < TessellationStreamCount; ++i)
			{
				VertexStream &tessellationStream = config->tessellationStreams[i];
				
				qASSERTM(tessellationStream.type != eVertexStreamType_Unset, "Vertex stream type %i of mesh %s is unset", i, config->name)
				qASSERTM(tessellationStream.data != NULL, "Tessellation stream data %i of mesh %s is unset", i, config->name);
				
				//one per factor unless specified
				NSUInteger count = (tessellationStream.count == 0) ? (config->indexCount[0] / 3) : tessellationStream.count;
				
				tessellationBuffers[i] = [qMetal::Device::Get() newBufferWithBytes:tessellationStream.data length:(NSUInteger)tessellationStream.type * count options:0];
				[tessellationBuffers[i] setLabel:@"tessellation buffer"];
			}
			
			for (int i = 0; i < VertexStreamCount; ++i)
			{
				VertexStream &vertexStream = config->vertexStreams[i];
				
				qASSERTM(vertexStream.type != eVertexStreamType_Unset, "Vertex stream type %i of mesh %s is unset", i, config->name)
				qASSERTM(vertexStream.data != NULL, "Vertex stream data %i of mesh %s is unset", i, config->name);
				
				//one per vertex unless specified
				NSUInteger count = (vertexStream.count == 0) ? config->vertexCount : vertexStream.count;
				
				vertexBuffers[i] = [qMetal::Device::Get() newBufferWithBytes:vertexStream.data length:(NSUInteger)vertexStream.type * count options:MTLResourceStorageModeShared];
				[vertexBuffers[i] setLabel:[NSString stringWithFormat:@"vertex buffer %i", i]];
			}
			
			for (NSUInteger lod = 0; lod < LODCount; ++lod)
			{
				if (!config->IsIndexed(lod))
				{
					continue;
				}
				
				if (config->indices16[lod] != NULL)
				{
					indexBuffer[lod] = [qMetal::Device::Get() newBufferWithBytes:config->indices16[lod] length:sizeof(uint16_t) * config->indexCount[lod] options:0];
				}
				else if (config->indices32[lod] != NULL)
				{
					indexBuffer[lod] = [qMetal::Device::Get() newBufferWithBytes:config->indices32[lod] length:sizeof(uint32_t) * config->indexCount[lod] options:0];
				}
				
				[indexBuffer[lod] setLabel:[NSString stringWithFormat:@"index buffer lod %lu", (unsigned long)lod]];
			}
			
			if (config->tessellated)
			{
				//one for each triangle, and each tesselation factor is 8 bytes
				NSUInteger tesselationFactorCount = (config->tessellationInstanceCount > 0) ? config->tessellationInstanceCount : 1;
				if (config->tessellationFactorMode == eTessellationFactorMode_PerTriangle)
				{
					tesselationFactorCount *= config->indexCount[0] / 3;
				}
				
				tessellationFactorsBuffer = [qMetal::Device::Get() newBufferWithLength:(tesselationFactorCount * sizeof(MTLTriangleTessellationFactorsHalf))
                                               options:MTLResourceStorageModePrivate];
				tessellationFactorsBuffer.label = @"Tessellation Factors";
			}
		}
		
		template<class _VertexParams, int _VertexTextureIndex, int _VertexParamsIndex, class _FragmentParams, int _FragmentTextureIndex, int _FragmentParamsIndex, class _ComputeParams, int _ComputeParamsIndex, class _InstanceParams, int _InstanceParamsIndex, bool _ForIndirectCommandBuffer>
        void Encode(id<MTLComputeCommandEncoder> encoder, const Material<_VertexParams, _VertexTextureIndex, _VertexParamsIndex, _FragmentParams, _FragmentTextureIndex, _FragmentParamsIndex, _ComputeParams, _ComputeParamsIndex, _InstanceParams, _InstanceParamsIndex, _ForIndirectCommandBuffer> *material)
        {
			qASSERT(config->tessellated);
			
			for (int i = 0; i < TessellationStreamCount; ++i)
			{
				[encoder setBuffer:tessellationBuffers[i] offset:0 atIndex:i];
			}
			
			[encoder setBuffer:tessellationFactorsBuffer offset:0 atIndex:TessellationStreamFactorsIndex];
			
			NSUInteger width = material->IsInstanced() ? material->InstanceCount() : 1;
			NSUInteger height = 1;
			if (config->tessellationFactorMode == eTessellationFactorMode_PerTriangle)
			{
				height = config->indexCount[0] / 3;
			}
			
			material->EncodeCompute(encoder, width, height);
		}
		
		template<class _VertexParams, int _VertexTextureIndex, int _VertexParamsIndex, class _FragmentParams, int _FragmentTextureIndex, int _FragmentParamsIndex, class _ComputeParams, int _ComputeParamsIndex, class _InstanceParams, int _InstanceParamsIndex, bool _ForIndirectCommandBuffer>
        void Encode(id<MTLRenderCommandEncoder> encoder, const Material<_VertexParams, _VertexTextureIndex, _VertexParamsIndex, _FragmentParams, _FragmentTextureIndex, _FragmentParamsIndex, _ComputeParams, _ComputeParamsIndex, _InstanceParams, _InstanceParamsIndex, _ForIndirectCommandBuffer> *material, NSUInteger LOD = 0)
        {
        	material->Encode(encoder);
			
			if (VertexStreamIndex != -1)
			{
				for (int i = 0; i < VertexStreamCount; ++i)
				{
					[encoder useResource:vertexBuffers[i] usage:MTLResourceUsageRead];
				}
		
				id<MTLBuffer> argumentBuffer = GetVertexArgumentBufferForMaterial(material);
				[encoder setVertexBuffer:argumentBuffer offset:0 atIndex:VertexStreamIndex];
			}
			else
			{
				for (int i = 0; i < VertexStreamCount; ++i)
				{
					[encoder setVertexBuffer:vertexBuffers[i] offset:0 atIndex:i];
				}
			}
			
			if (config->tessellated)
			{
				NSUInteger stride = material->IsInstanced() ? 1 : 0;
				
				if (config->tessellationFactorMode == eTessellationFactorMode_PerTriangle)
				{
					stride *= config->indexCount[0] / 3;
				}
				
				stride *= sizeof(MTLTriangleTessellationFactorsHalf);
				
				[encoder setTessellationFactorBuffer:tessellationFactorsBuffer offset:0 instanceStride:stride];
				
				if (config->IsIndexed(LOD))
				{
					//handles instanced as well
					[encoder drawIndexedPatches:3 patchStart:0 patchCount:(config->indexCount[0] / 3) patchIndexBuffer:NULL patchIndexBufferOffset:0 controlPointIndexBuffer:indexBuffer[0] controlPointIndexBufferOffset:0 instanceCount:material->InstanceCount() baseInstance:0];
				}
				else
				{
					[encoder drawPatches:3 patchStart:0 patchCount:(config->indexCount[0] / 3) patchIndexBuffer:NULL patchIndexBufferOffset:0 instanceCount:material->InstanceCount() baseInstance:0];
				}
			}
			else if (material->IsInstanced())
			{
				if (config->IsIndexed(LOD))
				{
					[encoder drawIndexedPrimitives:(MTLPrimitiveType)config->primitiveType indexCount:config->indexCount[LOD] indexType:((config->indices16[LOD] != NULL) ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32) indexBuffer:indexBuffer[LOD] indexBufferOffset:0 instanceCount:material->InstanceCount() baseVertex:0 baseInstance:0];
				}
				else
				{
					[encoder drawPrimitives:(MTLPrimitiveType)config->primitiveType vertexStart:0 vertexCount:config->vertexCount instanceCount:material->InstanceCount()];
				}
			}
			else
			{
				if (config->IsIndexed(LOD))
				{
					[encoder drawIndexedPrimitives:(MTLPrimitiveType)config->primitiveType indexCount:config->indexCount[LOD] indexType:((config->indices16[LOD] != NULL) ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32) indexBuffer:indexBuffer[LOD] indexBufferOffset:0];
				}
				else
				{
					[encoder drawPrimitives:(MTLPrimitiveType)config->primitiveType vertexStart:0 vertexCount:config->vertexCount];
				}
			}
        }
		
		void Encode(id<MTLIndirectRenderCommand> indirectRenderCommand)
		{
			qASSERTM(!config->tessellated, "TODO support tessellated meshes in indirect command buffers");

			for (int i = 0; i < VertexStreamCount; ++i)
			{
				[indirectRenderCommand setVertexBuffer:vertexBuffers[i] offset:0 atIndex:i];
			}
			
			[indirectRenderCommand drawIndexedPrimitives:(MTLPrimitiveType)config->primitiveType indexCount:config->indexCount indexType:((config->indices16[0] != NULL) ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32) indexBuffer:indexBuffer indexBufferOffset:0 instanceCount:1 baseVertex:0 baseInstance:0];
		}
		
		void UseResources(id<MTLComputeCommandEncoder> encoder, NSUInteger LOD = 0)
		{
			for (int i = 0; i < VertexStreamCount; ++i)
			{
				[encoder useResource:vertexBuffers[i] usage:MTLResourceUsageRead];
			}
			
			[encoder useResource:indexBuffer[LOD] usage:MTLResourceUsageRead];
		}
		
		void UseResources(id<MTLRenderCommandEncoder> encoder, NSUInteger LOD = 0)
		{
			for (int i = 0; i < VertexStreamCount; ++i)
			{
				[encoder useResource:vertexBuffers[i] usage:MTLResourceUsageRead];
			}
			
			[encoder useResource:indexBuffer[LOD] usage:MTLResourceUsageRead];
		}
		
		void UseResourcesAllLODs(id<MTLRenderCommandEncoder> encoder)
		{
			for (int i = 0; i < VertexStreamCount; ++i)
			{
				[encoder useResource:vertexBuffers[i] usage:MTLResourceUsageRead];
			}
			
			for (NSUInteger LOD = 0; LOD < LODCount; ++LOD)
			{
				[encoder useResource:indexBuffer[LOD] usage:MTLResourceUsageRead];
			}
		}
		
		template<class _VertexParams, int _VertexTextureIndex, int _VertexParamsIndex, class _FragmentParams, int _FragmentTextureIndex, int _FragmentParamsIndex, class _ComputeParams, int _ComputeParamsIndex, class _InstanceParams, int _InstanceParamsIndex, bool _ForIndirectCommandBuffer>
		id<MTLBuffer> GetVertexArgumentBufferForMaterial(const Material<_VertexParams, _VertexTextureIndex, _VertexParamsIndex, _FragmentParams, _FragmentTextureIndex, _FragmentParamsIndex, _ComputeParams, _ComputeParamsIndex, _InstanceParams, _InstanceParamsIndex, _ForIndirectCommandBuffer> *material)
		{
			if (argumentBufferMap.find(material->VertexFunction()) == argumentBufferMap.end())
			{
				return CreateVertexArgumentBufferForMaterial(material);
			}
			return argumentBufferMap.find(material->VertexFunction())->second;
		}
		
		id<MTLBuffer> GetVertexBuffer(uint index)
		{
			qASSERTM(index < VertexStreamCount, "index is out of range for the mesh");
			return vertexBuffers[index];
		}
		
		id<MTLBuffer> GetIndexBuffer(NSUInteger LOD = 0)
		{
			return indexBuffer[LOD];
		}
		
		Config* GetConfig() const
		{
			return config;
		}
		
    private:
		
		template<class _VertexParams, int _VertexTextureIndex, int _VertexParamsIndex, class _FragmentParams, int _FragmentTextureIndex, int _FragmentParamsIndex, class _ComputeParams, int _ComputeParamsIndex, class _InstanceParams, int _InstanceParamsIndex, bool _ForIndirectCommandBuffer>
		id<MTLBuffer> CreateVertexArgumentBufferForMaterial(const Material<_VertexParams, _VertexTextureIndex, _VertexParamsIndex, _FragmentParams, _FragmentTextureIndex, _FragmentParamsIndex, _ComputeParams, _ComputeParamsIndex, _InstanceParams, _InstanceParamsIndex, _ForIndirectCommandBuffer> *material)
		{
			id <MTLArgumentEncoder> argumentEncoder = [material->VertexFunction()->Get() newArgumentEncoderWithBufferIndex:VertexStreamIndex];
			id<MTLBuffer> argumentBuffer = [qMetal::Device::Get() newBufferWithLength:argumentEncoder.encodedLength options:0];
			argumentBuffer.label = @"Mesh Vertex Stream Argument Buffer";
			[argumentEncoder setArgumentBuffer:argumentBuffer offset:0];
			
			for (int i = 0; i < VertexStreamCount; ++i)
			{
				[argumentEncoder setBuffer:vertexBuffers[i] offset:0 atIndex:i];
			}
			
			argumentBufferMap_t::value_type KV(material->VertexFunction(), argumentBuffer);
			
			#if DEBUG
			std::pair<argumentBufferMap_t::iterator, bool> result =
			#endif
			argumentBufferMap.insert(KV);
			qASSERTM(result.second == true, "Failed to insert argument buffer");
			
			return argumentBuffer;
		}
		
		typedef std::map<const Function*, id<MTLBuffer> > argumentBufferMap_t;
		argumentBufferMap_t argumentBufferMap;
		
		id<MTLBuffer>		tessellationBuffers[TessellationStreamCount];
		id<MTLBuffer>		vertexBuffers[VertexStreamCount];
		id<MTLBuffer>		indexBuffer[LODCount];
		
		id<MTLBuffer> 		tessellationFactorsBuffer;
		
        Config              *config;
    };
}

#endif //__Q_METAL_MESH_H__

