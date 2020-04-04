# GLSL Trace

## Features
 * shader debugging
 * shader profiling (using extensions [EXT_shader_realtime_clock](https://github.com/KhronosGroup/GLSL/blob/master/extensions/ext/EXT_shader_realtime_clock.txt) and [ARB_shader_clock](https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_shader_clock.txt))
 * supports mesh and ray tracing shaders (Vulkan only)

## How its works
**Setup:**</br> 
 * (__OpenGL__) Create shader and shader program for reqular rendering.
 * Use [glslang](https://github.com/KhronosGroup/glslang) to parse GLSL or HLSL source code
 * (__Vulkan__) Convert glslang AST to SPIRV and create pipeline for reqular rendering.
 * Create `ShaderTrace` object to store debug information
 * Get glslang AST `TProgram::getIntermediate(EShLanguage stage)`
 * Insert trace recording `ShaderTrace::InsertTraceRecording(TIntermediate &intermediate, uint32_t setIndex)` or `ShaderTrace::InsertFunctionProfiler(TIntermediate &intermediate, uint32_t setIndex, bool shaderSubgroupClock, bool shaderDeviceClock)`, where:</br>
   shaderSubgroupClock - requires OpenGL extension [GL_ARB_shader_clock](https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_shader_clock.txt), requires Vulkan extension `VK_KHR_shader_clock` and `VkPhysicalDeviceShaderClockFeaturesKHR::shaderSubgroupClock` must be true.</br>
   shaderDeviceClock - requires OpenGL extension [GL_EXT_shader_realtime_clock](https://github.com/KhronosGroup/GLSL/blob/master/extensions/ext/GL_EXT_shader_realtime_clock.txt), requires Vulkan extension `VK_KHR_shader_clock` and `VkPhysicalDeviceShaderClockFeaturesKHR::shaderDeviceClock` must be true.</br>
   `descSetIndex` - descriptor set index that will be reserved for storage buffer (Vulkan only)</br>
 * (__Vulkan__) Convert AST with trace recording to SPIRV and create pipeline for debugging.
 * (__OpenGL__) Convert AST to SPIRV, use [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross) to get GLSL code and create shader program for debugging.


**To debug draw or compute or ray tracing invocation:**</br> 
 
 * (__Vulkan__) bind pipeline that contains shader with inserted trace recording
 * (__Vulkan__) bind descriptor set with storage buffer to index `descSetIndex`
 * (__OpenGL__) bind shader program that contains shader with inserted trace recording
 * (__OpenGL__) get shader storage block index of `dbg_ShaderTraceStorage`and bind storage buffer to that index.
 * after invocation map storage buffer and pass pointer to `ShaderTrace::ParseShaderTrace (const void *ptr, uint64_t maxSize, vector<string> &result)`
 
## Debugging

<details>
<summary>Enable pixel/invocation debugging from code (OpenGL)</summary>
   
```cpp
// clear buffer
uint32_t  zero = 0;
glClearBufferData( GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero );

// set pixel which you need to debug (2 components)
// record if {pixel_x, pixel_y} == floor(gl_FragCoord.xy)
uint32_t  data[] = { pixel_x, pixel_y };
glBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, sizeof(data), data );

// draw
...

// ... or which compute invocation or ray tracing launch (3 components)
// record if {thread_x, thread_y, thread_z} == gl_GlobalInvocationID
// record if {thread_x, thread_y, thread_z} == gl_LaunchID
uint32_t  data[] = { thread_x, thread_y, thread_z };
glBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, sizeof(data), data );

// dispatch or trace
...
```
</details>
<details>
<summary>Enable pixel/invocation debugging from code (Vulkan)</summary>
   
```cpp
// set pixel which you need to debug (2 components)
// record if {pixel_x, pixel_y} == floor(gl_FragCoord.xy)
uint32_t  data[] = { pixel_x, pixel_y };
vkCmdUpdateBuffer( cmdBuffer, debugOutputBuffer, 0, sizeof(data), data );
vkCmdFillBuffer( cmdBuffer, debugOutputBuffer, sizeof(data), VK_WHOLE_SIZE, 0 );

// draw
...

// ... or which compute invocation or ray tracing launch (3 components)
// record if {thread_x, thread_y, thread_z} == gl_GlobalInvocationID
// record if {thread_x, thread_y, thread_z} == gl_LaunchID
uint32_t  data[] = { thread_x, thread_y, thread_z };
vkCmdUpdateBuffer( cmdBuffer, debugOutputBuffer, 0, sizeof(data), data );
vkCmdFillBuffer( cmdBuffer, debugOutputBuffer, sizeof(data), VK_WHOLE_SIZE, 0 );

// dispatch or trace
...
```
</details>

<details>
<summary>Enable debugging from shader source</summary>

```cpp
// empty function will be replaced during shader compilation
void dbg_EnableTraceRecording (bool b) {}

void main ()
{
    bool condition = ...
        
    // if condition is true then trace recording will start here
    dbg_EnableTraceRecording( condition );
    ...
}
```
</details>

<details>
<summary>Example of shader trace</summary>

```cpp
//> gl_GlobalInvocationID: uint3 {8, 8, 0}
//> gl_LocalInvocationID: uint3 {0, 0, 0}
//> gl_WorkGroupID: uint3 {1, 1, 0}
no source

//> index: uint {136}
//  gl_GlobalInvocationID: uint3 {8, 8, 0}
11. index = gl_GlobalInvocationID.x + gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x;

//> size: uint {256}
12. size = gl_NumWorkGroups.x * gl_NumWorkGroups.y * gl_WorkGroupSize.x * gl_WorkGroupSize.y;

//> value: float {0.506611}
//  index: uint {136}
//  size: uint {256}
13. value = sin( float(index) / size );

//> imageStore(): void
//  gl_GlobalInvocationID: uint3 {8, 8, 0}
//  value: float {0.506611}
14.     imageStore( un_OutImage, ivec2(gl_GlobalInvocationID.xy), vec4(value) );
```
</details>


## Profiling

<details>
<summary>Enable pixel/invocation profiling from code (OpenGL)</summary>
   
```cpp
// clear buffer
uint32_t  zero = 0;
glClearBufferData( GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero );

// set pixel which you need to profile (2 components)
// record if {pixel_x, pixel_y} == floor(gl_FragCoord.xy)
uint32_t  data[] = { pixel_x, pixel_y };
glBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, sizeof(data), data );

// draw
...

// ... or which compute invocation or ray tracing launch (3 components)
// record if {thread_x, thread_y, thread_z} == gl_GlobalInvocationID
// record if {thread_x, thread_y, thread_z} == gl_LaunchID
uint32_t  data[] = { thread_x, thread_y, thread_z };
glBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, sizeof(data), data );

// dispatch or trace
...
```
</details>
<details>
<summary>Enable pixel/invocation profiling from code (Vulkan)</summary>
   
```cpp
// set pixel which you need to debug (2 components)
// record if {pixel_x, pixel_y} == floor(gl_FragCoord.xy)
uint32_t  data[] = { pixel_x, pixel_y };
vkCmdUpdateBuffer( cmdBuffer, debugOutputBuffer, 0, sizeof(data), data );
vkCmdFillBuffer( cmdBuffer, debugOutputBuffer, sizeof(data), VK_WHOLE_SIZE, 0 );

// draw
...

// ... or which compute invocation or ray tracing launch (3 components)
// record if {thread_x, thread_y, thread_z} == gl_GlobalInvocationID
// record if {thread_x, thread_y, thread_z} == gl_LaunchID
uint32_t  data[] = { thread_x, thread_y, thread_z };
vkCmdUpdateBuffer( cmdBuffer, debugOutputBuffer, 0, sizeof(data), data );
vkCmdFillBuffer( cmdBuffer, debugOutputBuffer, sizeof(data), VK_WHOLE_SIZE, 0 );

// dispatch or trace
...
```
</details>

<details>
<summary>Enable profiling from shader source</summary>
   
```cpp
// empty function will be replaced during shader compilation
void dbg_EnableProfiling (bool b) {}
    
void main ()
{
    bool condition = ...
        
    // if condition is true then profiling will start here
    dbg_EnableProfiling( condition );
    ...
}
```
</details>

<details>
<summary>Example of shader profiling output</summary>

```cpp
//> gl_GlobalInvocationID: uint3 {512, 512, 0}
//> gl_LocalInvocationID: uint3 {0, 0, 0}
//> gl_WorkGroupID: uint3 {64, 64, 0}
no source

// subgroup total: 100.00%,  avr: 100.00%,  (95108.00)
// device   total: 100.00%,  avr: 100.00%,  (2452.00)
// invocations:    1
106. void main ()

// subgroup total: 89.57%,  avr: 89.57%,  (85192.00)
// device   total: 89.56%,  avr: 89.56%,  (2196.00)
// invocations:    1
29. float FBM (in float3 coord)

// subgroup total: 84.67%,  avr: 12.10%,  (11504.57)
// device   total: 84.18%,  avr: 12.03%,  (294.86)
// invocations:    7
56. float GradientNoise (const float3 pos)

// subgroup total: 45.15%,  avr: 0.81%,  (766.86)
// device   total: 44.54%,  avr: 0.80%,  (19.50)
// invocations:    56
72. float3 DHash33 (const float3 p)
```
</details>

