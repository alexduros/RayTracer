#include "Orientation.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

const char * upAxisName (UpAxis up) {
    switch (up) {
        case UpAxis::PosX: return "+X";
        case UpAxis::NegX: return "-X";
        case UpAxis::PosY: return "+Y";
        case UpAxis::NegY: return "-Y";
        case UpAxis::PosZ: return "+Z";
        case UpAxis::NegZ: return "-Z";
    }
    return "+Y";
}

std::optional<UpAxis> parseUpAxis (const std::string & s) {
    std::string t;
    for (char c : s)
        if (!std::isspace (static_cast<unsigned char> (c)))
            t += static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
    if (t.empty ())
        return std::nullopt;
    bool negative = false;
    if (t[0] == '+' || t[0] == '-') {
        negative = t[0] == '-';
        t = t.substr (1);
    }
    if (t == "x") return negative ? UpAxis::NegX : UpAxis::PosX;
    if (t == "y") return negative ? UpAxis::NegY : UpAxis::PosY;
    if (t == "z") return negative ? UpAxis::NegZ : UpAxis::PosZ;
    return std::nullopt;
}

UpAxis detectUpAxis (const Scene & scene) {
    const BoundingBox & box = scene.getBoundingBox ();  // model only, backdrops excluded
    unsigned int onFace[3][2] = {{0, 0}, {0, 0}, {0, 0}};  // [axis][0 = min face, 1 = max face]
    float eps[3];
    for (int a = 0; a < 3; ++a)
        eps[a] = 0.01f * std::max (box.getMax ()[a] - box.getMin ()[a], 1e-6f);
    for (const Object & o : scene.getObjects ()) {
        if (o.isBackdrop ())
            continue;
        for (const Vertex & v : o.getMesh ().getVertices ()) {
            for (int a = 0; a < 3; ++a) {
                if (v.getPos ()[a] - box.getMin ()[a] <= eps[a]) onFace[a][0]++;
                if (box.getMax ()[a] - v.getPos ()[a] <= eps[a]) onFace[a][1]++;
            }
        }
    }
    // Candidate bottoms in tie-break order; up is the opposite direction.
    struct Candidate { int axis; int face; UpAxis up; };
    const Candidate candidates[] = {
        {1, 0, UpAxis::PosY}, {2, 0, UpAxis::PosZ}, {1, 1, UpAxis::NegY},
        {2, 1, UpAxis::NegZ}, {0, 0, UpAxis::PosX}, {0, 1, UpAxis::NegX},
    };
    UpAxis best = UpAxis::PosY;
    unsigned int bestCount = 0;
    bool first = true;
    for (const Candidate & c : candidates) {
        const unsigned int count = onFace[c.axis][c.face];
        if (first || count > bestCount) {
            best = c.up;
            bestCount = count;
            first = false;
        }
    }
    return best;
}

std::optional<UpAxis> upAxisFromManifest (const std::string & modelPath) {
    namespace fs = std::filesystem;
    const fs::path model (modelPath);
    const fs::path manifest = model.parent_path () / "orientation.txt";
    std::ifstream in (manifest);
    if (!in)
        return std::nullopt;
    const std::string wanted = model.stem ().string ();
    std::string line;
    while (std::getline (in, line)) {
        const size_t hash = line.find ('#');
        if (hash != std::string::npos)
            line = line.substr (0, hash);
        std::istringstream ss (line);
        std::string name, axis;
        if (!(ss >> name >> axis))
            continue;
        if (name == wanted)
            return parseUpAxis (axis);
    }
    return std::nullopt;
}

UpAxis resolveUpAxis (const std::string & modelPath, const Scene & scene, std::string * source) {
    if (const std::optional<UpAxis> curated = upAxisFromManifest (modelPath)) {
        if (source) *source = "orientation.txt";
        return *curated;
    }
    if (source) *source = "heuristic";
    return detectUpAxis (scene);
}
