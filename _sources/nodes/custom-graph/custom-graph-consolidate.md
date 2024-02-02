---
layout: page
parent: Custom Graphs
grand_parent: Nodes
title: Consolidate
subtitle: Rebuild a custom graph after editing.
color: blue
#summary: summary_goes_here
splash: icons/icon_custom-graphs-consolidate.svg
preview_img: docs/splash-customgraph-consolidate.png
toc_img: placeholder.jpg
tagged: 
    - customgraph
nav_order: 5
---

{% include header_card_node %}

{% include img a='details/details-customgraph-consolidate.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Prune Points           | If enabled, `Vtx` that aren't part of the hull are pruned from the output.   |
| **Hull** Attribute Name           | Name of the attribute to write the "is on hull" flag to.<br>*Disabled if points are pruned, since the output in that case will be exclusively hull points.* |
|**Projection Settings**| Projection settings allow you to control the projection plane used to compute the graph in 2D. See [Projection Settings](#settings-projection)|

{% include_relative settings-edge-types.md %}