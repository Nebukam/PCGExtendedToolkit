---
layout: page
grand_parent: Clusters
parent: Graphs
title: Voronoi 3D
subtitle: Outputs a 3D Voronoi graph.
summary: The **Voronoi 3D** node outputs a 3D Voronoi graph with options like balanced, canon, or centroid positioning. Adjust bounds, prune sites, and mark edges on the hull. 
color: blue
splash: icons/icon_graphs-voronoi.svg
preview_img: docs/splash-voronoi-3D.png
toc_img: placeholder.jpg
tagged:
    - node
    - clusters
    - graphs
nav_order: 3
see_also:
    - Working with Clusters
example: EdgesAndGraphs/PCGEx_Graph_Voronoi-3D
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

{% include img a='details/details-voronoi-3d.png' %} 

> If you'd like to know more about Voronoi intrinsic properties, check out the [Wikipedia article](https://en.wikipedia.org/wiki/Voronoi_diagram)!

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Method           | The method used to position Voronoi' sites. See [Method](#method)  |
| Bounds Cutoff           | If enabled, voronoi sites outside of the input points' bounds will be pruned. Bounds are expanded by this property.<br>*Activating this will enable graph output settings, as the graph is no longer guaranteed to be complete. See {% include lk id='Working with Clusters' a='#graph-output-settings-' %}.*  |

|**Hull Identification**||
| **Hull** Attribute Name           | Name of the attribute to write the "is on hull" flag to. |
| Mark Edge on Touch           | If enabled, edges that *connects to a hull point without being on the hull themselves* will be considered as "on hull". |

> Note that the hull is *optimized* and will ignore points that *lie* on the hull but don't mathematically influence it *(i.e collinear/coplanar points)*.
{: .warning }

---
## Method

There are three available methods to drive Voronoi' site position in space.

| Method       | Description          |
|:-------------|:------------------|
| Balanced           | Uses `Canon` site location when site is within bounds, and fallbacks to `Centroid` otherwise. |
| Canon           | Uses the real, computed voronoi site position.<br>**Sites on the outskirts of the graph usually have extreme deformations.**  |
| Centroid           | Uses the delaunay' triangulation centroid instead of the real position.<br>*This is usually good looking, but can lead to overlapping edges.*  |

---
# Inputs
## In
The input points to generate a Voronoi graph from.  
Each input dataset is processed separately and will generate its own graph.

---
# Outputs
## Vtx & Edges
See {% include lk id='Working with Clusters' %}