# qMetal
qMetal is light Objective-C++ wrapper around Metal to provide building blocks for rendering engines, abstracting away some Metal-specifics into primitives that most rendering engineers are more comfortable with, while being relatively un-opinionated on how they are used, to let Metal's full feature set shine through.

The goal is to increase productivity with Metal, while enforcing best practices for performance (e.g. argument buffers) and still letting Metal's feature set shine through.

Note that qMetal does *not* attempt to be an abstraction layer on top of Metal, such that it could be "swapped out" for Vulkan or DX12 on other platforms. As the name suggests, qMetal is intentionally Metal.

## Concepts qMetal Provides

### Device

The qMetal device manages the system device, command queue, current command buffer, and required semaphores for proper dispatch blocking, though a simple StartFrame() / Begin() / Present() interface. The device is also used to acquire render, compute, and blit command encoders, as well as push + pop debug groups. 

### State Management

Blend, Cull, Depth, Sampler, and Stencil states are all managed by qMetal, providing both pre-defined states (for easy state de-duplication) and the ability to create new states as required.

### Textures

Textures are a pre-existing primitive in Metal, and qMetal simply extends them by providing:
1. tight binding of texture <-> sampler state, to make managing sampler states much easer. Of course, at the shader level, any texture can sill be sampled with any sampler, including those defined inline.
1. texture creation through config classes
1. a simple LoadByName() method to load (and de-duplicate) textures through MTKTextureLoader
1. Fill() methods to CPU-load data into textures
1. Sample() methods to CPU-sample a texture with bilinear filtering
1. a dedicated ComputeTexture class, with even simpler creation and management, based on compute texture usage patterns.

### Meshes

Again, meshes are clearly a pre-existing primitive in Metal; qMetal's again extends them by providing:
1. LOD support through multiple index buffers
1. enforced argument buffer support for vertex streams
1. simplified mesh creation through config classes
1. simplified mesh dispatch through a coupling with materials, particularly for tessellated meshes
1. support for all mesh types: indexed, instanced, tessellated, and all combinations thereof

### Materials

Materials are one of the largest contributions of qMetal. Roughly analogous to a fused MTLComputePipelineState + MTLRenderPipelineState, materials are a templated type that greatly simplifies dispatch for both compute and render. 

Materials provide:
1. enfoced argument buffers for material parameter blocks (without being opinionated on how the data ends up in said blocks)
1. support for compute, vertex, fragment, and instance parameter blocks
1. seamless handling of the required parameter block triple-buffering
1. support for render-only, compute-only, or compute+render dispatches (e.g. tessellated meshes with GPU tessellation factor generation)
1. simplified dispatch

### Indirect Meshes

Indirect Meshes are an extension of qMetal's meshes, and similarly templated type that provides support for Metal's Indirect Command Buffers on the GPU.

Indirect Meshes provide:
1. a binding of multiple meshes to a single dispatch kernel
1. management of all resources and binding requried for GPU-based Indirect Command Buffers
1. a simplified dispatch interface as per meshes – using materials as expected – for  command buffer generation, optimization, and dispatch phases
