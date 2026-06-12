#pragma once
// GoldSrc BSP version 30 format loader
// Reference: Quake/GoldSrc BSP specification (public domain documentation)
// No Valve code used; structs derived from publicly documented binary format.

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include "../math/Math.h"

namespace OS {

// ─── BSP constants ────────────────────────────────────────────────────────────

static constexpr int32_t BSP_VERSION      = 30;
static constexpr int      BSP_HEADER_LUMPS = 15;
static constexpr int      BSP_MAX_HULLS    = 4;
static constexpr int      BSP_MAXLIGHTMAPS = 4;
static constexpr int      BSP_MAX_AMBIENT  = 4;

// ─── Lump indices ─────────────────────────────────────────────────────────────

enum BSPLumpIndex {
    BSP_LUMP_ENTITIES     = 0,
    BSP_LUMP_PLANES       = 1,
    BSP_LUMP_TEXTURES     = 2,
    BSP_LUMP_VERTICES     = 3,
    BSP_LUMP_VISIBILITY   = 4,
    BSP_LUMP_NODES        = 5,
    BSP_LUMP_TEXINFO      = 6,
    BSP_LUMP_FACES        = 7,
    BSP_LUMP_LIGHTING     = 8,
    BSP_LUMP_CLIPNODES    = 9,
    BSP_LUMP_LEAVES       = 10,
    BSP_LUMP_MARKSURFACES = 11,
    BSP_LUMP_EDGES        = 12,
    BSP_LUMP_SURFEDGES    = 13,
    BSP_LUMP_MODELS       = 14,
};

// ─── Plane axis types ─────────────────────────────────────────────────────────

enum BSPPlaneType {
    PLANE_X     = 0,
    PLANE_Y     = 1,
    PLANE_Z     = 2,
    PLANE_ANYX  = 3,
    PLANE_ANYY  = 4,
    PLANE_ANYZ  = 5,
};

// ─── Leaf contents ────────────────────────────────────────────────────────────

enum BSPContents {
    CONTENTS_EMPTY          = -1,
    CONTENTS_SOLID          = -2,
    CONTENTS_WATER          = -3,
    CONTENTS_SLIME          = -4,
    CONTENTS_LAVA           = -5,
    CONTENTS_SKY            = -6,
    CONTENTS_ORIGIN         = -7,
    CONTENTS_CLIP           = -8,
    CONTENTS_CURRENT_0      = -9,
    CONTENTS_CURRENT_90     = -10,
    CONTENTS_CURRENT_180    = -11,
    CONTENTS_CURRENT_270    = -12,
    CONTENTS_CURRENT_UP     = -13,
    CONTENTS_CURRENT_DOWN   = -14,
    CONTENTS_TRANSLUCENT    = -15,
    CONTENTS_LADDER         = -16,
};

// ─── Raw binary structs (must match GoldSrc on-disk layout exactly) ──────────
// All integers are little-endian.

#pragma pack(push, 1)

struct BSPRawLump {
    int32_t offset;
    int32_t length;
};

struct BSPRawHeader {
    int32_t   version;
    BSPRawLump lumps[BSP_HEADER_LUMPS];
};

struct BSPRawPlane {
    float   normal[3];
    float   dist;
    int32_t type;
};

struct BSPRawNode {
    int32_t  plane;
    int16_t  children[2]; // >0: node index, <0: leaf index (-(leaf+1))
    int16_t  mins[3];
    int16_t  maxs[3];
    uint16_t firstface;
    uint16_t numfaces;
};

struct BSPRawLeaf {
    int32_t  contents;
    int32_t  visofs;
    int16_t  mins[3];
    int16_t  maxs[3];
    uint16_t firstmarksurface;
    uint16_t nummarksurfaces;
    uint8_t  ambient_level[BSP_MAX_AMBIENT];
};

struct BSPRawFace {
    uint16_t plane;
    uint16_t side;
    int32_t  firstedge;
    int16_t  numedges;
    int16_t  texinfo;
    uint8_t  styles[BSP_MAXLIGHTMAPS];
    int32_t  lightofs;
};

struct BSPRawEdge {
    uint16_t v[2];
};

struct BSPRawTexInfo {
    float    vecs[2][4]; // s and t vectors with offsets
    int32_t  miptex;
    int32_t  flags;
};

struct BSPRawMipTex {
    char     name[16];
    uint32_t width;
    uint32_t height;
    uint32_t offsets[4]; // 0 = external (WAD), else relative to struct
};

struct BSPRawMipTexHeader {
    int32_t num_miptex;
    int32_t offsets[1]; // variable length, relative to lump start
};

struct BSPRawModel {
    float   mins[3];
    float   maxs[3];
    float   origin[3];
    int32_t headnode[BSP_MAX_HULLS];
    int32_t visleafs;
    int32_t firstface;
    int32_t numfaces;
};

struct BSPRawClipNode {
    int32_t plane;
    int16_t children[2]; // <0: BSPContents
};

#pragma pack(pop)

// ─── Parsed representation ────────────────────────────────────────────────────

struct BSPFaceVertex {
    Vec3 pos;
    Vec2 uv;       // lightmap UV
    Vec2 texUV;    // texture UV
};

struct BSPMesh {
    std::vector<BSPFaceVertex> verts;
    std::vector<uint32_t>      indices;
    int                        textureName = -1; // index into BSPFile::textures
    int                        lightmapId  = -1;
};

struct BSPTexture {
    std::string  name;
    uint32_t     width  = 0;
    uint32_t     height = 0;
    bool         embedded = false;
    // Pixel data for embedded textures (mip 0, RGBA8)
    std::vector<uint8_t> pixels;
    // WAD lookup key (empty if embedded)
    std::string wadName;
};

struct BSPNode {
    Plane    plane;
    int32_t  children[2]; // >0: node, <0: leaf (-(leaf+1))
    AABB     bounds;
    uint32_t firstFace;
    uint32_t numFaces;
};

struct BSPLeaf {
    int32_t  contents;
    int32_t  visofs;
    AABB     bounds;
    uint32_t firstMarkSurface;
    uint32_t numMarkSurfaces;
    uint8_t  ambientLevel[4];
};

struct BSPModel {
    AABB     bounds;
    Vec3     origin;
    int32_t  headnode[BSP_MAX_HULLS];
    uint32_t firstFace;
    uint32_t numFaces;
};

struct BSPClipNode {
    Plane   plane;
    int16_t children[2];
};

// Complete loaded BSP file
struct BSPFile {
    // Entity string (raw key-value text)
    std::string entities;

    // Geometry
    std::vector<Plane>         planes;
    std::vector<Vec3>          vertices;
    std::vector<BSPRawTexInfo> texInfos;
    std::vector<BSPRawFace>    rawFaces;
    std::vector<BSPRawEdge>    edges;
    std::vector<int32_t>       surfEdges;
    std::vector<uint16_t>      markSurfaces;

    // Tree
    std::vector<BSPNode>       nodes;
    std::vector<BSPLeaf>       leaves;
    std::vector<BSPClipNode>   clipNodes;

    // Models (brush entities)
    std::vector<BSPModel>      models;

    // Textures
    std::vector<BSPTexture>    textures;
    // Names of WAD files referenced in entities
    std::vector<std::string>   wadFiles;

    // Lightmap raw data (RGB triplets)
    std::vector<uint8_t> lighting;

    // Visibility data (RLE-encoded PVS)
    std::vector<uint8_t> visibility;

    // Pre-built renderable meshes (one per face)
    std::vector<BSPMesh> meshes;

    // Collision structures (per hull, per model)
    // Hull 0: point hull (used for BSP traversal / leaf queries)
    // Hull 1: player standing (32x32x72)
    // Hull 2: player crouching (32x32x36)
    // Hull 3: large entity
    struct HullData {
        std::vector<BSPClipNode> nodes;
    };
    HullData hulls[BSP_MAX_HULLS];

    bool valid = false;
    std::string error;
};

// ─── Loader ───────────────────────────────────────────────────────────────────

class BSPLoader {
public:
    // Load a BSP file from disk.
    // Returns a BSPFile with valid=true on success.
    static BSPFile Load(const std::string& path);

private:
    static void ParseEntities(BSPFile& bsp, const std::vector<uint8_t>& data, const BSPRawLump& lump);
    static void ParsePlanes  (BSPFile& bsp, const std::vector<uint8_t>& data, const BSPRawLump& lump);
    static void ParseTextures(BSPFile& bsp, const std::vector<uint8_t>& data, const BSPRawLump& lump);
    static void ParseVertices(BSPFile& bsp, const std::vector<uint8_t>& data, const BSPRawLump& lump);
    static void ParseNodes   (BSPFile& bsp, const std::vector<uint8_t>& data, const BSPRawLump& lump);
    static void ParseTexInfo (BSPFile& bsp, const std::vector<uint8_t>& data, const BSPRawLump& lump);
    static void ParseFaces   (BSPFile& bsp, const std::vector<uint8_t>& data, const BSPRawLump& lump);
    static void ParseLighting(BSPFile& bsp, const std::vector<uint8_t>& data, const BSPRawLump& lump);
    static void ParseClipNodes(BSPFile& bsp, const std::vector<uint8_t>& data, const BSPRawLump& lump);
    static void ParseLeaves  (BSPFile& bsp, const std::vector<uint8_t>& data, const BSPRawLump& lump);
    static void ParseMarkSurfaces(BSPFile& bsp, const std::vector<uint8_t>& data, const BSPRawLump& lump);
    static void ParseEdges   (BSPFile& bsp, const std::vector<uint8_t>& data, const BSPRawLump& lump);
    static void ParseSurfEdges(BSPFile& bsp, const std::vector<uint8_t>& data, const BSPRawLump& lump);
    static void ParseModels  (BSPFile& bsp, const std::vector<uint8_t>& data, const BSPRawLump& lump);
    static void ParseVisibility(BSPFile& bsp, const std::vector<uint8_t>& data, const BSPRawLump& lump);
    static void BuildMeshes  (BSPFile& bsp);
    static void ExtractWadList(BSPFile& bsp);
};

// ─── BSP tree queries ─────────────────────────────────────────────────────────

class BSPTree {
public:
    explicit BSPTree(const BSPFile& bsp) : m_bsp(bsp) {}

    // Find the leaf a point is in
    int FindLeaf(Vec3 point) const;
    int FindLeafInModel(Vec3 point, int modelIndex) const;

    // PVS: check if leaf B is visible from leaf A
    bool IsVisible(int leafA, int leafB) const;

    // Decode PVS for a leaf (returns set of visible leaf indices)
    std::vector<int> GetPVS(int leaf) const;

    // Point contents (what material the point is in)
    int PointContents(Vec3 point, int hull = 0) const;
    int PointContentsInModel(Vec3 point, int modelIndex, int hull = 0) const;

private:
    const BSPFile& m_bsp;

    int TraverseNode(int nodeIndex, Vec3 point) const;
    int TraverseClipNode(int nodeIndex, Vec3 point, int hull) const;
    void DecompressPVS(const uint8_t* data, int numLeaves, std::vector<int>& out) const;
};

} // namespace OS
