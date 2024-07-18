---
layout: page
grand_parent: Clusters
parent: Graphs
title: Delaunay 2D
subtitle: Outputs a 2D Delaunay triangulation.
summary: The **Delaunay 2D** node outputs a 2D Delaunay triangulation with options like Urquhart graph, hull identification, and projection settings.
color: blue
splash: icons/icon_graphs-delaunay.svg
preview_img: docs/splash-delaunay-2D.png
toc_img: placeholder.jpg
tagged: 
    - node
    - clusters
    - graphs
nav_order: 5
see_also:
    - Working with Clusters
example: EdgesAndGraphs/PCGEx_Graph_Delaunay-2D
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

{% include img a='details/details-delaunay-2d.png' %} 

> If you'd like to know more about Delaunay intrinsic properties, check out the [Wikipedia article](https://en.wikipedia.org/wiki/Delaunay_triangulation)!

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Urquhart           | If enabled, outputs the [Urquhart](https://en.wikipedia.org/wiki/Urquhart_graph) graph instead of Delaunay.  |
|**Hull Identification**||
| **Hull** Attribute Name           | Name of the attribute to write the "is on hull" flag to. |
| Mark Edge on Touch           | If enabled, edges that *connects to a hull point without being on the hull themselves* will be considered as "on hull". |
|**Projection Settings**| Projection settings allow you to control the projection plane used to compute the graph in 2D. See [Projection Settings](#settings-projection)|

> Note that the hull is *optimized* and will ignore points that *lie* on the hull but don't mathematically influence it *(i.e collinear/coplanar points)*.
{: .warning }

{% include embed id='settings-projection' %}

---
# Inputs
## In
The input points to generate a Delaunay triangulation from.  
Each input dataset is processed separately and will generate its own triangulation.

---
# Outputs
## Vtx & Edges
See {% include lk id='Working with Clusters' %}