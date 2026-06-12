# OpenStrike

Open-source clean-room reimplementation of the Counter-Strike 1.6 runtime environment.

## Overview

OpenStrike reproduces CS 1.6 gameplay behavior without using any Valve or GoldSrc source code.  
It reads user-provided game assets from a legal CS 1.6 installation.

No proprietary assets are included or redistributed.

## Requirements

- Legal copy of Counter-Strike 1.6 (Steam)
- CMake 3.20+
- C++20 compiler (Clang 14+, GCC 12+)
- SDL2
- OpenGL 3.3+

### macOS
- Xcode 14+ / Apple Silicon supported
- Metal renderer planned (Phase 2)

## Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build . -j$(nproc)
```

## Usage

```bash
./openstrike --cs-dir ~/.steam/steam/steamapps/common/Counter-Strike --map de_dust2
```

### Controls

| Key | Action |
|-----|--------|
| WASD | Move |
| Mouse | Look |
| Space | Jump |
| Ctrl | Duck |
| Escape | Toggle mouse lock |

## Architecture

```
src/
  math/        Math library (Vec2, Vec3, Vec4, Mat4, Angles, Plane, AABB, Ray)
  formats/     Asset loaders
    BSP.h/cpp    GoldSrc BSP v30 map format
    WAD.h/cpp    WAD3 texture archive
    MDL.h/cpp    GoldSrc MDL model format
    SPR.h/cpp    GoldSrc SPR sprite format
    Entity.h/cpp BSP entity string parser
  engine/      Core engine
    Engine.h/cpp   Main loop and system orchestration
    Camera.h/cpp   First-person camera
    Config.h/cpp   CVar system
    AssetManager   Asset resolution and caching
  physics/     Movement physics
    Movement.h/cpp CS 1.6 player movement (GoldSrc pmove reimplementation)
    Collision.h/cpp BSP hull tracing
  renderer/    Rendering
    Renderer.h     Abstract renderer interface
    GLRenderer     OpenGL 3.3 renderer
    Texture        Texture cache
  audio/       Audio system (SDL2, 3D spatialized)
  game/        Game logic
    Player       Player state
    Weapon       CS 1.6 weapon definitions and parameters
    GameRules    Round system, economy, win conditions
tests/
  test_movement  Movement physics behavioral tests
  test_bsp       BSP format tests
  test_wad       WAD format tests
  test_entity    Entity parser tests
```

## Compatibility Goals

| System | Status |
|--------|--------|
| BSP map loading | ✅ Phase 1 |
| WAD texture loading | ✅ Phase 1 |
| MDL model loading | ✅ Phase 1 |
| SPR sprite loading | ✅ Phase 1 |
| Player movement physics | ✅ Phase 1 |
| BSP collision (hull tracing) | ✅ Phase 1 |
| OpenGL renderer | ✅ Phase 1 |
| CS 1.6 weapon parameters | ✅ Phase 1 |
| Round/economy system | ✅ Phase 1 |
| 3D audio | ✅ Phase 1 |
| Bot AI | 🔲 Phase 2 |
| Networking | 🔲 Phase 2 |
| Metal renderer (macOS) | 🔲 Phase 2 |
| HUD (full) | 🔲 Phase 2 |
| Weapon animation | 🔲 Phase 2 |

## Legal

OpenStrike contains **no** Valve source code, no GoldSrc SDK code, and no copyrighted game assets.

All binary format parsers are implemented from publicly documented specifications.  
No DRM bypass, Steam bypass, or protection circumvention is performed.

Users must own a legal copy of Counter-Strike 1.6 to use OpenStrike.

## License

MIT — see LICENSE
