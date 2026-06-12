#include "engine/Engine.h"
#include <cstdio>
#include <cstring>
#include <string>

static void PrintUsage(const char* exe) {
    printf("Usage: %s --cs-dir <path> [options]\n\n", exe);
    printf("Options:\n");
    printf("  --cs-dir <path>    Path to CS 1.6 installation (required)\n");
    printf("  --game <dir>       Game directory name (default: cstrike)\n");
    printf("  --map <name>       Map to load on startup (e.g. de_dust2)\n");
    printf("  --width <n>        Window width  (default: 1280)\n");
    printf("  --height <n>       Window height (default: 720)\n");
    printf("  --fullscreen       Launch fullscreen\n");
    printf("  --fov <deg>        Field of view (default: 90)\n");
    printf("\nExample:\n");
    printf("  %s --cs-dir ~/.steam/steam/steamapps/common/Counter-Strike --map de_dust2\n\n", exe);
}

int main(int argc, char** argv) {
    OS::EngineConfig cfg;
    bool haveCSDir = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--cs-dir") == 0 && i+1 < argc) {
            cfg.csDir = argv[++i];
            haveCSDir = true;
        } else if (strcmp(argv[i], "--game") == 0 && i+1 < argc) {
            cfg.gameDir = argv[++i];
        } else if (strcmp(argv[i], "--map") == 0 && i+1 < argc) {
            cfg.startMap = argv[++i];
        } else if (strcmp(argv[i], "--width") == 0 && i+1 < argc) {
            cfg.width = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "--height") == 0 && i+1 < argc) {
            cfg.height = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "--fullscreen") == 0) {
            cfg.fullscreen = true;
        } else if (strcmp(argv[i], "--fov") == 0 && i+1 < argc) {
            cfg.fov = std::stof(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            PrintUsage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            PrintUsage(argv[0]);
            return 1;
        }
    }

    if (!haveCSDir) {
        fprintf(stderr, "Error: --cs-dir is required\n\n");
        PrintUsage(argv[0]);
        return 1;
    }

    printf("OpenStrike v0.1.0\n");
    printf("CS dir: %s\n", cfg.csDir.c_str());

    OS::Engine engine;
    if (!engine.Init(cfg)) {
        fprintf(stderr, "Engine initialization failed\n");
        return 1;
    }

    engine.Run();
    return 0;
}
