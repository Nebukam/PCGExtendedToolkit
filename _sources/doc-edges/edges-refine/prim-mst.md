---
layout: page
grand_parent: Edges
parent: Refine
title: Minimum Spanning Tree
subtitle: Implementation of Prim's Minimum Spanning Tree algorithm.
color: white
#summary: Index-based moving-average sampling
splash: icons/icon_edges-refine.svg
preview_img: docs/splash-mst.png
toc_img: placeholder.jpg
tagged: 
    - edgerefining
nav_order: 1
---

{% include header_card_node %}

This module offers an implementation of [Prim's algorithm](https://en.wikipedia.org/wiki/Prim%27s_algorithm) to find the minimum spanning tree of individual clusters.  
*By design, the output is guaranteed to be sanitized (e.g, each cluster will retains its existing connectivity properties).*

> Under the hood, MST leverages the {% include lk id='Local Distance' %} heuristic module -- enabling full tweaking of what is considered the "minimum-length" edge.  
> *The default settings is the "canon" implementation.*
{: .infos-hl }

{% include img a='docs/refine-mst/image.png' %} 
{% include img a='docs/refine-mst/details.png' %} 

| Property       | Description          |
|:-------------|:------------------|
| Heuristics Modifiers           | See {% include lk id='🝰 Heuristic Attribute' %}.|
