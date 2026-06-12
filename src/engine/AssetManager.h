#pragma once
// Asset manager — resolves paths and caches loaded assets.
// Knows about the CS 1.6 install directory and OpenStrike override directory.

#include <string>
#include <memory>
#include <unordered_map>
#include "../formats/BSP.h"
#include "../formats/WAD.h"
#include "../formats/MDL.h"
#include "../formats/SPR.h"

namespace OS {

class AssetManager {
public:
    // Call once at startup.
    // csDir:     path to user's CS 1.6 installation (e.g. ~/.steam/cs)
    // gameDir:   sub-directory within csDir (default "cstrike")
    void Init(const std::string& csDir, const std::string& gameDir = "cstrike");

    // Resolve a game-relative path to an absolute path.
    // Searches: override dir → gameDir → csDir/valve (base game)
    std::string Resolve(const std::string& gamePath) const;

    // Load a BSP file (cached)
    const BSPFile* LoadBSP(const std::string& mapName);

    // Load all WADs listed in a BSP's entity string
    void LoadWADsForMap(const BSPFile& bsp);

    // Look up a texture by name across all loaded WADs + embedded BSP textures
    const WADTexture* FindTexture(const std::string& name) const;

    // Load an MDL model (cached)
    const MDLFile* LoadMDL(const std::string& modelPath);

    // Load an SPR sprite (cached)
    const SPRFile* LoadSPR(const std::string& spritePath);

    const std::string& CSDir()   const { return m_csDir; }
    const std::string& GameDir() const { return m_gameDir; }
    const std::string& FullGameDir() const { return m_fullGameDir; }

private:
    std::string m_csDir;
    std::string m_gameDir;
    std::string m_fullGameDir; // csDir/gameDir

    WADManager m_wadManager;

    std::unordered_map<std::string, BSPFile> m_bspCache;
    std::unordered_map<std::string, MDLFile> m_mdlCache;
    std::unordered_map<std::string, SPRFile> m_sprCache;
};

} // namespace OS
