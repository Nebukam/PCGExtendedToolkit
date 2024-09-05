---
layout: page
#grand_parent: All Nodes
parent: Clusters
title: Refine
subtitle: Algorithmic edge refinement
summary: The **Refine Edges** node allows for algorithmic pruning of edges, in order to enforce specific properties into your graph.
color: blue
splash: icons/icon_edges-refine.svg
preview_img: docs/splash-refining.png
toc_img: placeholder.jpg
has_children: true
tagged:
    - node
    - edges
see_also: 
    - Refining
nav_order: 12
inputs:
    -   name : Vtx
        desc : Endpoints of the input Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the input Vtxs
        pin : points
    -   name : Heuristics
        desc : Heuristic nodes, if required by the selected refinement.
        pin : params
outputs:
    -   name : Vtx
        desc : Endpoints of the output Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the output Vtxs
        pin : points
---

{% include header_card_node %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Refinement           | This property lets you select which kind of refinement you want to apply to the input clusters.<br>**Specifics of the instanced module 
will be available under its inner Settings section.**<br>*See {% include lk id='Refining' %}.*  |
| Output Only Edges As Points | If enabled, this node will output edges as raw points, without the usually associated cluster. |

## Sanitization
The sanitization property lets you enforce some general conditions within the graph. Note that is applied after the refinement.

| Sanitization       | Description          |
|:-------------|:------------------|
| None           | No sanitization.  |
| Shortest           | If a node has no edge left, restore the shortest one.|
| Longest           | If a node has no edge left, restore the longest one.|

> Note that the sanitization options offer no guarantee that the initial interconnectivity will be preserved! 
{: .warning }

---
## Refining modules
<br>
{% include card_any tagged="edgerefining" %}

{% include embed id='settings-cluster-output' %}
{% include embed id='settings-performance' %}

---
# Inputs & Outputs
## Vtx & Edges
See {% include lk id='Working with Clusters' %}
