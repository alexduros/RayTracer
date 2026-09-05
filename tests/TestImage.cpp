#include <cstring>

#include "Image.h"
#include "Test.h"

TEST_CASE("image: pixel access, layout and bounds") {
    Image img(4, 3, Image::RGB888);
    CHECK(img.isValid());
    CHECK_EQ(img.width(), 4);
    CHECK_EQ(img.height(), 3);
    CHECK_EQ(img.channels(), 3);
    CHECK_EQ(img.sizeInBytes(), 36u);

    img.setPixel(1, 2, 10, 20, 30);
    unsigned char r = 0, g = 0, b = 0;
    img.getPixel(1, 2, r, g, b);
    CHECK_EQ(int(r), 10);
    CHECK_EQ(int(g), 20);
    CHECK_EQ(int(b), 30);
    // Row-major from the top-left corner, which is what stb_image_write expects.
    CHECK_EQ(int(img.data()[(2 * 4 + 1) * 3 + 1]), 20);

    img.setPixel(7, 7, 1, 1, 1);   // out of range: ignored
    img.getPixel(7, 7, r, g, b);   // out of range: black
    CHECK_EQ(int(r) + int(g) + int(b), 0);

    img.fill(5, 6, 7);
    img.getPixel(3, 0, r, g, b);
    CHECK_EQ(int(r), 5);
    CHECK_EQ(int(b), 7);

    Image empty;
    CHECK(!empty.isValid());
}

TEST_CASE("image: PNG round trip") {
    Image img(9, 5, Image::RGB888);
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 9; ++x)
            img.setPixel(x, y, static_cast<unsigned char>(x * 20), static_cast<unsigned char>(y * 40),
                         static_cast<unsigned char>((x + y) * 10));

    const std::string path = test::outputDir() + "/roundtrip.png";
    REQUIRE(img.save(path));
    Image back;
    REQUIRE(back.load(path));
    CHECK_EQ(back.width(), 9);
    CHECK_EQ(back.height(), 5);
    CHECK_EQ(back.channels(), 3);
    CHECK_EQ(back.sizeInBytes(), img.sizeInBytes());
    CHECK(std::memcmp(back.data(), img.data(), img.sizeInBytes()) == 0);

    CHECK_MSG(!img.save(test::outputDir() + "/unsupported.bmp"), "unsupported extension is refused");
    CHECK_MSG(!back.load(test::outputDir() + "/missing.png"), "missing file is reported");
}
