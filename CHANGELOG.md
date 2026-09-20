# Changelog

Every release is one step of `claudedocs/RAY_TRACING_TIMELINE.md`: a landmark
ray tracing paper turned into code, with the test that proves it. Format
follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions
follow [semantic versioning](https://semver.org/spec/v2.0.0.html), where a
new rendering feature bumps the minor.

The release workflow reads the section of the version it is tagging and
publishes it as the release notes, with the binaries for macOS and Linux.

## [Unreleased]

## [0.1.0] - 2026-09-20

The first packaged release: everything the first nine experiments built,
after thirteen years (a 2013 student project on Tamy Boubekeur's raymini
framework, modernised to C++17, GLFW and Dear ImGui).

### Added

- **Renderer.** Ray/triangle intersection through a bounding volume hierarchy
  per object (Kay & Kajiya 1986), traced tile by tile on every core, with a
  synchronous single-threaded reference the tests compare against.
- **Shading.** Lambert diffuse and the Blinn-Phong highlight (Phong 1975,
  Blinn 1977), hard and soft shadows from disk lights (Appel 1968; Cook,
  Porter & Carpenter 1984), ambient occlusion (Zhukov, Iones & Kronin 1998),
  mirror reflections and glass with Snell's law and the Fresnel equations
  (Whitted 1980), an optional ground plane that catches the shadows.
- **Image.** n x n supersampling with optional jitter (Whitted 1980, Cook
  1986), a floating-point radiance buffer, and a display that meters the
  exposure, applies a tone curve (Reinhard 2002, or ACES) and encodes in
  sRGB; `.hdr` output keeps the radiance (Ward 1991).
- **Modes.** Lit, ambient, hit mask, normals, depth, object id and ambient
  occlusion; each explains what it computes, how to read it and where it
  comes from, in the viewer, in `--help` and in the README.
- **Formats.** OFF, and OBJ with its MTL materials (colour, specular,
  shininess, transparency, index of refraction). Models are stood upright on
  load.
- **Tools.** `raymini-cli`, a headless renderer (`--version`, and a flag per
  effect), and `raymini`, a GLFW + Dear ImGui viewer with a GL preview, a
  raytraced panel that fills in tile by tile, and a display row that re-maps
  the last render without tracing it again.
- **Tests.** 114 cases and 7903 checks on synthetic geometry, each proving a
  physical property or an invariant, plus 42 golden images, run on Ubuntu and
  macOS by the CI.

[Unreleased]: https://github.com/alexduros/RayTracer/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/alexduros/RayTracer/releases/tag/v0.1.0
