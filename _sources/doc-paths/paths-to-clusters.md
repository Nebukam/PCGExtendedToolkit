---
layout: page
#grand_parent: All Nodes
parent: Paths
title: Paths to Clusters
subtitle: Convert paths to clusters.
color: white
summary: The **Paths to Clusters** node converts multiple input paths into edge clusters by fusing points but preserving edge relationships.
splash: icons/icon_path-to-edges.svg
preview_img: docs/splash-path-to-edges.png
toc_img: placeholder.jpg
tagged: 
    - node
    - paths
nav_order: 6
inputs:
    -   name : Paths
        desc : Paths which points will be checked for collinearity
        pin : points
outputs:
    -   name : Vtx
        desc : Endpoints of the output Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the output Vtxs
        pin : points
---

{% include header_card_node %}

{% include img a='details/details-paths-to-edges.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Fuse Distance           | This define the distance at this the points are considered to be identical.  |
| **Graph Output Settings**           | *See {% include lk id='Working with Clusters' a='#graph-output-settings-' %}.* |

---
# Inputs
## In
Any number of point datasets assumed to be paths.

---
# Outputs
## Vtx & Edges
See {% include lk id='Working with Clusters' %}.  
*Reminder that empty inputs will be ignored & pruned*.