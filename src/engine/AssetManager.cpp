#include "AssetManager.h"
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace fs = std::filesystem;

namespace OS {

static std::string JoinPath(const std::string& a, const std::string& b) {
    return (fs::path(a) / b).string();
}

void AssetManager::Init(const std::string& csDir, const std::string& gameDir) {
    m_csDir       = csDir;
    m_gameDir     = gameDir;
    m_fullGameDir = JoinPath(csDir, gameDir);
}

std::string AssetManager::Resolve(const std::string& gamePath) const {
    // Normalize: replace backslashes
    std::string norm = gamePath;
    for (char& c : norm) if (c == '\\') c = '/';

    // 1. Game dir
    std::string full = JoinPath(m_fullGameDir, norm);
    if (fs::exists(full)) return full;

    // 2. valve (base)
    full = JoinPath(m_csDir + "/valve", norm);
    if (fs::exists(full)) return full;

    // 3. Absolute or relative fallback
    if (fs::exists(gamePath)) return gamePath;

    return ""; // not found
}

const BSPFile* AssetManager::LoadBSP(const std::string& mapName) {
    auto key = mapName;
    auto it = m_bspCache.find(key);
    if (it != m_bspCache.end()) return &it->second;

    std::string path = Resolve("maps/" + mapName + ".bsp");
    if (path.empty()) {
        // Try absolute path
        path = Resolve(mapName);
    }
    if (path.empty()) return nullptr;

    BSPFile bsp = BSPLoader::Load(path);
    m_bspCache[key] = std::move(bsp);
    return &m_bspCache[key];
}

void AssetManager::LoadWADsForMap(const BSPFile& bsp) {
    int loaded = 0;
    for (const std::string& wadPath : bsp.wadFiles) {
        // BSP "wad" keys store Windows-style paths (e.g. "\cstrike\cstrike.wad"
        // or "C:\sierra\half-life\valve\halflife.wad"). On POSIX, backslashes
        // are NOT path separators, so std::filesystem won't split them — pull
        // out the basename manually after the last slash of either flavour.
        std::string norm = wadPath;
        for (char& c : norm) if (c == '\\') c = '/';
        size_t slash = norm.find_last_of('/');
        std::string wadName = (slash == std::string::npos) ? norm
                                                           : norm.substr(slash + 1);

        std::string resolved = Resolve(norm);
        if (resolved.empty()) resolved = Resolve("cstrike/" + wadName);
        if (resolved.empty()) resolved = Resolve(wadName);
        if (resolved.empty()) {
            for (const char* dir : {"", "valve/", "cstrike/", "cstrike_downloads/"}) {
                resolved = JoinPath(m_csDir, std::string(dir) + wadName);
                if (fs::exists(resolved)) break;
                resolved = "";
            }
        }

        if (!resolved.empty() && m_wadManager.AddWAD(resolved)) {
            loaded++;
        } else {
            fprintf(stderr, "[WAD] not found: %s (basename '%s')\n",
                    wadPath.c_str(), wadName.c_str());
        }
    }
    fprintf(stderr, "[WAD] loaded %d/%zu wads across %d files\n",
            loaded, bsp.wadFiles.size(), m_wadManager.WADCount());
}

const WADTexture* AssetManager::FindTexture(const std::string& name) const {
    return m_wadManager.FindTexture(name);
}

const MDLFile* AssetManager::LoadMDL(const std::string& modelPath) {
    auto it = m_mdlCache.find(modelPath);
    if (it != m_mdlCache.end()) return &it->second;

    std::string path = Resolve(modelPath);
    if (path.empty()) return nullptr;

    MDLFile mdl = MDLLoader::Load(path);
    m_mdlCache[modelPath] = std::move(mdl);
    return &m_mdlCache[modelPath];
}

const SPRFile* AssetManager::LoadSPR(const std::string& spritePath) {
    auto it = m_sprCache.find(spritePath);
    if (it != m_sprCache.end()) return &it->second;

    std::string path = Resolve(spritePath);
    if (path.empty()) return nullptr;

    SPRFile spr = SPRLoader::Load(path);
    m_sprCache[spritePath] = std::move(spr);
    return &m_sprCache[spritePath];
}

} // namespace OS
