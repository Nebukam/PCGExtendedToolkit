---
layout: page
grand_parent: Graphs
parent: Classics
title: Delaunay 3D
subtitle: Outputs a 3D Delaunay tetrahedralization.
color: blue
summary: Output a 3D Delaunay tetrahedralization with options like Urquhart graph, hull identification, and projection settings.
splash: icons/icon_graphs-delaunay.svg
preview_img: docs/splash-delaunay-3D.png
toc_img: placeholder.jpg
tagged: 
    - node
    - graphs
nav_order: 2
see_also:
    - Working with Graphs
example: EdgesAndGraphs/PCGEx_Graph_Delaunay-3D
---

{% include header_card_node %}

{% include img a='details/details-delaunay-3d.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Urquhart           | If enabled, outputs the [Urquhart](https://en.wikipedia.org/wiki/Urquhart_graph) graph instead of Delaunay.  |

|**Hull Identification**||
| **Hull** Attribute Name           | Name of the attribute to write the "is on hull" flag to. |
| Mark Edge on Touch           | If enabled, edges that *connects to a hull point without being on the hull themselves* will be considered as "on hull". |

> Note that the hull is *optimized* and will ignore points that *lie* on the hull but don't mathematically influence it *(i.e collinear/coplanar points)*.
{: .warning }

---
# Inputs
## In
The input points to generate a Delaunay tetrahedralization from.  
Each input dataset is processed separately and will generate its own tetrahedralization.

---
# Outputs
## Vtx & Edges
See {% include lk id='Working with Graphs' %}