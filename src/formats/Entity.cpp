#include "Entity.h"
#include <sstream>
#include <cstdio>

namespace OS {

Vec3 Entity::GetVec3(const std::string& key, Vec3 def) const {
    auto it = kv.find(key);
    if (it == kv.end()) return def;
    Vec3 v;
    if (sscanf(it->second.c_str(), "%f %f %f", &v.x, &v.y, &v.z) == 3)
        return v;
    return def;
}

float Entity::GetFloat(const std::string& key, float def) const {
    auto it = kv.find(key);
    if (it == kv.end()) return def;
    try { return std::stof(it->second); } catch (...) { return def; }
}

int Entity::GetInt(const std::string& key, int def) const {
    auto it = kv.find(key);
    if (it == kv.end()) return def;
    try { return std::stoi(it->second); } catch (...) { return def; }
}

Angles Entity::GetAngles(const std::string& key, Angles def) const {
    auto it = kv.find(key);
    if (it == kv.end()) return def;
    Angles a;
    if (sscanf(it->second.c_str(), "%f %f %f", &a.pitch, &a.yaw, &a.roll) >= 2)
        return a;
    return def;
}

std::vector<Entity> EntityParser::Parse(const std::string& text) {
    std::vector<Entity> entities;
    const char* p = text.c_str();
    const char* end = p + text.size();

    auto SkipWS = [&] {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
            p++;
    };

    auto ReadToken = [&](std::string& out) -> bool {
        SkipWS();
        if (p >= end) return false;
        if (*p == '"') {
            p++; // skip opening quote
            const char* start = p;
            while (p < end && *p != '"') p++;
            out.assign(start, p);
            if (p < end) p++; // skip closing quote
            return true;
        }
        // Unquoted token (shouldn't appear in valid entity lump but handle gracefully)
        const char* start = p;
        while (p < end && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r'
               && *p != '{' && *p != '}')
            p++;
        out.assign(start, p);
        return !out.empty();
    };

    while (p < end) {
        SkipWS();
        if (p >= end) break;
        if (*p == '{') {
            p++;
            Entity ent;
            while (true) {
                SkipWS();
                if (p >= end || *p == '}') { if (p < end) p++; break; }
                std::string key, val;
                if (!ReadToken(key)) break;
                if (!ReadToken(val)) break;
                ent.kv[key] = val;
            }
            entities.push_back(std::move(ent));
        } else {
            p++; // skip unexpected character
        }
    }

    return entities;
}

} // namespace OS
