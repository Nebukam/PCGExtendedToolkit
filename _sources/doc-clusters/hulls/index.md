---
layout: page
#grand_parent: All Nodes
parent: Clusters
title: Hulls
subtitle: Convex hull algorithms
summary: TBD
color: white
splash: icons/icon_misc-write-index.svg
preview_img: placeholder.jpg
toc_img: placeholder.jpg
has_children: true
use_child_thumbnails: true
tagged: 
    - clusters
    - node
nav_order: 30
---

{% include header_card_toc %}

Hulls capture the convex bounding shape of a set of point.  
The current implementation is based on an underlying Delaunay Graph.
{: .fs-5 .fw-400 } 

---
## Available Hulls
<br>
{% include card_childs tagged='clusters' %}

