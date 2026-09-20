#include <array>
#include <cmath>
#include <memory>
#include <cstring>
#include <string>
#include <vector>

#include "Camera.h"
#include "Fixtures.h"
#include "Image.h"
#include "Light.h"
#include "Material.h"
#include "ObjLoader.h"
#include "RayTracer.h"
#include "Scene.h"
#include "Test.h"
#include "Texture.h"

namespace {

const float kPi = 3.14159265358979f;

/// Write an image and read it back as a texture, the way a material does.
std::shared_ptr<const Texture> textureOf(const std::string& name, int w, int h,
                                         const std::vector<std::array<unsigned char, 3>>& texels) {
    Image image(w, h, Image::RGB888);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const auto& t = texels[static_cast<size_t>(y) * w + x];
            image.setPixel(x, y, t[0], t[1], t[2]);
        }
    const std::string path = test::outputDir() + "/" + name;
    REQUIRE(image.save(path));
    return Texture::load(path);
}

/// The four quadrants, in image order: red, green / blue, white.
std::shared_ptr<const Texture> quadrants() {
    return textureOf("quadrants.png", 2, 2,
                     {{{255, 0, 0}}, {{0, 255, 0}}, {{0, 0, 255}}, {{255, 255, 255}}});
}

float srgbToLinear(float s) { return s <= 0.04045f ? s / 12.92f : std::pow((s + 0.055f) / 1.055f, 2.4f); }

}  // namespace

TEST_CASE("texture: each texel owns its quadrant, and coordinates outside [0, 1] wrap") {
    const auto tex = quadrants();
    REQUIRE(tex != nullptr);
    CHECK_EQ(tex->width(), 2);
    CHECK_EQ(tex->height(), 2);

    // v grows upward, as OBJ means it: the image's top row is v near 1.
    const Vec3Df topLeft = tex->sample(0.25f, 0.75f), topRight = tex->sample(0.75f, 0.75f);
    const Vec3Df bottomLeft = tex->sample(0.25f, 0.25f), bottomRight = tex->sample(0.75f, 0.25f);
    CHECK(topLeft == Vec3Df(1.f, 0.f, 0.f));
    CHECK(topRight == Vec3Df(0.f, 1.f, 0.f));
    CHECK(bottomLeft == Vec3Df(0.f, 0.f, 1.f));
    CHECK(bottomRight == Vec3Df(1.f, 1.f, 1.f));

    // Wrapping repeats the same quadrants a tile further, in both directions.
    for (float shift : {-2.f, -1.f, 1.f, 3.f}) {
        CHECK(tex->sample(0.25f + shift, 0.75f) == topLeft);
        CHECK(tex->sample(0.25f, 0.75f + shift) == topLeft);
    }
}

TEST_CASE("texture: between two texels the read is their average, and it decodes sRGB") {
    // A row of black and white: halfway between the centres, the bilinear
    // read is the mean of the two, in linear light.
    const auto tex = textureOf("ramp.png", 2, 1, {{{0, 0, 0}}, {{255, 255, 255}}});
    REQUIRE(tex != nullptr);
    CHECK_CLOSE(tex->sample(0.25f, 0.5f)[0], 0.f, 1e-6);
    CHECK_CLOSE(tex->sample(0.75f, 0.5f)[0], 1.f, 1e-6);
    CHECK_CLOSE(tex->sample(0.5f, 0.5f)[0], 0.5f, 1e-5);
    CHECK_CLOSE(tex->sample(0.375f, 0.5f)[0], 0.25f, 1e-5);

    // 188 is what the display writes for a linear 0.5: reading it must give
    // 0.5 back, not 0.737.
    const auto grey = textureOf("grey.png", 1, 1, {{{188, 188, 188}}});
    REQUIRE(grey != nullptr);
    CHECK_CLOSE(grey->sample(0.5f, 0.5f)[0], 0.5f, 4e-3);  // 188 is 0.5 rounded to a byte
    for (unsigned char c : {0, 32, 128, 200, 255}) {
        const auto one = textureOf("one.png", 1, 1, {{{c, c, c}}});
        CHECK_CLOSE(one->sample(0.f, 0.f)[0], srgbToLinear(static_cast<float>(c) / 255.f), 1e-6);
    }

    // A file that is not there is a warning, not a crash.
    CHECK(Texture::load(test::outputDir() + "/no-such-texture.png") == nullptr);
}

TEST_CASE("texture: the material tints the picture, and no texture leaves the colour alone") {
    Material m(1.f, 0.f, Vec3Df(0.5f, 1.f, 1.f));
    CHECK(m.getColorAt(0.3f, 0.7f) == Vec3Df(0.5f, 1.f, 1.f));  // no map: the colour, wherever you ask
    m.setDiffuseMap(quadrants());
    CHECK(m.getColorAt(0.75f, 0.75f) == Vec3Df(0.f, 1.f, 0.f));          // green texel, halved red channel
    CHECK(m.getColorAt(0.75f, 0.25f) == Vec3Df(0.5f, 1.f, 1.f));         // white texel x the colour
}

TEST_CASE("texture: the coordinates are interpolated over the triangle, like the normal") {
    // A quad with the texture spread over it: the hit at a known point must
    // carry the coordinates that point deserves.
    const Vec3Df n(0.f, 0.f, 1.f);
    std::vector<Vertex> v = {Vertex(Vec3Df(-1.f, -1.f, 0.f), n, 0.f, 0.f), Vertex(Vec3Df(1.f, -1.f, 0.f), n, 1.f, 0.f),
                             Vertex(Vec3Df(1.f, 1.f, 0.f), n, 1.f, 1.f), Vertex(Vec3Df(-1.f, 1.f, 0.f), n, 0.f, 1.f)};
    const Mesh mesh(v, {Triangle(0, 1, 2), Triangle(0, 2, 3)});
    Scene s = fixtures::sceneOf(mesh);
    RayTracer rt;
    RayTracer::Hit hit;
    for (float x : {-0.9f, -0.5f, 0.f, 0.5f, 0.9f})
        for (float y : {-0.9f, 0.f, 0.6f}) {
            REQUIRE(rt.closestHit(s, Ray(Vec3Df(x, y, 5.f), Vec3Df(0.f, 0.f, -1.f)), hit));
            const std::string at = "(" + std::to_string(x) + ", " + std::to_string(y) + ")";
            CHECK_MSG(std::fabs(hit.vertex.getU() - (x + 1.f) / 2.f) < 1e-5f, "u at " + at);
            CHECK_MSG(std::fabs(hit.vertex.getV() - (y + 1.f) / 2.f) < 1e-5f, "v at " + at);
        }

    // The uv mode paints them: u to red, v to green, nothing in blue.
    rt.setDebugMode(RayTracer::DebugMode::UV);
    const Camera cam = Camera::lookAt(Vec3Df(0.f, 0.f, 4.f), Vec3Df(0.f, 0.f, 0.f), Vec3Df(0.f, 1.f, 0.f), kPi / 4.f, 1.f);
    const Image uv = rt.render(s, cam, 32, 32);
    unsigned char r = 0, g = 0, b = 0;
    uv.getPixel(24, 8, r, g, b);  // right of centre, above it: u high, v high
    CHECK(r > 160 && g > 160 && b == 0);
    uv.getPixel(8, 24, r, g, b);  // left and low: both small
    CHECK(r < 96 && g < 96 && b == 0);
}

TEST_CASE("texture: a textured quad shows each quadrant where it belongs") {
    // Straight-on render of the textured quad: the four corners of the image
    // hold the four texels, tinted by nothing and lit head-on.
    const Vec3Df n(0.f, 0.f, 1.f);
    std::vector<Vertex> v = {Vertex(Vec3Df(-1.f, -1.f, 0.f), n, 0.f, 0.f), Vertex(Vec3Df(1.f, -1.f, 0.f), n, 1.f, 0.f),
                             Vertex(Vec3Df(1.f, 1.f, 0.f), n, 1.f, 1.f), Vertex(Vec3Df(-1.f, 1.f, 0.f), n, 0.f, 1.f)};
    Material material(1.f, 0.f, Vec3Df(1.f, 1.f, 1.f));
    material.setDiffuseMap(quadrants());
    Scene s;
    s.addObject(Object(Mesh(v, {Triangle(0, 1, 2), Triangle(0, 2, 3)}), material));
    RayTracer rt;
    rt.setDebugMode(RayTracer::DebugMode::AMBIENT);  // the albedo alone, no lighting
    const Camera cam = Camera::lookAt(Vec3Df(0.f, 0.f, 3.f), Vec3Df(0.f, 0.f, 0.f), Vec3Df(0.f, 1.f, 0.f), kPi / 4.f, 1.f);
    const Image img = rt.render(s, cam, 64, 64);

    // Bilinear blends everywhere but at the texel centres, so each quadrant
    // is named by the channel that dominates it, not by a pure colour.
    struct Corner {
        int x, y;
        int channel;  // the one that must win
        const char* what;
    };
    for (const Corner& c : {Corner{20, 20, 0, "top left leans red"}, Corner{44, 20, 1, "top right leans green"},
                            Corner{20, 44, 2, "bottom left leans blue"}}) {
        unsigned char rgb[3] = {0, 0, 0};
        img.getPixel(c.x, c.y, rgb[0], rgb[1], rgb[2]);
        const int other1 = (c.channel + 1) % 3, other2 = (c.channel + 2) % 3;
        CHECK_MSG(rgb[c.channel] > 200 && rgb[other1] < 100 && rgb[other2] < 100,
                  std::string(c.what) + ": got " + std::to_string(rgb[0]) + "," + std::to_string(rgb[1]) + "," +
                      std::to_string(rgb[2]));
    }
    unsigned char wr = 0, wg = 0, wb = 0;
    img.getPixel(44, 44, wr, wg, wb);
    CHECK_MSG(wr > 200 && wg > 200 && wb > 200, "bottom right is the white texel");
    // Dead centre, the four texels weigh the same: their mean, in linear light.
    unsigned char cr = 0, cg = 0, cb = 0;
    img.getPixel(32, 32, cr, cg, cb);
    for (unsigned char channel : {cr, cg, cb})
        CHECK_MSG(std::abs(static_cast<int>(channel) - 128) <= 4,
                  "the centre averages the four texels: " + std::to_string(channel));

    // Without the map, the same render is the material's flat colour.
    s.getObjects()[0].getMaterial().setDiffuseMap(nullptr);
    const Image flat = rt.render(s, cam, 64, 64);
    unsigned char r = 0, g = 0, b = 0;
    flat.getPixel(20, 20, r, g, b);
    CHECK(r == 255 && g == 255 && b == 255);
}

TEST_CASE("obj: texture coordinates survive, and a seam keeps its two vertices") {
    // Two triangles sharing an edge, but not its texture coordinates: the
    // corners must not be merged, or the seam would smear.
    const std::string path = fixtures::writeFile("seam.obj",
        "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
        "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nvt 0.25 0.5\n"
        "vn 0 0 1\n"
        "f 1/1/1 2/2/1 3/3/1\n"
        "f 1/5/1 3/3/1 4/4/1\n");
    const std::vector<Object> objects = loadOBJ(path, fixtures::white());
    REQUIRE(objects.size() == 1);
    const Mesh& mesh = objects[0].getMesh();
    CHECK_EQ(mesh.getTriangles().size(), 2u);
    CHECK_MSG(mesh.getVertices().size() == 5u, "position 1 carries two coordinates, so it is two vertices");
    // Every vertex kept the coordinate its corner asked for.
    bool seamFound = false;
    for (const Vertex& vertex : mesh.getVertices())
        if (std::fabs(vertex.getU() - 0.25f) < 1e-6f && std::fabs(vertex.getV() - 0.5f) < 1e-6f) seamFound = true;
    CHECK(seamFound);

    // Spot, the model this was built for, carries hers.
    Scene spot;
    spot.addObjectsFromFile(test::modelPath("spot.obj"));
    float maxU = 0.f, maxV = 0.f, minU = 1.f, minV = 1.f;
    for (const Vertex& vertex : spot.getObjects()[0].getMesh().getVertices()) {
        maxU = std::max(maxU, vertex.getU());
        maxV = std::max(maxV, vertex.getV());
        minU = std::min(minU, vertex.getU());
        minV = std::min(minV, vertex.getV());
    }
    CHECK_MSG(maxU > 0.9f && maxV > 0.9f, "Spot's unwrapping fills the unit square");
    // A little outside is fine, the texture wraps; far outside would mean the
    // coordinates were read wrong.
    CHECK_MSG(minU > -0.5f && minV > -0.5f && maxU < 1.5f && maxV < 1.5f,
              "u in [" + std::to_string(minU) + ", " + std::to_string(maxU) + "], v in [" + std::to_string(minV) +
                  ", " + std::to_string(maxV) + "]");
}

TEST_CASE("obj: MTL map_Kd loads the picture beside the material file") {
    const auto tex = quadrants();  // writes quadrants.png into the test output directory
    REQUIRE(tex != nullptr);
    fixtures::writeFile("textured.mtl", "newmtl painted\nKd 1 1 1\nmap_Kd quadrants.png\n");
    fixtures::writeFile("nomap.mtl", "newmtl painted\nKd 1 0 0\nmap_Kd nowhere.png\n");
    const std::string obj = fixtures::writeFile("textured.obj",
        "mtllib textured.mtl\nusemtl painted\n"
        "v 0 0 0\nv 1 0 0\nv 1 1 0\nvt 0 0\nvt 1 0\nvt 1 1\nvn 0 0 1\n"
        "f 1/1/1 2/2/1 3/3/1\n");
    const std::vector<Object> objects = loadOBJ(obj, fixtures::white());
    REQUIRE(objects.size() == 1);
    const Material& m = objects[0].getMaterial();
    REQUIRE(m.getDiffuseMap() != nullptr);
    CHECK(m.getColorAt(0.75f, 0.75f) == Vec3Df(0.f, 1.f, 0.f));

    // A map_Kd that points nowhere leaves the material its colour.
    const std::string obj2 = fixtures::writeFile("nomap.obj",
        "mtllib nomap.mtl\nusemtl painted\n"
        "v 0 0 0\nv 1 0 0\nv 1 1 0\nvn 0 0 1\nf 1//1 2//1 3//1\n");
    const std::vector<Object> objects2 = loadOBJ(obj2, fixtures::white());
    REQUIRE(objects2.size() == 1);
    CHECK(objects2[0].getMaterial().getDiffuseMap() == nullptr);
    CHECK(objects2[0].getMaterial().getColor() == Vec3Df(1.f, 0.f, 0.f));
}
