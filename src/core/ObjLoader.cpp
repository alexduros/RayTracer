// Wavefront OBJ + MTL loader. See ObjLoader.h for how a file maps onto objects.
#include "ObjLoader.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {

std::string trim (const std::string & s) {
    const std::string ws = " \t\r\n";
    const size_t b = s.find_first_not_of (ws);
    if (b == std::string::npos)
        return "";
    const size_t e = s.find_last_not_of (ws);
    return s.substr (b, e - b + 1);
}

/// 1-based OBJ indices; 0 means "absent".
struct Corner {
    int v = 0, vt = 0, vn = 0;
};

int resolveIndex (int idx, size_t count, const char * what, const std::string & where) {
    if (idx < 0)
        idx = static_cast<int> (count) + idx + 1;  // negative = relative to the latest element
    if (idx < 1 || static_cast<size_t> (idx) > count)
        throw std::runtime_error ("loadOBJ: " + std::string (what) + " index out of range at " + where);
    return idx;
}

/// Parses "v", "v/vt", "v//vn" or "v/vt/vn".
Corner parseCorner (const std::string & token, size_t nv, size_t nvt, size_t nvn, const std::string & where) {
    int fields[3] = {0, 0, 0};
    size_t start = 0;
    for (int f = 0; f < 3; ++f) {
        const size_t slash = token.find ('/', start);
        const std::string part = token.substr (start, slash == std::string::npos ? std::string::npos : slash - start);
        if (!part.empty ()) {
            try {
                fields[f] = std::stoi (part);
            } catch (const std::exception &) {
                throw std::runtime_error ("loadOBJ: bad face token '" + token + "' at " + where);
            }
        }
        if (slash == std::string::npos)
            break;
        start = slash + 1;
    }
    Corner c;
    c.v = resolveIndex (fields[0], nv, "vertex", where);
    c.vt = fields[1] != 0 ? resolveIndex (fields[1], nvt, "texture coordinate", where) : 0;
    c.vn = fields[2] != 0 ? resolveIndex (fields[2], nvn, "normal", where) : 0;
    return c;
}

/// Faces sharing one material; vertices are de-duplicated on (position, normal).
struct Group {
    std::string material;
    std::vector<Vertex> vertices;
    std::vector<Triangle> triangles;
    std::map<std::pair<int, int>, unsigned int> lookup;
    bool missingNormals = false;

    unsigned int vertexFor (const Corner & c, const std::vector<Vec3Df> & positions,
                            const std::vector<Vec3Df> & normals) {
        const auto key = std::make_pair (c.v, c.vn);
        const auto it = lookup.find (key);
        if (it != lookup.end ())
            return it->second;
        Vec3Df n (0.f, 0.f, 1.f);
        if (c.vn > 0) {
            n = normals[c.vn - 1];
            n.normalize ();
        } else {
            missingNormals = true;
        }
        vertices.push_back (Vertex (positions[c.v - 1], n));
        const unsigned int index = static_cast<unsigned int> (vertices.size () - 1);
        lookup[key] = index;
        return index;
    }
};

} // namespace

std::map<std::string, Material> loadMTL (const std::string & filename) {
    std::ifstream in (filename);
    if (!in)
        throw std::runtime_error ("loadMTL: cannot open " + filename);
    std::map<std::string, Material> materials;
    std::string current;
    std::string line;
    while (std::getline (in, line)) {
        std::istringstream ss (line);
        std::string key;
        if (!(ss >> key) || key[0] == '#')
            continue;
        if (key == "newmtl") {
            std::string rest;
            std::getline (ss, rest);
            current = trim (rest);
            materials[current] = Material (1.f, 0.f, Vec3Df (0.8f, 0.8f, 0.8f));
        } else if (current.empty ()) {
            continue;  // statements before the first newmtl
        } else if (key == "Kd") {
            Vec3Df c;
            if (ss >> c)
                materials[current].setColor (c);
        } else if (key == "Ks") {
            Vec3Df s;
            if (ss >> s)
                materials[current].setSpecular ((s[0] + s[1] + s[2]) / 3.f);
        } else if (key == "Ns") {
            float ns = 0.f;
            if (ss >> ns)
                materials[current].setShininess (std::max (1.f, ns));  // 0 would flatten the highlight to white
        }
        // Ka, d, Tr, illum, map_*: not represented by Material yet.
    }
    return materials;
}

std::vector<Object> loadOBJ (const std::string & filename, const Material & fallback) {
    std::ifstream in (filename);
    if (!in)
        throw std::runtime_error ("loadOBJ: cannot open " + filename);

    std::vector<Vec3Df> positions, normals;
    size_t texcoords = 0;  // counted for index validation only; not stored
    std::vector<std::string> mtlFiles;
    std::vector<Group> groups;
    std::map<std::string, size_t> groupIndex;
    std::string currentMaterial;

    auto groupFor = [&] (const std::string & material) -> Group & {
        const auto it = groupIndex.find (material);
        if (it != groupIndex.end ())
            return groups[it->second];
        groupIndex[material] = groups.size ();
        groups.push_back (Group ());
        groups.back ().material = material;
        return groups.back ();
    };

    std::string line;
    size_t lineNo = 0;
    while (std::getline (in, line)) {
        ++lineNo;
        std::istringstream ss (line);
        std::string key;
        if (!(ss >> key) || key[0] == '#')
            continue;
        const std::string where = filename + ":" + std::to_string (lineNo);
        if (key == "v") {
            Vec3Df p;
            if (!(ss >> p))
                throw std::runtime_error ("loadOBJ: malformed vertex at " + where);
            positions.push_back (p);
        } else if (key == "vn") {
            Vec3Df n;
            if (!(ss >> n))
                throw std::runtime_error ("loadOBJ: malformed normal at " + where);
            normals.push_back (n);
        } else if (key == "vt") {
            ++texcoords;
        } else if (key == "mtllib") {
            std::string f;
            while (ss >> f)
                mtlFiles.push_back (f);
        } else if (key == "usemtl") {
            std::string rest;
            std::getline (ss, rest);
            currentMaterial = trim (rest);
        } else if (key == "f") {
            std::vector<Corner> corners;
            std::string token;
            while (ss >> token)
                corners.push_back (parseCorner (token, positions.size (), texcoords, normals.size (), where));
            if (corners.size () < 3)
                throw std::runtime_error ("loadOBJ: face with fewer than 3 vertices at " + where);
            Group & g = groupFor (currentMaterial);
            const unsigned int first = g.vertexFor (corners[0], positions, normals);
            for (size_t i = 1; i + 1 < corners.size (); ++i) {  // fan triangulation
                const unsigned int b = g.vertexFor (corners[i], positions, normals);
                const unsigned int c = g.vertexFor (corners[i + 1], positions, normals);
                g.triangles.push_back (Triangle (first, b, c));
            }
        }
        // o, g, s, l, p and anything else: ignored.
    }
    if (groups.empty ())
        throw std::runtime_error ("loadOBJ: no faces in " + filename);

    // Materials from every mtllib, relative to the OBJ's directory. A missing
    // file is a warning, not an error: the geometry still loads with `fallback`.
    std::map<std::string, Material> materials;
    const std::filesystem::path dir = std::filesystem::path (filename).parent_path ();
    for (const std::string & f : mtlFiles) {
        const std::filesystem::path p = std::filesystem::path (f).is_absolute () ? std::filesystem::path (f) : dir / f;
        try {
            for (const auto & kv : loadMTL (p.string ()))
                materials[kv.first] = kv.second;
        } catch (const std::exception & e) {
            std::cerr << "warning: " << e.what () << " (using the default material)" << std::endl;
        }
    }

    std::vector<Object> objects;
    objects.reserve (groups.size ());
    for (Group & g : groups) {
        Mesh mesh (g.vertices, g.triangles);
        if (g.missingNormals)
            mesh.recomputeSmoothVertexNormals (0);
        Material material = fallback;
        const auto it = materials.find (g.material);
        if (it != materials.end ())
            material = it->second;
        else if (!g.material.empty ())
            std::cerr << "warning: material '" << g.material << "' not found for " << filename
                      << " (using the default material)" << std::endl;
        objects.push_back (Object (mesh, material));
    }
    return objects;
}
