---
layout: page
#grand_parent: All Nodes
parent: Clusters
title: Refine
name_in_editor: "Cluster : Refine"
subtitle: Algorithmic edge refinement
summary: The **Refine** node algorithmically prunes edges in a graph to enforce specific properties, allowing selection of refinement type and optional sanitization to restore edges based on predefined conditions.
use_child_thumbnails: true
color: blue
splash: icons/icon_edges-refine.svg
preview_img: docs/splash-refining.png
toc_img: placeholder.jpg
has_children: true
tagged:
    - node
    - edges
see_also: 
    - Working with Clusters
    - Refining
nav_order: 12
inputs:
    -   name : Vtx
        desc : Endpoints of the input Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the input Vtxs
        pin : points
    -   name : 🝰 Heuristics
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

The **Cluster Refine** node lets you refine connections inside individual clusters, a.k.a Edge pruning.  
{: .fs-5 .fw-400 } 

> Refining only **removes** edges and does not create new ones.

{% include img a='placeholder-wide.jpg' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Refinement           | This property lets you select which kind of refinement you want to apply to the input clusters.<br>**Specifics of the instanced module will be available under its inner Settings section, if any.**  |
| Output Only Edges As Points | If enabled, this node will output edges as raw points, without the usually associated cluster.<br>*This can be significantly faster and less greedy depending on your need for the output edges.* |

---
## Sanitization
The sanitization property lets you enforce some general conditions within the graph. Note that is applied after the refinement.

| Sanitization       | Description          |
|:-------------|:------------------|
| None           | No sanitization.  |
| Shortest           | If a node has no edge left, restore the shortest one.|
| Longest           | If a node has no edge left, restore the longest one.|

> Note that the **sanitization options offer no guarantee that the initial interconnectivity will be preserved!** 
{: .warning }

---
## Available Refining modules
<br>
{% include card_any tagged="edgerefining" %}

{% include embed id='settings-cluster-output' %}
{% include embed id='settings-performance' %}
