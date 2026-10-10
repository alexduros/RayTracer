// *********************************************************
// Material Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2010 Tamy Boubekeur.
// All rights reserved.
// *********************************************************

#ifndef MATERIAL_H
#define MATERIAL_H

#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "Texture.h"
#include "Vec3D.h"

// Ce modèle suppose une couleur spéculaire blanche (1.0, 1.0, 1.0)

class Material {
public:
    inline Material () : diffuse (0.8f), specular (0.2f), color (0.5f, 0.5f, 0.5f), shininess (32.f) {}
    /// `shininess` is the Blinn-Phong exponent (MTL Ns): higher = tighter highlight.
    /// `reflectivity` in [0, 1] is the share of the colour that comes from a
    /// mirror ray (RayTracer::setMaxDepth): 0 = matte, 1 = perfect mirror.
    /// `transparency` in [0, 1] is the share that behaves as clear glass of
    /// index `ior` (MTL d and Ni): reflected and refracted by the Fresnel
    /// equations. 0 = opaque (the default).
    inline Material (float diffuse, float specular, const Vec3Df & color, float shininess = 32.f,
                     float reflectivity = 0.f)
            : diffuse (diffuse), specular (specular), color (color), shininess (shininess) {
        setReflectivity (reflectivity);
    }
    virtual ~Material () {}

    inline float getDiffuse () const { return diffuse; }
    inline float getSpecular () const { return specular; }
    inline Vec3Df getColor () const { return color; }
    /// The colour at a point of the surface: the texture (MTL map_Kd) read
    /// through its coordinates and tinted by `color`, or `color` alone.
    inline Vec3Df getColorAt (float u, float v) const {
        return diffuseMap ? color * diffuseMap->sample (u, v) : color;
    }
    inline const std::shared_ptr<const Texture> & getDiffuseMap () const { return diffuseMap; }
    inline void setDiffuseMap (const std::shared_ptr<const Texture> & map) { diffuseMap = map; }
    /// The height map that wrinkles the surface (MTL map_bump), read through
    /// the same coordinates: white stands `bumpScale` world units above
    /// black. Only the shading normal follows it (Bump.h); none by default.
    inline const std::shared_ptr<const Texture> & getBumpMap () const { return bumpMap; }
    inline void setBumpMap (const std::shared_ptr<const Texture> & map) { bumpMap = map; }
    inline float getBumpScale () const { return bumpScale; }
    inline void setBumpScale (float s) { bumpScale = s; }
    inline float getShininess () const { return shininess; }
    inline float getReflectivity () const { return reflectivity; }
    inline float getTransparency () const { return transparency; }
    inline float getIor () const { return ior; }
    /// What the inside of the glass absorbs, per channel and per world unit
    /// crossed: light that travels d inside keeps exp(-absorption x d) of
    /// itself (Beer-Lambert, optics::transmittance). 0 = clear, the default;
    /// a thick part is darker and more saturated than a thin one.
    inline const Vec3Df & getAbsorption () const { return absorption; }
    inline bool absorbs () const { return absorption[0] > 0.f || absorption[1] > 0.f || absorption[2] > 0.f; }
    /// The absorption that turns white light into `tint` after `distance`
    /// inside: -ln(tint) / distance per channel. A channel at 0 would take an
    /// infinite absorption and is held at 1e-4; one at 1 or more absorbs
    /// nothing.
    static inline Vec3Df absorptionFor (const Vec3Df & tint, float distance) {
        Vec3Df sigma;
        for (int c = 0; c < 3; ++c) {
            const float kept = tint[c] < 1e-4f ? 1e-4f : (tint[c] > 1.f ? 1.f : tint[c]);
            sigma[c] = distance > 0.f ? -std::log (kept) / distance : 0.f;
        }
        return sigma;
    }

    inline void setDiffuse (float d) { diffuse = d; }
    inline void setSpecular (float s) { specular = s; }
    inline void setColor (const Vec3Df & c) { color = c; }
    inline void setShininess (float s) { shininess = s; }
    inline void setReflectivity (float r) { reflectivity = r < 0.f ? 0.f : (r > 1.f ? 1.f : r); }
    inline void setTransparency (float t) { transparency = t < 0.f ? 0.f : (t > 1.f ? 1.f : t); }
    /// Index of refraction relative to the air around: 1 = no bending
    /// (clamped there), 1.33 water, 1.5 glass, 2.4 diamond.
    inline void setIor (float n) { ior = n < 1.f ? 1.f : n; }
    /// Negative channels are held at 0: glass does not amplify.
    inline void setAbsorption (const Vec3Df & a) {
        for (int c = 0; c < 3; ++c)
            absorption[c] = a[c] > 0.f ? a[c] : 0.f;
    }

private:
    float diffuse;
    float specular;
    Vec3Df color;
    float shininess;
    std::shared_ptr<const Texture> diffuseMap;  // MTL map_Kd, shared and never modified
    std::shared_ptr<const Texture> bumpMap;     // MTL map_bump, read as data (Texture::readData)
    float bumpScale = 1.f;                      // MTL -bm
    float reflectivity = 0.f;
    float transparency = 0.f;
    float ior = 1.5f;
    Vec3Df absorption = Vec3Df (0.f, 0.f, 0.f);  // per world unit inside; MTL Tf
};


#endif // MATERIAL_H

// Some Emacs-Hints -- please don't remove:
//
//  Local Variables:
//  mode:C++
//  tab-width:4
//  End:
