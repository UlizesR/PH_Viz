cmake_minimum_required(VERSION 3.16)

project(PH_Viz LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type to RelWithDebInfo if not specified
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
	message(STATUS "Setting default build type to RelWithDebInfo")
endif()

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

add_library(glad
	external/glad/src/glad.c
)

target_include_directories(glad PUBLIC
	external/glad/include
)

find_package(OpenGL REQUIRED)

find_package(GLFW3 QUIET)
if(NOT GLFW3_FOUND)
	find_package(glfw3 QUIET)
endif()

if(NOT GLFW3_FOUND AND NOT glfw3_FOUND)
	message(FATAL_ERROR "GLFW not found. Install via Homebrew: brew install glfw")
endif()

find_package(assimp QUIET)
if(NOT assimp_FOUND)
	message(FATAL_ERROR "Assimp not found. Install via Homebrew: brew install assimp")
endif()

# Optional header-only glm
find_package(glm QUIET)
if(NOT glm_FOUND)
	include(FetchContent)
	FetchContent_Declare(
		glm
		GIT_REPOSITORY https://github.com/g-truc/glm.git
		GIT_TAG 1.0.1
	)
	FetchContent_MakeAvailable(glm)
endif()

# ImGui
include(FetchContent)
FetchContent_Declare(
	imgui
	GIT_REPOSITORY https://github.com/ocornut/imgui.git
	GIT_TAG v1.90.4
)
FetchContent_MakeAvailable(imgui)

# ImGui sources
set(IMGUI_SOURCES
	${imgui_SOURCE_DIR}/imgui.cpp
	${imgui_SOURCE_DIR}/imgui_demo.cpp
	${imgui_SOURCE_DIR}/imgui_draw.cpp
	${imgui_SOURCE_DIR}/imgui_tables.cpp
	${imgui_SOURCE_DIR}/imgui_widgets.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

set(SRC
	src/main.cpp
	src/Graphics/Shader.h
	src/Graphics/Shader.cpp
	src/Graphics/Model.h
	src/Graphics/Model.cpp
	src/Graphics/Camera.hpp
	src/Graphics/Renderer.h
	src/Graphics/Renderer.cpp
	src/Graphics/RenderDevice.hpp
	src/Graphics/View.hpp
	src/Graphics/Scene.hpp
	src/Graphics/RenderUtils.hpp
	src/Graphics/DynamicLines.hpp
	src/Graphics/UBO.hpp
	src/Graphics/SpatialIndex.hpp
	src/Graphics/SpatialIndex.cpp
	src/Graphics/Utils.hpp
	src/Graphics/Utils.cpp
	src/Graphics/Scene.cpp
	src/Graphics/Culling/OcclusionCuller.hpp
	src/Graphics/Culling/OcclusionCuller.cpp
	src/Graphics/UI/Inspector.cpp
	${IMGUI_SOURCES}
)

add_executable(PH_Viz ${SRC})

target_include_directories(PH_Viz PRIVATE
	src
	external/glad/include
	${imgui_SOURCE_DIR}
	${imgui_SOURCE_DIR}/backends
)

# Copy shaders into the build directory
add_custom_target(copy_shaders ALL
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/shaders ${CMAKE_CURRENT_BINARY_DIR}/shaders
)
add_dependencies(PH_Viz copy_shaders)

if(TARGET glm::glm)
	target_link_libraries(PH_Viz PRIVATE glm::glm)
endif()

if(TARGET OpenGL::GL)
	target_link_libraries(PH_Viz PRIVATE OpenGL::GL)
else()
	target_link_libraries(PH_Viz PRIVATE ${OPENGL_gl_LIBRARY})
endif()

if(TARGET glfw)
	target_link_libraries(PH_Viz PRIVATE glfw)
elseif(TARGET glfw3)
	target_link_libraries(PH_Viz PRIVATE glfw3)
elseif(TARGET GLFW::GLFW)
	target_link_libraries(PH_Viz PRIVATE GLFW::GLFW)
else()
	message(FATAL_ERROR "Unable to determine GLFW target to link")
endif()

if(TARGET assimp::assimp)
	target_link_libraries(PH_Viz PRIVATE assimp::assimp)
else()
	target_link_libraries(PH_Viz PRIVATE assimp)
endif()

	target_link_libraries(PH_Viz PRIVATE glad)

if(APPLE)
	target_link_libraries(PH_Viz PRIVATE
		"-framework Cocoa"
		"-framework IOKit"
		"-framework CoreVideo"
	)
endif()

if(MSVC)
	target_compile_options(PH_Viz PRIVATE /W4 /permissive-)
else()
    target_compile_options(PH_Viz PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
endif()

# Unit tests
if(CMAKE_BUILD_TYPE MATCHES "Debug|RelWithDebInfo")
	enable_testing()
	add_executable(test_utils tests/test_utils.cpp)
	target_include_directories(test_utils PRIVATE src external/glad/include)
	target_compile_features(test_utils PRIVATE cxx_std_17)
	if(MSVC)
		target_compile_options(test_utils PRIVATE /W4 /permissive-)
	else()
		target_compile_options(test_utils PRIVATE -Wall -Wextra -Wpedantic)
	endif()
	add_test(NAME UtilsTests COMMAND test_utils)
endif()
