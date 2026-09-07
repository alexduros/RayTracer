// *********************************************************
// Material Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2010 Tamy Boubekeur.
// All rights reserved.
// *********************************************************

#ifndef MATERIAL_H
#define MATERIAL_H

#include <iostream>
#include <vector>

#include "Vec3D.h"

// Ce modèle suppose une couleur spéculaire blanche (1.0, 1.0, 1.0)

class Material {
public:
    inline Material () : diffuse (0.8f), specular (0.2f), color (0.5f, 0.5f, 0.5f), shininess (32.f) {}
    /// `shininess` is the Blinn-Phong exponent (MTL Ns): higher = tighter highlight.
    inline Material (float diffuse, float specular, const Vec3Df & color, float shininess = 32.f)
            : diffuse (diffuse), specular (specular), color (color), shininess (shininess) {}
    virtual ~Material () {}

    inline float getDiffuse () const { return diffuse; }
    inline float getSpecular () const { return specular; }
    inline Vec3Df getColor () const { return color; }
    inline float getShininess () const { return shininess; }

    inline void setDiffuse (float d) { diffuse = d; }
    inline void setSpecular (float s) { specular = s; }
    inline void setColor (const Vec3Df & c) { color = c; }
    inline void setShininess (float s) { shininess = s; }

private:
    float diffuse;
    float specular;
    Vec3Df color;
    float shininess;
};


#endif // MATERIAL_H

// Some Emacs-Hints -- please don't remove:
//
//  Local Variables:
//  mode:C++
//  tab-width:4
//  End:
