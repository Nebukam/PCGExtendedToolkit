![UE 5.6](https://img.shields.io/badge/UE-5.6-darkgreen)
# PCG Extended Toolkit 

![PCGEx](https://raw.githubusercontent.com/Nebukam/PCGExtendedToolkit/refs/heads/docs/_sources/smol-logo.png)

# What is it?
### The PCG Extended Toolkit is a plugin for [Unreal engine 5](https://www.unrealengine.com/en-US/) (5.6, + LTS on 5.5, 5.4, 5.3) that contains a collection of **low-level PCG Graph elements** offering additional ways to manipulate and control PCG Data in unique ways. Its primary focus is to create spatial relationships between points, and build around them; but it comes with a bunch of other super useful stuffs.

PCGEx allows you to create edge-based structures from points dataset, inside of which you can do pathfinding to generate splines; relax positions, attributes, flag specific types of connections, and much much more. 
It also comes with a set of lower-level, more generic features to manipulate attributes & points; as well as highly granular parameters & tweaks.

## Getting Started
— **[Documentation](https://pcgex.gitbook.io/pcgex)**  
— **[Installation](https://nebukam.github.io/PCGExtendedToolkit/installation.html) in your own project** | Or get it from [FAB](https://www.fab.com/listings/3f0bea1c-7406-4441-951b-8b2ca155f624)  
— Overview of [All the Nodes](https://nebukam.github.io/PCGExtendedToolkit/all-nodes.html)  

For questions & support, join the [PCGEx Discord Server](https://discord.gg/mde2vC5gbE)!

— Support the project on **[Patreon](https://www.patreon.com/c/pcgex)**  

## **[Example Project](https://pcgex.gitbook.io/pcgex/basics/quickstart/example-project)**
![image](https://github.com/user-attachments/assets/b8bd713e-0b60-4cdc-84d9-dd776d452bf8)

# Highlights
### Graph theory with Vtx/Edge structures
A new concept of connection between points, enabling entierely new ways of working with PCG

![hl-graphs](https://raw.githubusercontent.com/Nebukam/PCGExtendedToolkit/docs/_sources/assets/misc/highlight-graphs.jpg)

### Pathfinding
Advanced pathfinding utilities with a completely modular heuristics system

![hl-graphs](https://raw.githubusercontent.com/Nebukam/PCGExtendedToolkit/docs/_sources/assets/misc/highlight-pathfinding.jpg)

### Sampling
Powerful tool to extract, transfer and blend data between groups of points, splines, meshes

![hl-sampling](https://raw.githubusercontent.com/Nebukam/PCGExtendedToolkit/docs/_sources/assets/misc/highlight-samplers.jpg)

### Path Manipulation
Extensive toolset focusing on path manipulation & modification
![hl-paths](https://raw.githubusercontent.com/Nebukam/PCGExtendedToolkit/docs/_sources/assets/misc/highlight-paths.jpg)

### Tensors & vector fields
A very easy to use toolset to create & work with tensors, allowing to transform points & extrude paths using spatial effectors

![hl-misc](https://raw.githubusercontent.com/Nebukam/PCGExtendedToolkit/docs/_sources/assets/misc/highlight-tensors.jpg)

### Low level utilities
A lot of simple yet powerful utility nodes ranging from complex sorting, complex partitioning, remapping, advanced filters with chainable conditions, to bitmask operations.

![hl-misc](https://raw.githubusercontent.com/Nebukam/PCGExtendedToolkit/docs/_sources/assets/misc/highlight-miscjpg.jpg)

#### *And so much more -- PCGEx has 180+ nodes!*


# Interops
PCGEx has a few additional interops plugins to interfaces with "other things":

— **[PCGEx + Zone Graph](https://github.com/Nebukam/PCGExtendedToolkitZoneGraph)**  
— **[PCGEx + Watabou](https://github.com/Nebukam/PCGExtendedToolkitWatabou)**  

---

### [Contributing](https://github.com/Nebukam/PCGExtendedToolkit/blob/main/CONTRIBUTING.md)
### Disclaimer
This software is provided under the MIT License. It is freely available for use & modifications, and may be incorporated into commercial products without the necessity of attribution (*though it is appreciated*).

### Supporters
-Check out the [Supporters](https://pcgex.gitbook.io/pcgex/supporters) page on gitbook!
- Thanks to [Sine Nomine Associates](https://sinenomine.net/) for generously providing and maintaining the automated Linux build infrastructure that helps keep this project running smoothly.

### Special Thanks
- Kudo to [@MikeC](https://github.com/mikec316) for his reckless experiments with uncooked releases, feedbacks, suggestions. Without him this plugin wouldn't be as useful and stable as it is today.
- The Epic staff in the person of [@Amathlog](https://github.com/Amathlog), for his availability and readiness to help with all things PCG.
- [@Erlandys](https://github.com/Erlandys) for his invaluable insights into advanced C++.
- [@Syscrusher](https://github.com/sna-scourtney) for his invaluable help and support on Linux
- [@MaximeDup](https://github.com/MaximeDup) & [@EmSeta](https://github.com/EmSeta) for helping with the macOS version
- And of course the contributors, but they have their own special place on the sidebar <3

### Third party & Credits
- This plugin includes [delaunator-cpp](https://github.com/delfrrr/delaunator-cpp)
- The `PCGExElementsClipper` module includes a C++ modified port of the [Clipper2](https://github.com/AngusJohnson/Clipper2) (2.0.1) library created by Angus Johnson
- The `PCGExElementsCavalierContours` module includes a C++ modified port of the [cavalier_contours](https://github.com/jbuckmccready/cavalier_contours) Rust library created by jbuckmccready