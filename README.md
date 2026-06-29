# Hail
 Custom game engine - framework for c++ game development

# TODO: priority order for project

- [x] Text render commands.
- [x] Depth Sorting of sprite and text commands.
- [x] Implement batch rendering of sprites and fonts.
- [x] Cloud rendering experiment of a point cloud dataset.
- [x] First draft Update cloud point data as a fluid for dynamic clouds.
- [x] Rewrite the fluid solver not after Sebastians video (Coding Adventure: Simulating Fluids) but this paper: Smoothed Particle Hydrodynamics. Techniques for the Physics Based Simulation of Fluids and Solids.
- [x] Compute shader support.
- [x] Texture views to have read only, write only and read write access to textures (might need to add memory barriers for the reading).
- [x] Sorting with a compute shader.
- [x] liquid simulation on the GPU.
- [x] Create a sdf texture each frame for all fluids and use it to drive the visual effect. 
- [x] Multiple clouds.
- [x] Sky background.
- [x] Crash logger and send out a build for testing. 

---------------
# TODO list:

- [x] Look over the Frame In Flight fences as the tutroial I followed was wrong. 
- [x] Look over all resources that are using frame in flight and remove uneccessary uses. 
- [x] Fix include hierarchy so I do not need "../../Engine_ResourceHandling/ResourceCommon.h" in rendering code. 
- [x] Angelscript, implement the language server protocol for SyntaxHighlighting in VS-Code.
- [x] Angelscript, send error messages on Angelscript compilation fail and Engine registered resources.
- [x] AngelScript creates a Capability base class.
- [] Hook up Tick-Graph with created capabilities.
- [] Create a ECS or GO structure to manage Capabilities and GamePlay systems.
- [] Hook up ImGui with functionPtrs from Gamethread instead of HandMade registry.
- [] Angelscript, replace std::string with our own string class, register string to the type registry.
- [] Angelscript, improve hot reloading and make hotreloading when changing dependency files.
- [] Create a pipeline to create sprites render commands from AngelScript.
- [] Fix broken reloading of GPU resources and then remake hot reloading.
- [] Shader include gets updated, update all shaders that depends on it, so shader dependency tracking. 
- [] Improve RadixSort to use a proper reduction for the shuffle step. 

### Unsorted tasks

- [] Memory allocators & memory pool (check out the Arena memory allocator strategy).
- [] Red-black tree and hashmap.
- [x] Improve growing array.
- [x] Long string class.
- [x] Text render commands.
- [x] Font rendering.
- [x] Make asserts and error Handling.
- [x] Update the input handler and create an input to action map.
- [] Threaded loading, make it safe to load from game thread.
- [] Animation system for 2D animations.
- [] Profiling and a profiler window.
- [] Imgui Dockable.
- [] Serialize the resource registry in to a manifest so that resources does not get hotreloaded when in a new environment.
- [x] Use material system to take control over render styles and add blending.
- [x] Shader owned by material Instances
- [] Configurable rendering, so start of a render graph.
- [x] Compute shader support
- [] Implement Mip-Mapping.
- [] Implement a File watcher.
- [] Make resource registry thread safe, could be done by locking it on write operations and waiting on read.
- [x] 2D camera for the 2D rendering.
- [] Resource registration Init Check if current path does not match the meta resource project path, and make a function to clean that up.
- [x] Implement batch rendering of sprites and fonts.
- [] Spline objects.
- [x] Implement VMA on the Vulkan backend.
- [x] VMA implement texture support.
- [x] Angelscript, implement array.
- [x] Angelscript, add debugging support in VS code.
- [x] Angelscript, implement the input handler and debug commands to the scripts.
- [x] Angelscript, implement the language server protocol for SyntaxHighlighting in VS-Code.
- [x] Angelscript, send error messages on Angelscript compilation fail and Engine registered resources.
- [x] Context upload once function.
- [x] Context, move over rendering and state functions to the context.
- [x] Depth Sorting of sprite and text commands.
- [x] Explicit setting of Framebuffer bind state through the context object. Make materials transition have a bind state that is not set at Init.
- [] Shader include gets updated, update all shaders that depends on it. 
- [] Bindless resources.
- [] RawInput instead of Windows input.
- [] When program is out of focus, do not record any input and yield threads to the OS.

### Quality of life tasks

- [] Replace in FileObject the WString256 with a StringLW.
- [] Input queue to make input more predictable.
- [] Configure more startup attributes through command args, like Asserting, ImGui and ErrorHandling.
- [] Improved system diagnostics and error handling for builds.

## Important Notes
Create a seperate Command Pool for short lived commands, as well as a transfer Queue only for transfer commands on the GPU.
Iterators for for-each loops on all container types

## Known bugs:
Compound glyphs can get the wrong horizontal alignment, repo case, Roboto-Medium ':'
