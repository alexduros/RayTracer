# Models

Five shapes, each earning its place, and the texture that comes with one of
them. `raymini-cli <name>` and the viewer's picker resolve a bare name here.

| File | Triangles | Why it is here |
|------|-----------|----------------|
| `teapot.off` | 880 | The Utah teapot: smooth, curved, with a handle and a spout that catch reflections. Our best glass subject. |
| `ram.off` | 3 054 | The ram of the 2013 project, the one every gallery picture shows. |
| `ram_HD.off` | 50 544 | The same ram, finely tessellated: the heavy model the BVH is measured on. |
| `cube.obj`, `cube.mtl` | 12 | The only OBJ with materials, one per face: it proves the MTL path and the object-id mode. |
| `belly.obj`, `belly.mtl` | 74 478 | Belly, the project's mascot: five materials, and the heaviest model here. It has texture coordinates too. |
| `spot.obj` | 5 856 | Spot, with texture coordinates and a texture map: what the teapot and the rams cannot have, since OFF stores no UVs. |
| `spot_texture.png` | — | Spot's texture, 1024 x 1024, to be read once texture mapping lands (timeline step 12). |

`models/orientation.txt` says which axis of each file points up; without it,
Spot is laid on her side by the flattest-side heuristic.

## Where they come from

- **Spot** is by Keenan Crane, from his [3D Model
  Repository](https://www.cs.cmu.edu/~kmcrane/Projects/ModelRepository/). His
  README states: "As the sole author of this data, I hereby release it into
  the public domain." The file here is his `spot_triangulated.obj`, renamed
  `spot.obj` so a bare name resolves it, and his `spot_texture.png`
  unchanged.
- **Belly** is Alexandre Duros's mascot, exported from three-d-stage.
- **The teapot, the rams and the cube** come with Tamy Boubekeur's raymini
  teaching framework, which this project started from in 2013. The teapot is
  Martin Newell's, 1975.

A model that is not in this table was dropped in v0.4.0: two dozen course
meshes nothing tested or documented, 7 MB of the repository and of every
release archive. Any OFF or OBJ file still loads by path, and personal
models can sit here untracked (see `.gitignore`).
