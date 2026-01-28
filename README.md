![UE 5.7](https://img.shields.io/badge/UE-5.7-darkgreen) ![5.6](https://img.shields.io/badge/5.6-darkgreen) ![5.5](https://img.shields.io/badge/5.5-gray) ![5.4](https://img.shields.io/badge/5.4-gray) ![5.3](https://img.shields.io/badge/5.3-gray) [![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/Nebukam/PCGExtendedToolkit)

<p align="center">
  <img src="https://raw.githubusercontent.com/Nebukam/PCGExtendedToolkit/refs/heads/docs/_sources/smol-logo.png" alt="PCGEx Logo">
</p>

<h1 align="center">PCG Extended Toolkit (PCGEx)</h1>

<p align="center">
  <strong>200+ nodes for advanced procedural generation in Unreal Engine</strong><br>
  Graph theory, pathfinding, spatial queries, asset management, and more.
</p>

<p align="center">
  <a href="https://pcgex.gitbook.io/pcgex">Documentation</a> •
  <a href="https://pcgex.gitbook.io/pcgex/general/quickstart/installation">Installation</a> •
  <a href="https://pcgex.gitbook.io/pcgex/changelogs">Changelogs</a> •
  <a href="https://discord.gg/mde2vC5gbE">Discord</a> •
  <a href="https://www.patreon.com/c/pcgex">Support on Patreon</a>
</p>

---

## What is PCGEx?

### PCGEx is a **low-level, use-case agnostic toolkit** extending Unreal Engine's PCG framework with **200+ nodes**.

Vanilla PCG excels at scattering and rule-based placement. PCGEx adds what's missing: **structure**. Build graphs from points, find paths through them, analyze topology, and work with explicit connections—not just proximity. Delaunay, Voronoi, MST, convex hulls, A* pathfinding, and more.

Graph theory is just the headline. PCGEx is also a comprehensive data manipulation toolkit: spatial queries, sampling, blending, path operations, polygon booleans, asset management, filtering, sorting—the low-level primitives that vanilla PCG doesn't provide. Reusable sub-nodes (filters, heuristics, blenders) plug into operations to keep your graphs clean. 

> _It doesn't solve specific problems for you._  
_It gives you the tools to solve them yourself._

<!-- SCREENSHOT: Hero image showing a complex procedural generation result that highlights the variety of what's possible -->

---

## Getting Started

### Installation

PCGEx is available through multiple channels:

- **[FAB](https://www.fab.com/)** — Epic's official marketplace
- **[Gumroad](https://nebukam.gumroad.com/)** — Precompiled Binaries _(until FAB is back)_
- **[Source](https://github.com/Nebukam/PCGExtendedToolkit)** — Build from GitHub

See the [Installation Guide](https://nebukam.github.io/PCGExtendedToolkit/installation.html) for detailed instructions.

### Example Project

The best way to learn PCGEx is through the **[Example Project](https://pcgex.gitbook.io/pcgex/basics/quickstart/example-project)**, which contains hundred of annotated graphs and complex examples demonstrating PCGEx capabilities.

<!-- SCREENSHOT: Overview of example project content - perhaps a grid of different example scenes -->

![Example Project](https://github.com/user-attachments/assets/b8bd713e-0b60-4cdc-84d9-dd776d452bf8)

### Documentation

- **[Gitbook Documentation](https://pcgex.gitbook.io/pcgex)** — Comprehensive guides and tutorials
- **[Discord Server](https://discord.gg/mde2vC5gbE)** — Community support and discussion

---

## Key Features

### Clusters & Graphs
The heart of PCGEx. Transform points into connected networks via Delaunay, Voronoi, convex hulls, MST, and custom builders. Every connection is data you can query, filter, refine, and build upon. This is what vanilla PCG can't do.

<!-- SCREENSHOT: Side-by-side: scattered points → Delaunay → refined graph → final result -->

![hl-graphs](https://raw.githubusercontent.com/Nebukam/PCGExtendedToolkit/docs/_sources/assets/misc/highlight-graphs.jpg)

### Filter Ecosystem
Define selection logic once, reuse everywhere. AND/OR composition, attribute tests, spatial queries, bitmasks—all as portable sub-nodes that plug into operations. No more duplicating filter chains across your graph.

### Asset Collections
Curate meshes, actors, and data assets with weighted distribution, tags, and per-entry property overrides. Define a collection once, use it consistently everywhere. The asset management layer vanilla PCG lacks.

### Paths
Smooth, simplify, subdivide, cut, fuse, offset, bevel. Tangent operations for clean curves. Convert freely between points, paths, and splines. A complete path manipulation toolbox.

<!-- SCREENSHOT: Path pipeline: raw → smoothed → subdivided → final -->

![hl-paths](https://raw.githubusercontent.com/Nebukam/PCGExtendedToolkit/docs/_sources/assets/misc/highlight-paths.jpg)

### Pathfinding
A*/Djikstra routing through your clusters with pluggable heuristics. Weight by distance, slope, attributes, or custom logic. Find optimal paths when you need them, _without a single loop_.

<!-- SCREENSHOT: Weighted paths through a graph -->

![hl-pathfinding](https://raw.githubusercontent.com/Nebukam/PCGExtendedToolkit/docs/_sources/assets/misc/highlight-pathfinding.jpg)

### Spatial Operations
**Point fusion** is foundational—merge nearby points with attribute blending. Beyond that: Lloyd relaxation, bin packing, octree queries, bounds analysis. Power tools for when you need them.

### Sampling & Blending
Transfer data between point sets, surfaces, splines, textures. Configurable weighting and falloff. The glue that connects disparate data sources.

![hl-sampling](https://raw.githubusercontent.com/Nebukam/PCGExtendedToolkit/docs/_sources/assets/misc/highlight-samplers.jpg)

### Tensors & Vector Fields
Spatial effectors that influence transforms. Stack them for complex directional fields—orienting objects, extruding paths, guiding growth.

![hl-tensors](https://raw.githubusercontent.com/Nebukam/PCGExtendedToolkit/docs/_sources/assets/misc/highlight-tensors.jpg)

### Probing
Build clusters from connection rules—define how points should connect based on spatial relationships and let PCGEx figure out the graph.

### Valency (WFC)
Wave Function Collapse for modular generation. Impressive constraint solving with a dedicated editor mode. Eye candy that actually works.

### Supporting Tools
**Shapes** — 2D/3D primitives, Clipper2 polygon booleans.  
**Topology** — Boundary detection, flood fill, island analysis.  
**Noise** — 3D procedural noise for natural variation.  
**Bridges** — Convert between meshes, clusters, and paths.  
**Utilities** — Sorting, partitioning, attributes, hashes, so much more.  


![hl-misc](https://raw.githubusercontent.com/Nebukam/PCGExtendedToolkit/docs/_sources/assets/misc/highlight-miscjpg.jpg)


---

## Interop Plugins

PCGEx has companion plugins for specialized integrations:

| Plugin | Description |
|--------|-------------|
| **[PCGEx + ZoneGraph](https://github.com/Nebukam/PCGExtendedToolkitZoneGraph)** | Generate ZoneGraph data for AI navigation from PCGEx clusters |
| **[PCGEx + Watabou](https://github.com/Nebukam/PCGExtendedToolkitWatabou)** | Import procedural maps from Watabou's generators |

---

## For C++ Developers

PCGEx is designed for extensibility. The plugin provides a robust framework for creating custom PCG nodes with:

- **Processor Pattern** — Per-input processing with automatic parallelization across threads
- **Factory System** — Pluggable operations (filters, blenders, samplers) via a Settings → Factory → Operation pipeline
- **Data Facades** — Type-safe, cached attribute access with thread-safe buffer management
- **Cluster Infrastructure** — Full graph/topology data structures ready for custom algorithms

### Architecture

The plugin is organized into **core modules** (foundational infrastructure) and **element modules** (node implementations):

**Core Modules:**
```
PCGExCore          → Data facades, threading primitives, macros, containers
PCGExGraphs        → Graph/cluster structures, node/edge topology
PCGExFilters       → Composable filter system with manager orchestration
PCGExBlending      → Attribute blending with multiple blend modes
PCGExCollections   → Asset collection management, weighted picking
PCGExFoundations   → Polylines, tangents, geometric primitives
PCGExProperties    → Unified property system across modules
PCGExMatching      → Pattern matching framework
PCGExHeuristics    → Heuristic calculations for pathfinding
PCGExNoise3D       → Procedural noise
```

All processing runs **off the game thread** with pre-allocated buffers and parallel-safe patterns. Custom nodes inherit these capabilities automatically by extending the appropriate base classes.

See [CONTRIBUTING.md](https://github.com/Nebukam/PCGExtendedToolkit/blob/main/CONTRIBUTING.md) for development guidelines.

---

## Support the Project

PCGEx is free and open source under the MIT license. If it's useful to your work, consider:

- ⭐ **Starring** the repository
- 💬 **Joining** the [Discord community](https://discord.gg/mde2vC5gbE)
- ❤️ **Supporting** on [Patreon](https://www.patreon.com/c/pcgex)

---

## Acknowledgments

### Supporters
Check out the [Supporters page](https://pcgex.gitbook.io/pcgex/supporters) on Gitbook!

Special thanks to [Sine Nomine Associates](https://sinenomine.net/) for generously providing and maintaining the automated Linux build infrastructure.

### Special Thanks

| | |
|---|---|
| **[@MikeC](https://github.com/mikec316)** | Reckless experiments, feedback, and suggestions that shaped the plugin into what it is today |
| **[@Amathlog](https://github.com/Amathlog)** | Epic Games staff, invaluable PCG framework guidance |
| **[@Erlandys](https://github.com/Erlandys)** | Advanced C++ insights |
| **[@Syscrusher](https://github.com/sna-scourtney)** | Linux support and maintenance |
| **[@staminajim](https://github.com/staminajim), [@MaximeDup](https://github.com/MaximeDup)** and **[@EmSeta](https://github.com/EmSeta)** | macOS compatibility |

And all the [contributors](https://github.com/Nebukam/PCGExtendedToolkit/graphs/contributors) who make this project better! ❤️

### Third-Party Libraries

- **[delaunator-cpp](https://github.com/delfrrr/delaunator-cpp)** — Fast Delaunay triangulation
- **[Clipper2](https://github.com/AngusJohnson/Clipper2)** — Polygon clipping and offsetting (modified C++ port, v2.0.1) by Angus Johnson

---

## License

**MIT License** — Free for personal and commercial use. Attribution appreciated but not required.
