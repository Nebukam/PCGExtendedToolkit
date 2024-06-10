---
layout: page
#grand_parent: All Nodes
parent: Edges
title: Refine
subtitle: Algorithmic edge refinement
color: blue
summary: The **Refine Edges** node allows for algorithmic pruning of edges, in order to enforce specific properties into your graph.
splash: icons/icon_edges-refine.svg
preview_img: docs/splash-refining.png
toc_img: placeholder.jpg
has_children: true
tagged:
    - node
    - edges
see_also: 
    - Refining
nav_order: 2
inputs:
    -   name : Vtx
        desc : Endpoints of the input Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the input Vtxs
        pin : points
outputs:
    -   name : Vtx
        desc : Endpoints of the output Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the output Vtxs
        pin : points
---

{% include header_card_node %}

{% include img a='details/details-edges-refine.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Refinement           | This property lets you select which kind of refinement you want to apply to the input clusters.<br>**Specifics of the instanced module will be available under its inner Settings section.**<br>*See {% include lk id='Refining' %}.*  |

---
## Refining modules
<br>
{% include card_any tagged="edgerefining" %}

{% include embed id='settings-graph-output' %}
{% include embed id='settings-performance' %}

---
# Inputs & Outputs
## Vtx & Edges
See {% include lk id='Working with Graphs' %}
