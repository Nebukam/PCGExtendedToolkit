---
layout: page
parent: Custom Graphs
grand_parent: All Nodes
title: Custom Graph Params
subtitle: Create params to build a custom graph from.
color: blue
summary: The **Custom Graph Params** node is at the core of the Custom Graph family of nodes. It generate a custom data object that is then used by other nodes to build and work with custom graphs until they are interpreted as regular clusters.
splash: icons/icon_custom-graphs-params.svg
preview_img: docs/splash-customgraph-params.png
toc_img: placeholder.jpg
tagged: 
    - node
    - customgraph
nav_order: 1
---

{% include header_card_node %}

{% include img a='details/details-customgraph-params.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Prune Points           | If enabled, `Vtx` that aren't part of the hull are pruned from the output.   |
| **Hull** Attribute Name           | Name of the attribute to write the "is on hull" flag to.<br>*Disabled if points are pruned, since the output in that case will be exclusively hull points.* |
|**Projection Settings**| Projection settings allow you to control the projection plane used to compute the graph in 2D. See [Projection Settings](#settings-projection)|

{% include embed id='settings-edge-types' %}