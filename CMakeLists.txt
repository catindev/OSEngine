cmake_minimum_required(VERSION 3.20)
project(OpenStrike VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type default
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# Options
option(OSTRIKE_BUILD_TESTS    "Build unit tests"           ON)
option(OSTRIKE_METAL_RENDERER "Use Metal renderer (macOS)" OFF)
option(OSTRIKE_GL_RENDERER    "Use OpenGL renderer"        ON)

# macOS: enable Metal option automatically
if(APPLE)
  option(OSTRIKE_APPLE_SILICON "Optimize for Apple Silicon" ON)
endif()

# Platform detection
if(APPLE)
  add_compile_definitions(OS_MACOS)
elseif(UNIX)
  add_compile_definitions(OS_LINUX)
elseif(WIN32)
  add_compile_definitions(OS_WINDOWS)
endif()

# Compiler flags
if(MSVC)
  add_compile_options(/W4 /WX-)
else()
  add_compile_options(-Wall -Wextra -Wno-unused-parameter)
  if(APPLE AND OSTRIKE_APPLE_SILICON)
    add_compile_options(-mcpu=apple-m1)
  endif()
endif()

# Dependencies
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

# glad (OpenGL loader) — bundled
add_library(glad STATIC
  third_party/glad/src/glad.c
)
target_include_directories(glad PUBLIC third_party/glad/include)

# stb — header-only, bundled
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE third_party/stb)

# Engine library
add_library(ostrike_engine STATIC
  src/math/Angles.cpp
  src/formats/BSP.cpp
  src/formats/WAD.cpp
  src/formats/MDL.cpp
  src/formats/SPR.cpp
  src/formats/Entity.cpp
  src/engine/Engine.cpp
  src/engine/Camera.cpp
  src/engine/Config.cpp
  src/engine/AssetManager.cpp
  src/physics/Movement.cpp
  src/physics/Collision.cpp
  src/renderer/GLRenderer.cpp
  src/renderer/Texture.cpp
  src/audio/Audio.cpp
  src/game/GameRules.cpp
  src/game/Player.cpp
  src/game/Weapon.cpp
)

target_include_directories(ostrike_engine PUBLIC src)

target_link_libraries(ostrike_engine PUBLIC
  glad
  stb
  SDL2::SDL2
  OpenGL::GL
)

if(APPLE)
  target_link_libraries(ostrike_engine PUBLIC
    "-framework AudioToolbox"
    "-framework CoreAudio"
    "-framework Metal"
    "-framework MetalKit"
    "-framework QuartzCore"
  )
endif()

# Main executable
add_executable(openstrike src/main.cpp)
target_link_libraries(openstrike PRIVATE ostrike_engine SDL2::SDL2main)

# Tests
if(OSTRIKE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# Installation
install(TARGETS openstrike DESTINATION bin)
