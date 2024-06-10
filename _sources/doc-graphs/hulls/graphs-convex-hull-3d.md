---
layout: page
grand_parent: Graphs
parent: Hulls
title: Convex Hull 3D
subtitle: Outputs a 3D convex hull.
color: blue
summary: The **Convex Hull 3D** Outputs the edges/graph of a 3D convex hull. Prune points to exclude non-hull vertices. Specify attributes and projection settings for customization.
splash: icons/icon_graphs-convex-hull.svg
preview_img: docs/splash-convex-hull-3D.png
toc_img: placeholder.jpg
tagged: 
    - node
    - graphs
nav_order: 1
see_also:
    - Working with Graphs
inputs:
    -   name : In
        desc : Points clouds that will be triangulated
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

{% include img a='details/details-convex-hull-3d.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Prune Points           | If enabled, `Vtx` that aren't part of the hull are pruned from the output.   |
| **Hull** Attribute Name           | Name of the attribute to write the "is on hull" flag to.<br>*Disabled if points are pruned, since the output in that case will be exclusively hull points.* |

> Note that the hull is *optimized* and will ignore points that *lie* on the hull but don't mathematically influence it *(i.e collinear/coplanar points)*.
{: .warning }

---
# Inputs
## In
The input points to generate a Convex Hull from.  
Each input dataset is processed separately and will generate its own hull.

---
# Outputs
## Vtx & Edges
See {% include lk id='Working with Graphs' %}
