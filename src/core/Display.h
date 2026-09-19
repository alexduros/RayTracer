// From linear radiance to display bytes: an exposure in stops, a tone curve
// that compresses light above 1 instead of clipping it, and an encoding for
// the screen. The tracer computes radiance (HdrImage); only this last step
// decides how it looks.
//
// References: E. Reinhard, M. Stark, P. Shirley & J. Ferwerda,
// "Photographic Tone Reproduction for Digital Images", SIGGRAPH 2002 (the
// curve L (1 + L / Lwhite^2) / (1 + L) on luminance, and the key 0.18 of the
// automatic exposure); K. Narkowicz, "ACES Filmic Tone Mapping Curve", 2015
// (a fit of the Academy's reference rendering transform); IEC 61966-2-1
// (sRGB).
#ifndef DISPLAY_H
#define DISPLAY_H

#include "HdrImage.h"
#include "Image.h"
#include "Vec3D.h"

struct Display {
    enum class ToneMap { NONE, REINHARD, ACES };
    enum class Encoding { LINEAR, SRGB, GAMMA };

    float exposure = 0.f;               // stops: radiance x 2^exposure (on top of the automatic one)
    bool autoExposure = false;          // apply () first meters the image (meteredExposure ())
    ToneMap toneMap = ToneMap::NONE;    // NONE clips at 1
    float whitePoint = 0.f;             // Reinhard's L_white after exposure; 0 = infinite
    Encoding encoding = Encoding::LINEAR;
    float gamma = 2.2f;                 // for Encoding::GAMMA

    /// Display-referred value in [0, 1] for a linear radiance. The default
    /// (LINEAR, no curve, no exposure) returns it unchanged: with toBytes it
    /// is the tracer's historical conversion, value x 255, rounded, clamped.
    Vec3Df map (const Vec3Df & linear) const;
    /// map() quantised to 8 bits.
    void toBytes (const Vec3Df & linear, unsigned char rgb[3]) const;
    /// A whole image through map(), after resolving the automatic exposure.
    Image apply (const HdrImage & hdr) const;
    /// This display with the automatic exposure measured on `hdr` and folded
    /// into `exposure`: what apply () uses, and what a caller reports.
    Display resolved (const HdrImage & hdr) const;

    /// The tracer's default and the encoding of every golden: linear, clipped
    /// at 1, no exposure.
    static Display linear () { return Display (); }
    /// What the CLI and the viewer show by default: exposure measured on the
    /// image, the ACES curve, sRGB.
    static Display filmic () {
        Display d;
        d.autoExposure = true;
        d.toneMap = ToneMap::ACES;
        d.encoding = Encoding::SRGB;
        return d;
    }

    /// The exposure, in stops, that brings the log-average luminance of the
    /// image to `key` (Reinhard et al.'s 0.18, mid grey). Black pixels (the
    /// background) are left out rather than offset; 0 if every pixel is black.
    static float meteredExposure (const HdrImage & hdr, float key = 0.18f);
};

#endif // DISPLAY_H
