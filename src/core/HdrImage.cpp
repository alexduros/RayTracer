#include "HdrImage.h"

#include <iostream>

#include "stb/stb_image.h"
#include "stb/stb_image_write.h"

void HdrImage::fill (const Vec3Df & c) {
    for (size_t i = 0; i < rgb.size (); i += 3) {
        rgb[i] = c[0];
        rgb[i + 1] = c[1];
        rgb[i + 2] = c[2];
    }
}

bool HdrImage::save (const std::string & filename) const {
    if (!isValid ()) {
        std::cerr << "Cannot save an empty HDR image" << std::endl;
        return false;
    }
    if (!stbi_write_hdr (filename.c_str (), w, h, 3, rgb.data ())) {
        std::cerr << "Failed to save HDR image: " << filename << std::endl;
        return false;
    }
    return true;
}

bool HdrImage::load (const std::string & filename) {
    int width = 0, height = 0, channels = 0;
    float * data = stbi_loadf (filename.c_str (), &width, &height, &channels, 3);
    if (!data) {
        std::cerr << "Failed to load HDR image: " << filename << std::endl;
        return false;
    }
    w = width;
    h = height;
    rgb.assign (data, data + static_cast<size_t> (width) * height * 3);
    stbi_image_free (data);
    return true;
}
