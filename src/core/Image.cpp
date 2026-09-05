#include "Image.h"
#include <iostream>
#include <algorithm>
#include <cstring>

// STB image implementation
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"

Image::Image() : m_width(0), m_height(0), m_format(RGB888) {
}

Image::Image(int width, int height, Format format)
    : m_width(width), m_height(height), m_format(format) {
    allocateData();
}

Image::Image(const Image& other)
    : m_width(other.m_width), m_height(other.m_height), m_format(other.m_format), m_data(other.m_data) {
}

Image::Image(Image&& other) noexcept
    : m_width(other.m_width), m_height(other.m_height), m_format(other.m_format), m_data(std::move(other.m_data)) {
    other.m_width = 0;
    other.m_height = 0;
}

Image& Image::operator=(const Image& other) {
    if (this != &other) {
        m_width = other.m_width;
        m_height = other.m_height;
        m_format = other.m_format;
        m_data = other.m_data;
    }
    return *this;
}

Image& Image::operator=(Image&& other) noexcept {
    if (this != &other) {
        m_width = other.m_width;
        m_height = other.m_height;
        m_format = other.m_format;
        m_data = std::move(other.m_data);
        other.m_width = 0;
        other.m_height = 0;
    }
    return *this;
}

void Image::allocateData() {
    size_t size = m_width * m_height * channels();
    m_data.resize(size);
}

size_t Image::pixelIndex(int x, int y) const {
    return (y * m_width + x) * channels();
}

void Image::setPixel(int x, int y, unsigned char r, unsigned char g, unsigned char b) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) return;

    size_t idx = pixelIndex(x, y);
    m_data[idx] = r;
    m_data[idx + 1] = g;
    m_data[idx + 2] = b;

    if (m_format == RGBA8888) {
        m_data[idx + 3] = 255; // Full alpha
    }
}

void Image::getPixel(int x, int y, unsigned char& r, unsigned char& g, unsigned char& b) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        r = g = b = 0;
        return;
    }

    size_t idx = pixelIndex(x, y);
    r = m_data[idx];
    g = m_data[idx + 1];
    b = m_data[idx + 2];
}

void Image::fill(unsigned char r, unsigned char g, unsigned char b) {
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            setPixel(x, y, r, g, b);
        }
    }
}

void Image::clear() {
    std::fill(m_data.begin(), m_data.end(), 0);
}

bool Image::save(const std::string& filename) const {
    if (!isValid()) {
        std::cerr << "Cannot save invalid image" << std::endl;
        return false;
    }

    // Determine format from extension
    std::string ext = filename.substr(filename.find_last_of('.'));
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    int result = 0;
    int comp = channels();

    if (ext == ".png") {
        result = stbi_write_png(filename.c_str(), m_width, m_height, comp, m_data.data(), m_width * comp);
    } else if (ext == ".jpg" || ext == ".jpeg") {
        result = stbi_write_jpg(filename.c_str(), m_width, m_height, comp, m_data.data(), 90);
    } else if (ext == ".tga") {
        result = stbi_write_tga(filename.c_str(), m_width, m_height, comp, m_data.data());
    } else {
        std::cerr << "Unsupported image format: " << ext << std::endl;
        return false;
    }

    if (!result) {
        std::cerr << "Failed to save image: " << filename << std::endl;
        return false;
    }

    return true;
}

bool Image::load(const std::string& filename) {
    int width, height, channels;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &channels, 0);

    if (!data) {
        std::cerr << "Failed to load image: " << filename << std::endl;
        return false;
    }

    m_width = width;
    m_height = height;
    m_format = (channels == 4) ? RGBA8888 : RGB888;

    size_t dataSize = width * height * channels;
    m_data.resize(dataSize);
    std::memcpy(m_data.data(), data, dataSize);

    stbi_image_free(data);
    return true;
}