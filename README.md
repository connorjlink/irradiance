![final_outdoors.png](final_outdoors.png)

![final_cornell_diffracted.png](final_cornell_diffracted.png)

# Feature List

## Required Basic Functionality

 - [x] A camera with configurable position, orientation, and field of view
 - [x] Anti-aliasing
 - [x] Ray/sphere intersections
 - [x] Ray/triangle intersections
 - [x] The ability to load textures (PNG, BMP, HDR/EXR)
 - [x] Textured spheres and triangles
 - [x] The ability to load and render triangle meshes (OBJ/MTL)
 - [x] BVH spatial subdivision acceleration 
 - [x] Specular, diffuse, and dielectric materials
 - [x] Emissive materials

## Bonus Features

 - [x] High dynamic range images: 10
 - [x] Homogenous volume rendering: 10
 - [x] Quads: 10
 - [x] Quadrics: 15
 - [ ] Spectral rendering: 30
 - [x] BRDF materials with Cook–Torrance, GGX D, Smith masking-and-shadowing G: 30
 - [ ] Subsurface scattering (BSSRDFs): 30
 - [x] Motion blur: 10
 - [x] Defocus blur/depth of field: 10
 - [x] Object instancing: 10
 - [x] Perlin noise: 10
 - [x] Cube maps (skybox projected as a sphere map): 15
 - [x] Importance sampling (next-event estimation for direct lighting and shaped reflection lobes): 15
 - [x] Parallelization: 10
 - [x] Normal interpolation (smooth shading): 5
 - [ ] Hybrid rendering with a GPU (OpenGL/DirectX + ray tracing): 20
 - [ ] GPU acceleration (GPU computing w/ e.g., CUDA): 20
 - [ ] Adaptive sampling: 15

## Additional Features Implemented not Explicitly Prescribed

- [x] Post-process lens diffraction modelling with aperture mask via FFT and convolution
- [x] Temporal sample reprojection