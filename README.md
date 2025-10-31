# PH_Viz

A high-performance 3D model viewer and visualization tool built with OpenGL. PH_Viz is designed for visualizing complex 3D models and point clouds with advanced rendering optimizations, making it ideal for working with large datasets and computational geometry applications.

## Features

### 📦 Model Loading
- **Multiple file formats**: Supports `.obj`, `.ply`, and `.off` files
- **Assimp integration**: Robust mesh loading with automatic handling of complex scene graphs
- **Automatic scaling**: Models are automatically centered and scaled to fit a unit box
- **Smart orientation**: Models are oriented to face up (+Y) and towards the camera (+Z)

### 🎨 Rendering Features
- **Physically Based Rendering (PBR)**: Cook-Torrance BRDF shader with hemisphere ambient lighting
- **Point Cloud Support**: Specialized rendering modes for point cloud data:
  - `GL_POINTS`: Fast, simple point rendering
  - **Sphere Impostors**: Geometry shader-based billboard spheres
  - **Instanced Spheres**: Hardware-instanced low-poly spheres
- **Wireframe Mode**: Toggle wireframe rendering for mesh visualization (F2 key)
- **Color Modes**:
  - **Uniform**: Single color for the entire model
  - **Vertex RGB**: Per-vertex colors from model data (e.g., PLY files)
  - **Scalar**: Color mapping based on scalar values (e.g., filtration values)
- **Bounding Box Visualization**: Visualize model bounds and coordinate axes

### 🎮 Interactive Controls

#### Camera Controls
- **FPS-style movement**: WASD for forward/back/left/right, Q/E for up/down
- **Mouse look**: Left-click and drag to rotate the camera
- **Camera presets**: 
  - Save: `Ctrl + 1-9,0`
  - Restore: `1-9,0`
- **Speed modifiers**:
  - `Shift`: Move faster
  - `Ctrl`: Move slower

#### Keyboard Shortcuts
- **F2**: Toggle wireframe mode
- **F5**: Reload shaders (hot-reload)
- **+/-** or **Page Up/Down**: Adjust point size (for point clouds)

### ⚡ Performance Optimizations

PH_Viz includes a comprehensive suite of rendering optimizations:

#### CPU-Side Optimizations
- **Multi-threaded Geometry Processing**: Parallelizes mesh loading and processing for complex models
- **Spatial Indexing**: Octree-based hierarchical LOD for point clouds (100k+ points)
- **Frustum Culling**: Skips rendering objects outside the camera view
- **Vertex Buffer Optimization**: Half-precision floats for positions/UVs when beneficial

#### GPU-Side Optimizations
- **Uniform Buffer Objects (UBOs)**: Efficient uniform data transfer for matrices, materials, and lighting
- **Early-Z Depth Prepass**: Two-pass rendering to leverage hardware Early-Z rejection
- **Occlusion Culling**: Hardware occlusion queries to skip fully occluded objects
- **Index Buffer Optimization**: 16-bit indices when vertex count < 65k
- **OpenGL State Caching**: Minimizes redundant OpenGL state changes
- **Shader State Batching**: Reduces unnecessary shader program switches

#### Point Cloud Optimizations
- **View-Dependent Culling**: Spatial indexing enables culling based on camera position
- **Distance-Based LOD**: Automatically selects rendering mode based on camera distance
- **Hierarchical Rendering**: Octree structure allows rendering only visible portions

### 📊 Performance Profiling

Built-in performance monitoring with ImGui UI:
- **CPU Frame Time**: Real-time CPU rendering time
- **GPU Frame Time**: Hardware GPU timing queries (when available)
- **Draw Call Count**: Number of rendering calls per frame
- **Triangle/Point Count**: Primitives rendered
- **GPU Memory Usage**: GPU memory consumption (when available)

### 🔧 Developer Features
- **Shader Hot-Reloading**: Press F5 or edit shader files to reload shaders at runtime
- **OpenGL Debug Output**: Automatic error detection and reporting
- **Comprehensive Logging**: OpenGL vendor/renderer info, configuration thresholds
- **Unit Tests**: Automated tests for utility functions (half-float conversion, config constants)

### 🎛️ UI Controls

Interactive ImGui interface with:
- Scene controls (wireframe, color mode, bounding box)
- Optimization toggles (frustum culling, occlusion culling, Early-Z prepass)
- Point cloud settings (rendering mode, spatial indexing, auto-LOD)
- Performance profiling display

## Requirements

### macOS (Recommended)
```bash
brew install glfw assimp glm
```

### Linux
```bash
sudo apt-get install libglfw3-dev libassimp-dev libglm-dev  # Debian/Ubuntu
# or
sudo dnf install glfw-devel assimp-devel glm-devel          # Fedora
```

### Windows
- GLFW: Download from [glfw.org](https://www.glfw.org/download.html)
- Assimp: Install via vcpkg or download from [assimp.org](https://www.assimp.org/index.php/downloads)
- GLM: Header-only, downloaded automatically via CMake

## Building

### Basic Build
```bash
mkdir -p build && cd build
cmake ..
cmake --build . -j
```

The default build type is `RelWithDebInfo` (optimized with debug symbols), which is recommended for development.

See [BUILD_TYPES.md](BUILD_TYPES.md) for more details on build configurations.

### Running Unit Tests
```bash
cd build
ctest
# or
./test_utils
```

## Usage

### Basic Usage
```bash
./PH_Viz <path-to-model>
```

### Examples
```bash
# Load an OBJ file
./PH_Viz ../assets/model.obj

# Load a PLY point cloud
./PH_Viz ../assets/pointcloud.ply

# Load an OFF file
./PH_Viz ../assets/mesh.off
```

### Default Model
If no path is provided, PH_Viz will attempt to load `assets/11091_FemaleHead_v4.obj`.

## Technical Details

### Rendering Pipeline
1. **Early-Z Prepass** (optional): Render depth-only pass to populate depth buffer
2. **Occlusion Culling** (optional): Test bounding box visibility using hardware queries
3. **Main Pass**: Render with full PBR shading
   - Frustum culling skips off-screen objects
   - Spatial indexing (for point clouds) renders only visible points
   - UBOs provide efficient uniform data access

### Architecture
- **Modular Design**: Separated into `Renderer`, `Scene`, `View`, and `Model` components
- **RAII Resource Management**: All OpenGL resources are automatically managed
- **Modern C++17**: Uses standard library features and modern practices
- **Header-Only Components**: Camera and some utilities are header-only for flexibility

### Shaders
- **PBR Shader**: `shaders/pbr.vert` + `shaders/pbr.frag`
- **Depth-Only Shader**: `shaders/depth_only.vert` + `shaders/depth_only.frag`
- **Line Shader**: `shaders/line.vert` + `shaders/line.frag`
- **Sphere Impostor**: `shaders/pbr.vert` + `shaders/sphere_impostor.geom` + `shaders/sphere_impostor.frag`
- **Instanced Sphere**: `shaders/instanced_sphere.vert` + `shaders/pbr.frag`

Shaders use UBOs for efficient data transfer and support OpenGL 3.3 compatibility.

## Project Structure

```
PH_Viz/
├── assets/           # 3D model files (.obj, .ply, .off)
├── shaders/          # GLSL shader source files
├── src/              # Source code
│   ├── Graphics/     # Graphics subsystem
│   │   ├── Culling/  # Culling helpers (OcclusionCuller, Frustum)
│   │   ├── UI/       # ImGui UI components
│   │   ├── *.h       # Headers
│   │   └── *.cpp     # Implementations
│   └── main.cpp      # Application entry point
├── tests/            # Unit tests
├── external/         # Third-party dependencies (GLAD)
└── CMakeLists.txt    # Build configuration
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for coding standards, directory layout, and contribution guidelines.

## License

[Add your license here]

## Acknowledgments

- **OpenGL**: Graphics API
- **GLFW**: Window and input management
- **GLAD**: OpenGL loader
- **Assimp**: 3D model loading
- **GLM**: Mathematics library
- **ImGui**: Immediate-mode GUI library

---

For questions, issues, or contributions, please open an issue or submit a pull request.
