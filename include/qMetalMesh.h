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
    class Mesh
    {
    public:
		static constexpr NSUInteger TessellationStreamLimit = 31;
		static constexpr NSUInteger VertexStreamLimit = 31;
		
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
			uint32_t					tessellationStreamCount;
			int32_t						tessellationFactorsIndex;
			uint32_t					vertexStreamCount;
			int32_t						vertexStreamIndex;
            VertexStream				tessellationStreams[TessellationStreamLimit];
            VertexStream				vertexStreams[VertexStreamLimit];
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
            : name([_name retain])
            , primitiveType(ePrimitiveType_Triangle)
            , vertexStreamCount(0)
            , vertexStreamIndex(EmptyIndex)
            , tessellationStreamCount(0)
            , tessellationFactorsIndex(EmptyIndex)
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
		
        Mesh(Config* _config);
		
		template<class _VertexParams, class _FragmentParams, class _ComputeParams, class _InstanceParams>
        void Encode(id<MTLComputeCommandEncoder> encoder, const Material<_VertexParams, _FragmentParams, _ComputeParams, _InstanceParams> *material)
        {
			qASSERT(config->tessellated);
			
			for (int i = 0; i < config->tessellationStreamCount; ++i)
			{
				[encoder setBuffer:tessellationBuffers[i] offset:0 atIndex:i];
			}
			
			[encoder setBuffer:tessellationFactorsBuffer offset:0 atIndex:config->tessellationFactorsIndex];
			
			NSUInteger width = material->IsInstanced() ? material->InstanceCount() : 1;
			NSUInteger height = 1;
			if (config->tessellationFactorMode == eTessellationFactorMode_PerPatch)
			{
				height = config->indexCount / 3;
			}
			
			material->EncodeCompute(encoder, width, height);
		}
		
		template<class _VertexParams, class _FragmentParams, class _ComputeParams, class _InstanceParams>
        void Encode(id<MTLRenderCommandEncoder> encoder, const Material<_VertexParams, _FragmentParams, _ComputeParams, _InstanceParams> *material)
        {
        	material->Encode(encoder);
			
			if (config->vertexStreamIndex == EmptyIndex)
			{
				for (int i = 0; i < config->vertexStreamCount; ++i)
				{
					[encoder setVertexBuffer:vertexBuffers[i] offset:0 atIndex:i];
				}
			}
			else
			{
				for (int i = 0; i < config->vertexStreamCount; ++i)
				{
					[encoder useResource:vertexBuffers[i] usage:MTLResourceUsageRead];
				}
		
				id<MTLBuffer> argumentBuffer = GetVertexArgumentBufferForMaterial(material);
				[encoder setVertexBuffer:argumentBuffer offset:0 atIndex:config->vertexStreamIndex];
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
		
		void Encode(id<MTLIndirectRenderCommand> indirectRenderCommand);
		
		void UseResources(id<MTLComputeCommandEncoder> encoder, bool withVertexArgumentBuffer);
		
		void UseResources(id<MTLRenderCommandEncoder> encoder);
		
		template<class _VertexParams, class _FragmentParams, class _ComputeParams, class _InstanceParams>
		id<MTLBuffer> GetVertexArgumentBufferForMaterial(const Material<_VertexParams, _FragmentParams, _ComputeParams, _InstanceParams> *material)
		{
			if (argumentBufferMap.find(material->VertexFunction()) == argumentBufferMap.end())
			{
				return CreateVertexArgumentBufferForMaterial(material);
			}
			return argumentBufferMap.find(material->VertexFunction())->second;
		}
		
		id<MTLBuffer> GetVertexBuffer(uint index)
		{
			qASSERTM(index < config->vertexStreamCount, "index is out of range for the mesh");
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
		
		template<class _VertexParams, class _FragmentParams, class _ComputeParams, class _InstanceParams>
		id<MTLBuffer> CreateVertexArgumentBufferForMaterial(const Material<_VertexParams, _FragmentParams, _ComputeParams, _InstanceParams> *material)
		{
			id <MTLArgumentEncoder> argumentEncoder = [material->VertexFunction()->Get() newArgumentEncoderWithBufferIndex:config->vertexStreamIndex];
			id<MTLBuffer> argumentBuffer = [qMetal::Device::Get() newBufferWithLength:argumentEncoder.encodedLength options:0];
			argumentBuffer.label = @"Mesh Vertex Stream Argument Buffer";
			[argumentEncoder setArgumentBuffer:argumentBuffer offset:0];
			
			for (int i = 0; i < config->vertexStreamCount; ++i)
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
		
		id<MTLBuffer>		tessellationBuffers[TessellationStreamLimit];
		id<MTLBuffer>		vertexBuffers[VertexStreamLimit];
		id<MTLBuffer>		indexBuffer;
		
		NSUInteger			tessellationFactorsCount;
		id<MTLBuffer> 		tessellationFactorsBuffer;	//RPW TODO we need one per instance and need to double buffer... probably a ring buffer?
		id<MTLBuffer>		quadIndexBuffer;
		
        Config              *config;
    };
}

#endif //__Q_METAL_MESH_H__

