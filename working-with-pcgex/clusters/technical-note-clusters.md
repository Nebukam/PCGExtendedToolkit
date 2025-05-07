---
description: Technical design node on Cluster data
hidden: true
icon: microchip
---

# Technical Note : Clusters

Vtx & Edge Data

TODO :\
\- Why vtx have multiple edges\
\- Each vtx group are processed in parallel\
\- Each vtx processes its edges in parallel as well\
\- Cached clusters stay in memory until flushed, good for execution time, bad for memory\
\- Can be tweaked per node, or in the project settings.\


<details>

<summary>Note about the future of PCG</summary>

PCG is always evolving, and some of the initial constraints I had to deal with when making PCGEx will slowly fade away. I'm hoping that starting in 5.6 I won't have rely on tags to maintain relationship between vtx & edges.

</details>

