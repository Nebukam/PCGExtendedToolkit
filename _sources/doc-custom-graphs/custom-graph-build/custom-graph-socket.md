---
layout: page
parent: Build Custom Graph
grand_parent: Custom Graphs
title: Socket
subtitle: Data definition for a single custom graph socket.
color: blue
#summary: summary_goes_here
splash: icons/icon_custom-graphs-build.svg
preview_img: placeholder.jpg
toc_img: placeholder.jpg
tagged: 
    - node
    - customgraph
nav_order: 2
---

{% include header_card_node %}

{% include img a='details/details-customgraph-socket.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Prune Points           | If enabled, `Vtx` that aren't part of the hull are pruned from the output.   |
| **Hull** Attribute Name           | Name of the attribute to write the "is on hull" flag to.<br>*Disabled if points are pruned, since the output in that case will be exclusively hull points.* |
|**Projection Settings**| Projection settings allow you to control the projection plane used to compute the graph in 2D. See [Projection Settings](#settings-projection)|

{% include embed id='settings-edge-types' %}