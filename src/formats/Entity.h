#pragma once
// BSP entity string parser
// Parses GoldSrc entity key-value format (subset of Quake map format)

#include <string>
#include <vector>
#include <unordered_map>
#include "../math/Math.h"

namespace OS {

using KeyValues = std::unordered_map<std::string, std::string>;

struct Entity {
    KeyValues kv;

    const std::string& Get(const std::string& key, const std::string& def = "") const {
        auto it = kv.find(key);
        return it != kv.end() ? it->second : def;
    }

    const std::string& ClassName() const { return Get("classname"); }
    const std::string& TargetName() const { return Get("targetname"); }

    bool HasKey(const std::string& key) const { return kv.count(key) > 0; }

    Vec3 GetVec3(const std::string& key, Vec3 def = {}) const;
    float GetFloat(const std::string& key, float def = 0.0f) const;
    int   GetInt  (const std::string& key, int   def = 0)   const;
    Angles GetAngles(const std::string& key, Angles def = {}) const;
};

class EntityParser {
public:
    // Parse a GoldSrc entity string into a list of Entity objects
    static std::vector<Entity> Parse(const std::string& text);
};

} // namespace OS
