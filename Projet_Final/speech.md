[INTRO]

"Hi, I'm Martin, and this is my final project for the IGR course at Telecom Paris. I built a real-time 3D engine from scratch in C++ and OpenGL that combines two things: a rigid body physics simulator, and a Monte Carlo path tracer running entirely on the GPU. The goal was to explore what happens when you put physically-based simulation and physically-based rendering in the same sandbox."

[ARCHITECTURE]

"The engine is structured around four main classes. The Renderer manages the OpenGL context and dispatches the compute shaders. The Scene holds all the objects and light sources. The RigidSolver handles all the physics, independently of rendering. And Mesh and Object manage geometry and material properties. This separation was important — it means you can swap the renderer between rasterization and path tracing without touching the physics at all."

[PHYSICS — INTEGRATION]

"The physics engine uses semi-implicit Euler integration. Each frame, we first update velocities using gravity, then solve contacts, then integrate positions. For rotations, I use quaternions — the angular velocity is converted to an axis-angle representation and composed as a delta quaternion each step. One important detail: after every integration step, the quaternion is explicitly renormalized to prevent floating-point drift from distorting the inertia tensor over time."

[PHYSICS — STABILITY]

"Stability was honestly the hardest part of this project. Three mechanisms work together to keep the simulation from exploding. First, a small damping factor of 0.999 is applied to all velocities every step — this bleeds off the artificial energy that discrete integration tends to accumulate. Second, I use Baumgarte stabilization: instead of correcting penetrations directly in position — which can cause overshooting — the penetration depth is fed back as a target velocity in the constraint solver. There's also a slop threshold so tiny resting contacts don't generate jitter. Together, these let a 4-by-4-by-4 tower of 64 boxes sit stably on the ground."

[PHYSICS — COLLISION & SOLVER]

"Collision detection has three phases. A broad phase filters pairs using bounding spheres. Then depending on the shapes involved, I use either a closed-form sphere-sphere or sphere-box test, or the Separating Axis Theorem for box-box contacts. Once contacts are detected, the constraint solver kicks in. It uses Projected Gauss-Seidel — iterating 20 times per frame over all contacts, applying incremental impulses and clamping them to stay physically valid. Friction is handled with a Coulomb cone in the two tangent directions. The whole collision detection loop is parallelized with OpenMP."

[DEMO — PHYSICS SCENE]

"Here's the physics stress test. You can see 64 boxes stacked in a grid. I can shoot a sphere into the structure — and you can watch the impulse propagate through the stack as it collapses. Notice there's no jitter or explosion even as dozens of contacts are being resolved simultaneously."

[RENDERER — RASTERIZER]

"The engine has two rendering modes. The rasterizer is a standard OpenGL forward renderer — fast, good for previewing the physics in real time. I can toggle wireframe to see the mesh. But the more interesting mode is the path tracer."

[RENDERER — PATH TRACER]

"The path tracer runs as an OpenGL 4.5 compute shader, dispatched in 16-by-16 workgroups. For each pixel, 16 rays are jittered for anti-aliasing. Each ray bounces up to 5 times, accumulating light along the path. Ray-scene intersection uses AABB culling per object, Möller-Trumbore for triangles, and an analytic formula for spheres. The material model supports three paths: diffuse, with cosine-weighted hemisphere sampling; specular, with optional roughness blending; and dielectric, using Schlick's Fresnel approximation to decide between reflection and refraction, plus Beer-Lambert absorption for the thickness of glass."

[DEMO — PATH TRACER ACCUMULATION]

"When the camera is still, frames are averaged using a running temporal accumulation buffer. Watch the noise converge — each new frame contributes a decreasing weight of 1 over N, so the image gets sharper over time without any extra per-frame cost."

[DEMO — MATERIALS SCENE]

"This is the material showcase scene. We have gold, glass, and matte spheres in front of a mirror wall. The rough surfaces converge slower because each bounce samples a wider hemisphere. The glass sphere shows the Fresnel effect — at grazing angles it becomes more reflective, and inside you can see the Beer-Lambert tinting from the volume thickness."

[DEMO — MIRROR ROOM]

"In the mirror room scene, all walls are perfect reflectors. With only 5 bounces, surfaces go black where paths terminate early. Pushing to 50 bounces gives the full hall-of-mirrors effect — light keeps traveling until it finds the sky."

[GLARE]

"One effect I spent a lot of time on is the volumetric glare around emissive light sources. True atmospheric scattering would require volumetric path marching, which is way too expensive in real time. Instead, I use a power-law approximation based purely on the angular distance between the ray direction and the direction to the light. A very high exponent gives a sharp core; a lower one gives the broader atmospheric halo. It's depth-aware — geometry in front of the light correctly occludes the glow."

[SEA SCENE]

"The most complex scene combines everything: vertex animation, transparency, and glare. The water surface is animated on the CPU every frame using a sum of absolute-sine waves — which gives a peaky, choppy shape. The updated vertex positions are uploaded to the GPU SSBO each frame so the path tracer intersects the correct geometry. The challenge was managing the glare so it reflects in the moving waves without bleeding through solid geometry."

[PERFORMANCE]

"In terms of performance: in high-quality mode with 16 samples and 12 bounces, a frame takes about 30 milliseconds on this machine — that's around 33 FPS. Dropping to 4 samples and 6 bounces gives interactive rates around 7 milliseconds. The main bottleneck is the brute-force triangle loop in the shader — a BVH would be the obvious next step."

[CONCLUSION]

"To summarize: this project implements a full hybrid engine combining impulse-based rigid body simulation with a GPU Monte Carlo path tracer. The physics is stable thanks to Baumgarte correction, velocity damping, and PGS constraint solving. The renderer supports diffuse, specular, and dielectric materials with temporal accumulation and a real-time glare approximation. It was a challenging project — getting the physics stable and the rendering converging cleanly took a lot of iteration — but the result is a sandbox where you can simultaneously simulate and render physically complex scenes in real time. Thanks."