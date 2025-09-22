# RayTracer Modernization Roadmap

## Overview
Modernizing a 15-year-old Qt/libqglviewer raytracer to use modern C++ and OpenGL technologies while maintaining the core raytracing functionality and adding new features.

## Current State
- **Original**: Qt + libqglviewer (15 years old)
- **Current**: GLFW + OpenGL 3.3 + GLM (WIP branch `feat/refacto-with-qlfw`)
- **Core Features**: KdTree acceleration, shadows, ambient occlusion, anti-aliasing
- **Issue**: Qt dependencies (`QImage`, `QString`) mixed with GLFW

## Target Architecture

### Technology Stack
- **Core**: Modern C++17/20
- **Graphics**: GLFW 3.3 + OpenGL 4.1+
- **Math**: GLM (already integrated)
- **Build**: CMake (already configured)
- **GUI**: Dear ImGui (lightweight, perfect for tools)
- **Image**: STB image library (header-only)

### Dual-View Architecture
```
┌─────────────────┬──────────────────┐
│   GL Viewer     │  Raytracer View  │
│  (Real-time)    │   (High Quality) │
│                 │                  │
│ • Mesh preview  │ • Final renders  │
│ • Fast display  │ • Phong shading  │
│ • Camera ctrl   │ • Reflections    │
│ • Material edit │ • Textures       │
└─────────────────┴──────────────────┘
```

## Implementation Phases

### Phase 1: Clean Foundation ⭐ CURRENT
**Goal**: Remove Qt dependencies, establish modern C++ foundation

1. **Image Handling Modernization**
   - Replace `QImage` with custom Image class using STB
   - Implement save/load functionality (PNG, JPG, TGA)
   - Create GPU texture upload utilities

2. **String Handling**
   - Remove `QString` dependencies
   - Use `std::string` and `std::filesystem` for file operations

3. **Dear ImGui Integration**
   - Add ImGui to CMake build
   - Create basic window with menu system
   - Implement camera controls UI

4. **Build System Cleanup**
   - Update CMakeLists.txt for modern dependencies
   - Remove Qt references
   - Add STB and ImGui as dependencies

### Phase 2: Dual Rendering System
**Goal**: Create interactive GL viewer + high-quality raytracer

1. **GL Viewport Implementation**
   - Real-time mesh rendering with basic shading
   - Interactive camera (orbit, pan, zoom)
   - Material preview capabilities

2. **Raytracer Viewport**
   - High-quality offline rendering
   - Progress display and cancellation
   - Result display and export

3. **Unified Scene Management**
   - Single scene representation for both renderers
   - Synchronized cameras and lighting
   - Live preview → final render workflow

### Phase 3: Advanced Rendering Features
**Goal**: Add modern shading and visual effects

1. **Phong/Blinn-Phong Shading**
   - Implement in raytracer
   - Add material property controls
   - Support for multiple light types

2. **Physically-Based Materials**
   - Metallic/roughness workflow
   - Fresnel reflections
   - Advanced BRDF models

3. **Texture Mapping**
   - UV coordinate support
   - Diffuse, normal, and material maps
   - Texture loading and management

4. **Post-Processing Effects**
   - Tone mapping (ACES, Reinhard)
   - Gamma correction
   - Anti-aliasing improvements

### Phase 4: Modern C++ Enhancements
**Goal**: Leverage modern C++ features for better code quality

1. **Memory Management**
   - Smart pointers (`std::unique_ptr`, `std::shared_ptr`)
   - RAII principles
   - Container improvements

2. **Performance Optimizations**
   - Modern STL algorithms
   - Move semantics
   - Optional threading with `std::async`

3. **Code Quality**
   - Const correctness
   - Type safety improvements
   - Better error handling

## Current Dependencies Status
- ✅ **GLFW**: Already integrated and working
- ✅ **OpenGL**: Core 3.3 context established
- ✅ **GLM**: Math library integrated
- ✅ **GLAD**: OpenGL loader working
- ❌ **Qt**: Remove QImage, QString dependencies
- ⏳ **STB**: Need to add for image handling
- ⏳ **Dear ImGui**: Need to add for modern UI

## Development Principles
1. **Incremental Progress**: Each phase should result in a working application
2. **No Regressions**: Maintain existing raytracing quality
3. **Modern C++**: Use C++17/20 features appropriately
4. **Performance**: Keep real-time GL view responsive
5. **Extensibility**: Design for easy addition of new features

## Success Metrics
- **Phase 1**: Qt-free build, basic ImGui interface
- **Phase 2**: Dual viewport working with basic rendering
- **Phase 3**: Phong shading and texture support
- **Phase 4**: Modern C++ codebase with improved performance

---
*Last Updated: 2025-01-22*
*Current Phase: Phase 1 - Clean Foundation*