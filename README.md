# RayTracer

**Original project by Benjamin Combourieu, Alexandre Duros et Raphaël Moutard.**

A modern C++ raytracer with real-time 3D viewer, featuring dual-viewport architecture for interactive model visualization and high-quality ray-traced rendering.

## Project History

This raytracer was originally developed 15+ years ago using Qt and libqglviewer. It has been completely modernized to use:
- **GLFW 3.3** + **OpenGL 3.3 Core** (replacing Qt/libqglviewer)
- **Dear ImGui** for immediate-mode GUI
- **STB** libraries for image handling
- **GLM** for mathematics
- **Modern C++17** standards

The interface has been customized to allow users to freely test the different available effects.

## Features

- **Dual-Viewport System**: Real-time OpenGL 3D viewer alongside raytracer output
- **Modern UI**: Dear ImGui interface with fixed panels for settings and controls
- **3D Model Loading**: Support for OFF format files
- **Ray Tracing**: High-quality offline rendering with configurable parameters
- **KD-Tree Acceleration**: Spatial acceleration structure for fast ray-triangle intersections
- **Material System**: Diffuse, specular, and ambient lighting models
- **Automatic Model Centering**: Models are automatically centered and scaled for optimal viewing

## Requirements

### macOS (Homebrew)
```bash
# Install dependencies
brew install cmake glfw glm
```

### Linux (Ubuntu/Debian)
```bash
# Install dependencies
sudo apt-get update
sudo apt-get install build-essential cmake libglfw3-dev libglm-dev
```

### Linux (Fedora/CentOS)
```bash
# Install dependencies
sudo dnf install cmake glfw-devel glm-devel gcc-c++
```

## Compilation

### Option 1: Using CMake (Recommended)
```bash
# Navigate to source directory
cd src/

# Build the project
make clean && make

# Or manually with cmake
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

### Option 2: Manual cmake build
```bash
cd src/
mkdir -p build
cd build
cmake -DCMAKE_CXX_STANDARD=17 ..
make -j$(nproc)
```

## Execution

### From src/ directory:
```bash
# After compilation
./raymini

# Or with a specific model
./raymini models/minion.off
```

### From build/ directory:
```bash
# If using manual cmake build
cd build/
./raymini
```

## Usage

1. **3D Viewer**: Use mouse to rotate, zoom, and pan the 3D model
   - Left click + drag: Rotate camera
   - Right click + drag: Zoom in/out
   - Middle click + drag: Pan camera

2. **Raytracer Panel**: Configure rendering settings
   - Adjust camera parameters
   - Set image resolution
   - Configure lighting and materials
   - Click "Render" to generate ray-traced image

3. **Model Loading**: Load different 3D models in OFF format
   - Use the file browser in the interface
   - Models are automatically centered and scaled

## Architecture Overview

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        RayTracer Application                    │
│                                                                 │
│  ┌─────────────────┐                    ┌─────────────────┐    │
│  │   GLFW Window   │                    │  Dear ImGui UI  │    │
│  │                 │                    │                 │    │
│  │  ┌───────────┐  │                    │ ┌─────────────┐ │    │
│  │  │ OpenGL 3D │  │◄───────────────────┤ │ Control     │ │    │
│  │  │ Viewer    │  │                    │ │ Panels      │ │    │
│  │  │           │  │                    │ │             │ │    │
│  │  │ Real-time │  │                    │ │ - Camera    │ │    │
│  │  │ Preview   │  │                    │ │ - Lighting  │ │    │
│  │  └───────────┘  │                    │ │ - Material  │ │    │
│  └─────────────────┘                    │ │ - Render    │ │    │
│                                         │ └─────────────┘ │    │
│                                         └─────────────────┘    │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                Ray Tracing Engine                      │   │
│  │                                                         │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐    │   │
│  │  │   Scene     │◄─┤  KdTree     │◄─┤ RayTracer   │    │   │
│  │  │ Management  │  │ Acceleration│  │   Core      │    │   │
│  │  │             │  │ Structure   │  │             │    │   │
│  │  │ - Objects   │  │             │  │ - Ray-Tri   │    │   │
│  │  │ - Lights    │  │ - Spatial   │  │   Intersect │    │   │
│  │  │ - Materials │  │   Partition │  │ - Shading   │    │   │
│  │  │ - Camera    │  │ - Fast Hit  │  │ - Image Gen │    │   │
│  │  └─────────────┘  │   Testing   │  └─────────────┘    │   │
│  │                   └─────────────┘                     │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

### Component Architecture

```
                    ┌─────────────────┐
                    │   Main.cpp      │
                    │ Application     │
                    │ Entry Point     │
                    └─────────┬───────┘
                              │
                    ┌─────────▼───────┐
                    │  GLFW + OpenGL  │
                    │   Window Mgmt   │
                    └─────────┬───────┘
                              │
              ┌───────────────┼───────────────┐
              │               │               │
    ┌─────────▼───────┐ ┌─────▼─────┐ ┌───────▼───────┐
    │   Dear ImGui    │ │  OpenGL   │ │  RayTracer    │
    │   Interface     │ │  Renderer │ │   Engine      │
    │                 │ │           │ │               │
    │ • Control Panel │ │ • Shaders │ │ ┌───────────┐ │
    │ • File Browser  │ │ • Buffers │ │ │   Scene   │ │
    │ • Render Config │ │ • Textures│ │ │           │ │
    └─────────────────┘ └───────────┘ │ └───────────┘ │
                                      │ ┌───────────┐ │
                                      │ │  KdTree   │ │
                                      │ │           │ │
                                      │ └───────────┘ │
                                      │ ┌───────────┐ │
                                      │ │   Image   │ │
                                      │ │  Output   │ │
                                      │ └───────────┘ │
                                      └───────────────┘
```

### Data Flow

```
    User Input (Mouse/Keyboard)
                │
                ▼
         ┌─────────────┐
         │ Dear ImGui  │
         │   Events    │
         └──────┬──────┘
                │
        ┌───────┼───────┐
        │       │       │
        ▼       ▼       ▼
   ┌────────┐ ┌────┐ ┌──────┐
   │ Camera │ │Lit │ │Render│
   │ Update │ │Upd │ │Trigg │
   └───┬────┘ └─┬──┘ └───┬──┘
       │        │        │
       ▼        ▼        ▼
   ┌─────────────────────────┐
   │     OpenGL Viewer       │
   │   (Real-time update)    │
   └─────────────────────────┘
                │
                ▼
          ┌──────────┐
          │   Mesh   │        ┌────────────┐
          │ Loading  │◄───────┤ OFF File   │
          └────┬─────┘        │ Parser     │
               │              └────────────┘
               ▼
          ┌──────────┐        ┌────────────┐
          │  Scene   │◄───────┤  Material  │
          │ Building │        │ & Lighting │
          └────┬─────┘        └────────────┘
               │
               ▼
          ┌──────────┐
          │ KdTree   │
          │ Building │
          └────┬─────┘
               │
               ▼
          ┌──────────┐        ┌────────────┐
          │   Ray    │────────┤   Image    │
          │ Tracing  │        │  Output    │
          └──────────┘        └────────────┘
```

### Class Hierarchy

```
Core Classes:
├── Scene (Singleton)
│   ├── Objects[]
│   ├── Lights[]
│   └── Camera
│
├── RayTracer (Singleton)
│   ├── render()
│   ├── rayTrace()
│   └── buildKDTrees()
│
├── KdTree
│   ├── build()
│   ├── hasHit()
│   ├── searchHit()
│   └── renderGL()
│
├── Object
│   ├── Mesh
│   ├── Material
│   └── KdTree
│
├── Mesh
│   ├── Vertices[]
│   ├── Triangles[]
│   ├── loadOFF()
│   └── split()
│
├── Ray
│   ├── origin
│   ├── direction
│   ├── intersect()
│   ├── hasHit()
│   └── nearestHit()
│
├── Image
│   ├── width/height
│   ├── pixels[]
│   ├── save()
│   ├── load()
│   └── setPixel()
│
└── Geometric Primitives
    ├── Vertex
    ├── Triangle
    ├── BoundingBox
    ├── Light
    └── Material
```

## File Structure

```
RayTracer/
├── src/                    # Source code
│   ├── Main.cpp           # Application entry point & ImGui interface
│   ├── Image.h/cpp        # Modern image handling (replaces QImage)
│   ├── RayTracer.h/cpp    # Core raytracing engine
│   ├── Scene.h/cpp        # Scene management
│   ├── KdTree.h/cpp       # Spatial acceleration structure
│   ├── Object.h/cpp       # 3D objects with materials
│   ├── Mesh.h/cpp         # 3D mesh loading and manipulation
│   ├── Ray.h/cpp          # Ray casting and intersection
│   ├── Vertex.h/cpp       # 3D vertices with properties
│   ├── Triangle.h/cpp     # Triangle primitives
│   ├── BoundingBox.h/cpp  # Axis-aligned bounding boxes
│   ├── Light.h/cpp        # Light sources
│   ├── Material.h/cpp     # Material properties
│   ├── glad/              # OpenGL loader
│   ├── imgui/             # Dear ImGui library
│   ├── stb/               # STB image libraries
│   ├── models/            # 3D model files
│   └── CMakeLists.txt     # Build configuration
├── .vscode/               # VSCode configuration
└── README.md              # This file
```

## Troubleshooting

### Build Issues
- Ensure GLFW and GLM are properly installed
- Check that you're using C++17 compatible compiler (GCC 7+, Clang 5+)
- Verify include paths in VSCode settings if using IDE

### Runtime Issues
- Make sure you're running from the correct directory (src/ or build/)
- Ensure model files exist in the models/ directory
- Check OpenGL 3.3 support on your graphics hardware

### VSCode Integration
The project includes VSCode configuration for:
- C++ IntelliSense with proper include paths
- CMake integration
- Debugging support

## Development

This modernized version maintains the core raytracing algorithms while updating the infrastructure:
- Removed Qt dependencies completely
- Implemented modern OpenGL rendering pipeline
- Added Dear ImGui for immediate-mode interface
- Updated to C++17 standards with RAII and smart pointers
- Enhanced build system with CMake

## Future Enhancements

- [ ] Connect raytracer to render button
- [ ] Add Phong shading implementation
- [ ] Implement additional material models
- [ ] Add more 3D model format support
- [ ] Enhance lighting system

---

*Modernized for C++17 with GLFW + OpenGL 3.3 + Dear ImGui*