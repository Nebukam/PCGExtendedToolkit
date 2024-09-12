---
layout: page
#grand_parent: All Nodes
parent: Clusters
title: Graphs
subtitle: Delaunay 2D / 3D, Voronoi 2D / 3D
summary: TBD
color: white
splash: icons/icon_misc-write-index.svg
preview_img: placeholder.jpg
toc_img: placeholder.jpg
has_children: true
use_child_thumbnails: true
tagged: 
    - clusters
    - graphs
    - node
nav_order: 1
---

{% include header_card %}

Classic graphs are very basic generators that can turn any random point cloud into a nicely interconnected structure.  
Delaunay & Voronoi are two of the most popular algorithms to achieve that; as they offer the benefit of interesting & useful properties.  

> Note that the 3D version of those generators requires the points to NOT be coplanar for the maths to work -- you'll be prompted to use the 2D version otherwise.
{: .warning }

---
## Classic Graphs
<br>
{% include card_childs tagged='node' %}