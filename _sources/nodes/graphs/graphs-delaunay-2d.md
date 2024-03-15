---
layout: page
parent: Graphs
grand_parent: All Nodes
title: Delaunay 2D
subtitle: Outputs a 2D Delaunay triangulation.
color: blue
summary: Output a 2D Delaunay triangulation with options like Urquhart graph, hull identification, and projection settings.
splash: icons/icon_graphs-delaunay.svg
preview_img: docs/splash-delaunay-2D.png
toc_img: placeholder.jpg
tagged: 
    - graphs
nav_order: 5
---

{% include header_card_node %}

{% include img a='details/details-delaunay-2d.png' %} 

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
See {% include lk id='Working with Graphs' %}