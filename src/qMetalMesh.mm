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

#include "qMetalMesh.h"

namespace qMetal
{
    Mesh::Mesh(Mesh::Config* _config)
	: config(_config)
	{
		qASSERTM(config->quadIndexCount == 0 || config->tessellated, "Can't have quad indices on mesh %s unless we're tessellated", config->name.UTF8String);
		qASSERTM(config->vertexCount > 0, "Vertex count of mesh %s can can not be zero", config->name.UTF8String);
		qASSERTM(!config->IsIndexed() || config->indexCount > 0, "Index count of mesh %s can can not be zero", config->name.UTF8String);
		qASSERTM(!config->IsQuadIndexed() || config->quadIndexCount > 0, "Quad index count of mesh %s can can not be zero", config->name.UTF8String);
		qASSERTM(config->vertexStreamCount < VertexStreamLimit, "Too many vertex streams");
		qASSERTM(config->tessellationStreamCount < TessellationStreamLimit, "Too many tessellation streams");
		
		for (int i = 0; i < config->tessellationStreamCount; ++i)
		{
			VertexStream &tessellationStream = config->tessellationStreams[i];
			
			qASSERTM(tessellationStream.type != eVertexStreamType_Unset, "Vertex stream type %i of mesh %s is unset", i, config->name.UTF8String)
			qASSERTM(tessellationStream.data != NULL, "Tessellation stream data %i of mesh %s is unset", i, config->name.UTF8String);
			
			//one per factor unless specified
			NSUInteger count = (tessellationStream.count == 0) ? (config->indexCount / 3) : tessellationStream.count;
			
			tessellationBuffers[i] = [qMetal::Device::Get() newBufferWithBytes:tessellationStream.data length:(NSUInteger)tessellationStream.type * count options:0];
			tessellationBuffers[i].label = [NSString stringWithFormat:@"%@ tesselation buffer %i", config->name, i];
		}
		
		for (int i = 0; i < config->vertexStreamCount; ++i)
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
	
	void Mesh::Encode(id<MTLIndirectRenderCommand> indirectRenderCommand)
	{
		qASSERTM(!config->tessellated, "TODO support tessellated meshes in indirect command buffers");

		for (int i = 0; i < config->vertexStreamCount; ++i)
		{
			[indirectRenderCommand setVertexBuffer:vertexBuffers[i] offset:0 atIndex:i];
		}
		
		[indirectRenderCommand drawIndexedPrimitives:(MTLPrimitiveType)config->primitiveType indexCount:config->indexCount indexType:((config->indices16 != NULL) ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32) indexBuffer:indexBuffer indexBufferOffset:0 instanceCount:1 baseVertex:0 baseInstance:0];
	}
	
	void Mesh::UseResources(id<MTLComputeCommandEncoder> encoder, bool withVertexArgumentBuffer)
	{
		if (withVertexArgumentBuffer)
		{
			for (int i = 0; i < config->vertexStreamCount; ++i)
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
	
	void Mesh::UseResources(id<MTLRenderCommandEncoder> encoder)
	{
		for (int i = 0; i < config->vertexStreamCount; ++i)
		{
			[encoder useResource:vertexBuffers[i] usage:MTLResourceUsageRead];
		}
		
		if (config->IsIndexed())
		{
			[encoder useResource:indexBuffer usage:MTLResourceUsageRead];
		}
	}
}

