![UE 5.3](https://img.shields.io/badge/UE-5.3-darkgreen) ![5.4](https://img.shields.io/badge/5.4-darkgreen) ![5.5](https://img.shields.io/badge/5.5-darkgreen)
# PCG Extended Toolkit 

![PCGEx](/Resources/Icon128.png)

# What is it?
### The PCG Extended Toolkit is a plugin for [Unreal engine 5](https://www.unrealengine.com/en-US/) (5.3, 5.4, 5.5) that contains a collection of **low-level PCG Graph elements** offering additional ways to manipulate and control PCG Data in unique ways. Its primary focus is to create spatial relationships between points, and build around them; but it comes with a bunch of other super useful stuffs.

PCGEx allows you to create edge-based structures from points dataset, inside of which you can do pathfinding to generate splines; relax positions, attributes, flag specific types of connections, and much much more. 
It also comes with a set of lower-level, more generic features to manipulate attributes & points with a performance boost that can be up to 150x faster than some vanilla PCG Nodes; as well as highly granular parameters & tweaks.

## Getting Started
* **[Example Project](https://github.com/Nebukam/PCGExExampleProject)**
* **[Full documentation](https://nebukam.github.io/PCGExtendedToolkit/)**
* [Installation](https://nebukam.github.io/PCGExtendedToolkit/installation/) in your own project
* Overview of [All the Nodes](https://nebukam.github.io/PCGExtendedToolkit/all-nodes.html)!

For questions & support, join the [PCGEx Discord Server](https://discord.gg/mde2vC5gbE)!

# Highlights
### Vtx/Edge structures
A new concept of connection between points, enabling entierely new ways of working with PCG

![hl-graphs](https://raw.githubusercontent.com/Nebukam/PCGExtendedToolkit/docs/_sources/assets/misc/highlight-graphs.jpg)

### Pathfinding
Advanced pathfinding utilities with a completely modular heuristics system

![hl-graphs](https://raw.githubusercontent.com/Nebukam/PCGExtendedToolkit/docs/_sources/assets/misc/highlight-pathfinding.jpg)

### Sampling
Powerful tool to extract, transfer and blend data between groups of points, splines, meshes

![hl-sampling](https://raw.githubusercontent.com/Nebukam/PCGExtendedToolkit/docs/_sources/assets/misc/highlight-samplers.jpg)

### Low level utilities
A lot of simple yet powerful utility nodes ranging from complex sorting, complex partitioning, remapping, advanced filters with chainable conditions, to bitmask operations.

![hl-misc](https://raw.githubusercontent.com/Nebukam/PCGExtendedToolkit/docs/_sources/assets/misc/highlight-miscjpg.jpg)

#### *And so much more -- PCGEx has 100+ nodes!*

### Disclaimer
This software is provided under the MIT License. It is freely available for use & modifications, and may be incorporated into commercial products without the necessity of attribution (*though it is appreciated*). **The contents of this project are entirely original, comprising no AI-generated materials or third-party content, including but not limited to code and assets.**

### Thanks
- Kudo to [@MikeC](https://github.com/mikec316) for his reckless experiments with uncooked releases, feedbacks, suggestions. Without him this plugin wouldn't be as useful and stable as it is today.
- The Epic staff in the person of [@Amathlog](https://github.com/Amathlog), for his availability and readiness to help with all things PCG.
- [@Erlandys](https://github.com/Erlandys) for his invaluable insights into advanced C++.
- [@MaximeDup](https://github.com/MaximeDup) & [@EmSeta](https://github.com/EmSeta) for helping with the macOS version
