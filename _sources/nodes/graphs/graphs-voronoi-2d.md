---
layout: page
parent: Graphs
grand_parent: Nodes
title: Voronoi 2D
subtitle: Outputs a 2D Voronoi graph.
color: blue
summary: The **Voronoi 2D** node outputs a 2D Voronoi graph with options like balanced, canon, or centroid positioning. Adjust bounds, prune sites, and mark edges on the hull. 
splash: icons/icon_graphs-voronoi.svg
preview_img: docs/splash-voronoi-2D.png
toc_img: placeholder.jpg
tagged:
    - graphs
nav_order: 6
WIP: true
---

{% include header_card_node %}

{% include img a='details/details-voronoi-2d.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Method           | The method used to position Voronoi' sites. See [Method](#method)  |
| Bounds Cutoff           | If enabled, voronoi sites outside of the input points' bounds will be pruned. Bounds are expanded by this property.<br>*Activating this will enable graph output settings, as the graph is no longer guaranteed to be complete. See {% include lk id='Working with Graphs' a='#graph-output-settings-' %}.*  |

|**Hull Identification**||
| **Hull** Attribute Name           | Name of the attribute to write the "is on hull" flag to. |
| Mark Edge on Touch           | If enabled, edges that *connects to a hull point without being on the hull themselves* will be considered as "on hull". |
|**Projection Settings**| Projection settings allow you to control the projection plane used to compute the graph in 2D. See [Projection Settings](#settings-projection)|

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

{% include_relative settings-projection.md %}

---
# Inputs
## In
The input points to generate a Voronoi graph from.  
Each input dataset is processed separately and will generate its own graph.

---
# Outputs
## Vtx & Edges
See {% include lk id='Working with Graphs' %}