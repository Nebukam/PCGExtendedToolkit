---
layout: page
grand_parent: Clusters
parent: Refine
title: 🝔 Minimum Spanning Tree
subtitle: Modified Prim's Minimum Spanning Tree algorithm that supports heuristics.
#summary: The **MST** refinement ...
color: blue
splash: icons/icon_edges-refine.svg
see_also:
    - Refine
tagged: 
    - edgerefining
nav_order: 1
---

{% include header_card_toc %}

This refinement offers a highly customizable implementation of [Prim's algorithm](https://en.wikipedia.org/wiki/Prim%27s_algorithm) to find the minimum spanning tree of individual clusters.  
*By design, the output is guaranteed to be sanitized (e.g, each cluster will retains its existing connectivity properties).*
{: .fs-5 .fw-400 } 

> It relies on user-defined {% include lk id='🝰 Heuristics' %} in order to build the tree, providing very high level of control.  
{: .infos-hl }

{% include img a='details/edges-refine/refine-prim-mst.png' %}

---
## Available Heuristics Modules
<br>
{% include card_any tagged="heuristics" %}
