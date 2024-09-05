---
layout: page
grand_parent: Clusters
parent: Hulls
title: Convex Hull 2D
subtitle: Outputs a 2D convex hull.
summary: The **Convex Hull 2D** outputs the edges/graph of a 2D convex hull. Prune points to exclude non-hull vertices. Specify attributes and projection settings for customization.
color: blue
splash: icons/icon_graphs-convex-hull.svg
preview_img: docs/splash-convex-hull-2D.png
toc_img: placeholder.jpg
tagged: 
    - node
    - clusters
nav_order: 4
see_also:
    - Working with Clusters
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

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Prune Points           | If enabled, `Vtx` that aren't part of the hull are pruned from the output.   |
| **Hull** Attribute Name           | Name of the attribute to write the "is on hull" flag to.<br>*Disabled if points are pruned, since the output in that case will be exclusively hull points.* |
|**Projection Settings**| Projection settings allow you to control the projection plane used to compute the graph in 2D. See [Projection Settings](#settings-projection)|

> Note that the hull is *optimized* and will ignore points that *lie* on the hull but don't mathematically influence it *(i.e collinear/coplanar points)*.
{: .warning }

{% include embed id='settings-projection' %}

---
# Inputs
## In
The input points to generate a Convex Hull from.  
Each input dataset is processed separately and will generate its own hull.

---
# Outputs
## Vtx & Edges
See {% include lk id='Working with Clusters' %}

## Paths
TBD
