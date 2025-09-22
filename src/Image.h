#pragma once

#include <vector>
#include <string>
#include <memory>

/**
 * Modern Image class to replace QImage dependency
 * Supports RGB/RGBA formats with STB integration
 */
class Image {
public:
    enum Format {
        RGB888,   // 3 channels, 8 bits per channel
        RGBA8888  // 4 channels, 8 bits per channel
    };

    // Constructors
    Image();
    Image(int width, int height, Format format = RGB888);
    Image(const Image& other);
    Image(Image&& other) noexcept;

    // Assignment operators
    Image& operator=(const Image& other);
    Image& operator=(Image&& other) noexcept;

    // Destructor
    ~Image() = default;

    // Basic properties
    int width() const { return m_width; }
    int height() const { return m_height; }
    Format format() const { return m_format; }
    int channels() const { return m_format == RGB888 ? 3 : 4; }
    size_t sizeInBytes() const { return m_data.size(); }

    // Data access
    unsigned char* data() { return m_data.data(); }
    const unsigned char* data() const { return m_data.data(); }

    // Pixel access (RGB only for now)
    void setPixel(int x, int y, unsigned char r, unsigned char g, unsigned char b);
    void getPixel(int x, int y, unsigned char& r, unsigned char& g, unsigned char& b) const;

    // File I/O
    bool save(const std::string& filename) const;
    bool load(const std::string& filename);

    // Utility
    void fill(unsigned char r, unsigned char g, unsigned char b);
    void clear();
    bool isValid() const { return m_width > 0 && m_height > 0 && !m_data.empty(); }

private:
    int m_width;
    int m_height;
    Format m_format;
    std::vector<unsigned char> m_data;

    void allocateData();
    size_t pixelIndex(int x, int y) const;
};