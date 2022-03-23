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
    template<int VertexStreamCount, int VertexStreamIndex = EmptyIndex, int TessellationStreamCount = 0, int TessellationFactorsIndex = EmptyIndex>
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
            NSString*					name;
			ePrimitiveType				primitiveType;
            VertexStream				tessellationStreams[TessellationStreamCount];
            VertexStream				vertexStreams[VertexStreamCount];
            NSUInteger	      			vertexCount;
            uint16_t					*indices16;
            uint32_t					*indices32;
            NSUInteger	      			indexCount;
            uint16_t					*quadIndices16;
            uint32_t					*quadIndices32;
            NSUInteger	      			quadIndexCount;
			bool 						tessellated;
			eTessellationFactorMode 	tessellationFactorMode;
			uint32_t				 	tessellationFactorMultiplier;
			NSUInteger					tessellationInstanceCount;
            
            Config(NSString* _name)
            : name(_name)
            , primitiveType(ePrimitiveType_Triangle)
            , vertexCount(0)
            , indices16(NULL)
            , indices32(NULL)
            , indexCount(0)
            , quadIndices16(NULL)
            , quadIndices32(NULL)
            , quadIndexCount(0)
            , tessellated(false)
            , tessellationFactorMultiplier(1)
			, tessellationInstanceCount(0)
            {
				
			}
			
			bool IsIndexed() const
			{
				return (indices16 != NULL) || (indices32 != NULL);
			}
			
			bool IsQuadIndexed() const
			{
				return (quadIndices16 != NULL) || (quadIndices32 != NULL);
			}
            
        } Config;
		
        Mesh(Config *_config)
        : config(_config)
        {
			qASSERTM(config->quadIndexCount == 0 || config->tessellated, "Can't have quad indices on mesh %s unless we're tessellated", config->name.UTF8String);
			qASSERTM(config->vertexCount > 0, "Vertex count of mesh %s can can not be zero", config->name.UTF8String);
			qASSERTM(!config->IsIndexed() || config->indexCount > 0, "Index count of mesh %s can can not be zero", config->name.UTF8String);
			qASSERTM(!config->IsQuadIndexed() || config->quadIndexCount > 0, "Quad index count of mesh %s can can not be zero", config->name.UTF8String);
			
			for (int i = 0; i < TessellationStreamCount; ++i)
			{
				VertexStream &tessellationStream = config->tessellationStreams[i];
				
				qASSERTM(tessellationStream.type != eVertexStreamType_Unset, "Vertex stream type %i of mesh %s is unset", i, config->name.UTF8String)
				qASSERTM(tessellationStream.data != NULL, "Tessellation stream data %i of mesh %s is unset", i, config->name.UTF8String);
				
				//one per factor unless specified
				NSUInteger count = (tessellationStream.count == 0) ? (config->indexCount / 3) : tessellationStream.count;
				
				tessellationBuffers[i] = [qMetal::Device::Get() newBufferWithBytes:tessellationStream.data length:(NSUInteger)tessellationStream.type * count options:0];
				tessellationBuffers[i].label = [NSString stringWithFormat:@"%@ tesselation buffer %i", config->name, i];
			}
			
			for (int i = 0; i < VertexStreamCount; ++i)
			{
				VertexStream &vertexStream = config->vertexStreams[i];
				
				qASSERTM(vertexStream.type != eVertexStreamType_Unset, "Vertex stream type %i of mesh %s is unset", i, config->name.UTF8String)
				qASSERTM(vertexStream.data != NULL, "Vertex stream data %i of mesh %s is unset", i, config->name.UTF8String);
				
				//one per vertex unless specified
				NSUInteger count = (vertexStream.count == 0) ? config->vertexCount : vertexStream.count;
				
				vertexBuffers[i] = [qMetal::Device::Get() newBufferWithBytes:vertexStream.data length:(NSUInteger)vertexStream.type * count options:MTLResourceStorageModeShared]; //TODO MTLResourceStorageModePrivate via a blit?
				vertexBuffers[i].label = [NSString stringWithFormat:@"%@ vertices %i", config->name, i];
			}
			
			if (config->indices16 != NULL)
			{
				indexBuffer = [qMetal::Device::Get() newBufferWithBytes:config->indices16 length:sizeof(uint16_t) * config->indexCount options:0];
				indexBuffer.label = [NSString stringWithFormat:@"%@ 16-bit indices", config->name];
			}
			else if (config->indices32 != NULL)
			{
				indexBuffer = [qMetal::Device::Get() newBufferWithBytes:config->indices32 length:sizeof(uint32_t) * config->indexCount options:0];
				indexBuffer.label = [NSString stringWithFormat:@"%@ 32-bit indices", config->name];
			}
			
			if (config->quadIndices16 != NULL)
			{
				quadIndexBuffer = [qMetal::Device::Get() newBufferWithBytes:config->quadIndices16 length:sizeof(uint16_t) * config->quadIndexCount options:0];
				quadIndexBuffer.label = [NSString stringWithFormat:@"%@ 16-bit quad indices", config->name];
			}
			else if (config->quadIndices32 != NULL)
			{
				quadIndexBuffer = [qMetal::Device::Get() newBufferWithBytes:config->quadIndices32 length:sizeof(uint32_t) * config->quadIndexCount options:0];
				quadIndexBuffer.label = [NSString stringWithFormat:@"%@ 32-bit quad indices", config->name];
			}
			
			if (config->tessellated)
			{
				NSUInteger patchCount = config->vertexCount / 3;
				if (config->IsQuadIndexed())
				{
					patchCount = config->quadIndexCount / 4;
				}
				else if (config->IsIndexed())
				{
					patchCount = config->indexCount / 3;
				}
				
				//one for each triangle, and each tesselation factor is 8 bytes
				tessellationFactorsCount = (config->tessellationInstanceCount > 0) ? config->tessellationInstanceCount : 1;
				if (config->tessellationFactorMode == eTessellationFactorMode_PerPatch)
				{
					tessellationFactorsCount *= patchCount;
				}
				else if (config->tessellationFactorMode == eTessellationFactorMode_PerPatchMultiplied)
				{
					tessellationFactorsCount = patchCount * config->tessellationFactorMultiplier;
				}
				
				tessellationFactorsBuffer = [qMetal::Device::Get() newBufferWithLength:(tessellationFactorsCount * (config->IsQuadIndexed() ? sizeof(MTLQuadTessellationFactorsHalf) : sizeof(MTLTriangleTessellationFactorsHalf))) options:MTLResourceStorageModePrivate];
				tessellationFactorsBuffer.label = [NSString stringWithFormat:@"%@ tessellation factors", config->name];
				
				uint tesellationPatchIndices[patchCount];
				
				for(uint i = 0; i < patchCount; ++i)
				{
					tesellationPatchIndices[i] = i;
				}
			}
		}
		
		template<class _VertexParams, int _VertexTextureIndex, int _VertexParamsIndex, class _FragmentParams, int _FragmentTextureIndex, int _FragmentParamsIndex, class _ComputeParams, int _ComputeParamsIndex, class _InstanceParams, int _InstanceParamsIndex>
        void Encode(id<MTLComputeCommandEncoder> encoder, const Material<_VertexParams, _VertexTextureIndex, _VertexParamsIndex, _FragmentParams, _FragmentTextureIndex, _FragmentParamsIndex, _ComputeParams, _ComputeParamsIndex, _InstanceParams, _InstanceParamsIndex> *material)
        {
			qASSERT(config->tessellated);
			
			for (int i = 0; i < TessellationStreamCount; ++i)
			{
				[encoder setBuffer:tessellationBuffers[i] offset:0 atIndex:i];
			}
			
			[encoder setBuffer:tessellationFactorsBuffer offset:0 atIndex:TessellationFactorsIndex];
			
			NSUInteger width = material->IsInstanced() ? material->InstanceCount() : 1;
			NSUInteger height = 1;
			if (config->tessellationFactorMode == eTessellationFactorMode_PerPatch)
			{
				height = config->indexCount / 3;
			}
			
			material->EncodeCompute(encoder, width, height);
		}
		
		template<class _VertexParams, int _VertexTextureIndex, int _VertexParamsIndex, class _FragmentParams, int _FragmentTextureIndex, int _FragmentParamsIndex, class _ComputeParams, int _ComputeParamsIndex, class _InstanceParams, int _InstanceParamsIndex>
        void Encode(id<MTLRenderCommandEncoder> encoder, const Material<_VertexParams, _VertexTextureIndex, _VertexParamsIndex, _FragmentParams, _FragmentTextureIndex, _FragmentParamsIndex, _ComputeParams, _ComputeParamsIndex, _InstanceParams, _InstanceParamsIndex> *material)
        {
        	material->Encode(encoder);
			
			if (VertexStreamIndex == EmptyIndex)
			{
				for (int i = 0; i < VertexStreamCount; ++i)
				{
					[encoder setVertexBuffer:vertexBuffers[i] offset:0 atIndex:i];
				}
			}
			else
			{
				for (int i = 0; i < VertexStreamCount; ++i)
				{
					[encoder useResource:vertexBuffers[i] usage:MTLResourceUsageRead];
				}
		
				id<MTLBuffer> argumentBuffer = GetVertexArgumentBufferForMaterial(material);
				[encoder setVertexBuffer:argumentBuffer offset:0 atIndex:VertexStreamIndex];
			}
			
			if (config->tessellated)
			{
				//handles instanced as well
				NSUInteger stride = material->IsInstanced() ? 1 : 0;
				
				if (config->tessellationFactorMode == eTessellationFactorMode_PerPatch)
				{
					stride *= config->indexCount / 3;
				}
				
				stride *= sizeof(MTLTriangleTessellationFactorsHalf);
				
				[encoder setTessellationFactorBuffer:tessellationFactorsBuffer offset:0 instanceStride:stride];
				
				if (config->IsQuadIndexed())
				{
					[encoder drawIndexedPatches:4 patchStart:0 patchCount:(config->quadIndexCount / 4) patchIndexBuffer:NULL patchIndexBufferOffset:0 controlPointIndexBuffer:quadIndexBuffer controlPointIndexBufferOffset:0 instanceCount:material->InstanceCount() baseInstance:0];
				}
				else if (config->IsIndexed())
				{
					[encoder drawIndexedPatches:3 patchStart:0 patchCount:(config->indexCount / 3) patchIndexBuffer:NULL patchIndexBufferOffset:0 controlPointIndexBuffer:indexBuffer controlPointIndexBufferOffset:0 instanceCount:material->InstanceCount() baseInstance:0];
				}
				else
				{
					[encoder drawPatches:3 patchStart:0 patchCount:(config->vertexCount / 3) patchIndexBuffer:NULL patchIndexBufferOffset:0 instanceCount:material->InstanceCount() baseInstance:0];
				}
			}
			else if (material->IsInstanced())
			{
				if (config->IsIndexed())
				{
					[encoder drawIndexedPrimitives:(MTLPrimitiveType)config->primitiveType indexCount:config->indexCount indexType:((config->indices16 != NULL) ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32) indexBuffer:indexBuffer indexBufferOffset:0 instanceCount:material->InstanceCount() baseVertex:0 baseInstance:0];
				}
				else
				{
					[encoder drawPrimitives:(MTLPrimitiveType)config->primitiveType vertexStart:0 vertexCount:config->vertexCount instanceCount:material->InstanceCount()];
				}
			}
			else
			{
				if (config->IsIndexed())
				{
					[encoder drawIndexedPrimitives:(MTLPrimitiveType)config->primitiveType indexCount:config->indexCount indexType:((config->indices16 != NULL) ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32) indexBuffer:indexBuffer indexBufferOffset:0];
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
		
		void UseResources(id<MTLComputeCommandEncoder> encoder, bool withVertexArgumentBuffer)
		{
			if (withVertexArgumentBuffer)
			{
				for (int i = 0; i < VertexStreamCount; ++i)
				{
					[encoder useResource:vertexBuffers[i] usage:MTLResourceUsageRead];
				}
			}
			
			if (config->IsQuadIndexed())
			{
				[encoder useResource:quadIndexBuffer usage:MTLResourceUsageRead];
			}
			else if (config->IsIndexed())
			{
				[encoder useResource:indexBuffer usage:MTLResourceUsageRead];
			}
			
			if (config->tessellated)
			{
				[encoder useResource:tessellationFactorsBuffer usage:MTLResourceUsageWrite];
			}
		}
		
		void UseResources(id<MTLRenderCommandEncoder> encoder)
		{
			for (int i = 0; i < VertexStreamCount; ++i)
			{
				[encoder useResource:vertexBuffers[i] usage:MTLResourceUsageRead];
			}
			
			if (config->IsIndexed())
			{
				[encoder useResource:indexBuffer usage:MTLResourceUsageRead];
			}
		}
		
		template<class _VertexParams, int _VertexTextureIndex, int _VertexParamsIndex, class _FragmentParams, int _FragmentTextureIndex, int _FragmentParamsIndex, class _ComputeParams, int _ComputeParamsIndex, class _InstanceParams, int _InstanceParamsIndex>
		id<MTLBuffer> GetVertexArgumentBufferForMaterial(const Material<_VertexParams, _VertexTextureIndex, _VertexParamsIndex, _FragmentParams, _FragmentTextureIndex, _FragmentParamsIndex, _ComputeParams, _ComputeParamsIndex, _InstanceParams, _InstanceParamsIndex> *material)
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
		
		id<MTLBuffer> GetQuadIndexBuffer()
		{
			return quadIndexBuffer;
		}
		
		id<MTLBuffer> GetIndexBuffer()
		{
			return indexBuffer;
		}
		
		NSUInteger GetTessellationFactorsCount()
		{
			return tessellationFactorsCount;
		}
		
		id<MTLBuffer> GetTessellationFactorsBuffer()
		{
			return tessellationFactorsBuffer;
		}
		
		Config* GetConfig() const
		{
			return config;
		}
		
    private:
		
		template<class _VertexParams, int _VertexTextureIndex, int _VertexParamsIndex, class _FragmentParams, int _FragmentTextureIndex, int _FragmentParamsIndex, class _ComputeParams, int _ComputeParamsIndex, class _InstanceParams, int _InstanceParamsIndex>
		id<MTLBuffer> CreateVertexArgumentBufferForMaterial(const Material<_VertexParams, _VertexTextureIndex, _VertexParamsIndex, _FragmentParams, _FragmentTextureIndex, _FragmentParamsIndex, _ComputeParams, _ComputeParamsIndex, _InstanceParams, _InstanceParamsIndex> *material)
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
		id<MTLBuffer>		indexBuffer;
		
		NSUInteger			tessellationFactorsCount;
		id<MTLBuffer> 		tessellationFactorsBuffer;	//RPW TODO we need one per instance and need to double buffer... probably a ring buffer?
		id<MTLBuffer>		quadIndexBuffer;
		
        Config              *config;
    };
}

#endif //__Q_METAL_MESH_H__

