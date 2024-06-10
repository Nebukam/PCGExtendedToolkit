---
layout: page
#grand_parent: All Nodes
parent: Graphs
title: Classics
subtitle: Delaunay 2D / 3D, Voronoi 2D / 3D
color: white
summary: TBD
splash: icons/icon_misc-write-index.svg
preview_img: docs/splash-write-index.png
toc_img: placeholder.jpg
has_children: true
tagged: 
    - graphs
    - node
nav_order: 1
use_child_thumbnails: true
---

{% include header_card_node %}

Classic graphs are very basic generators that can turn any random point cloud into a nicely interconnected structure.  
Delaunay & Voronoi are two of the most popular algorithms to achieve that; as they offer the benefit of interesting & useful properties.  

> Note that the 3D version of those generators requires the points to NOT be coplanar for the maths to work -- you'll be prompted to use the 2D version otherwise.
{: .warning }

---
## Classic Graphs
<br>
{% include card_childs tagged='node' %}