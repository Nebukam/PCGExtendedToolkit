---
layout: page
parent: Custom Graphs
grand_parent: All Nodes
title: Build Custom Graph
subtitle: Build a custom graph from a one or multiple params.
color: blue
#summary: summary_goes_here
splash: icons/icon_custom-graphs-build.svg
preview_img: docs/splash-customgraph-build.png
toc_img: placeholder.jpg
has_children: true
tagged: 
    - node
    - customgraph
nav_order: 2
---

{% include header_card_node %}

{% include img a='details/details-customgraph-build.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Prune Points           | If enabled, `Vtx` that aren't part of the hull are pruned from the output.   |
| **Hull** Attribute Name           | Name of the attribute to write the "is on hull" flag to.<br>*Disabled if points are pruned, since the output in that case will be exclusively hull points.* |
|**Projection Settings**| Projection settings allow you to control the projection plane used to compute the graph in 2D. See [Projection Settings](#settings-projection)|

{% include embed id='settings-edge-types' %}