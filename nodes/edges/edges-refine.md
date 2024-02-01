---
layout: page
parent: Edges
grand_parent: Nodes
title: Refine
subtitle: Algorithmic edge refinement
color: blue
summary: The **Refine Edges** node allows for algorithmic pruning of edges, in order to enforce specific properties in.
splash: icons/icon_edges-refine.svg
preview_img: docs/splash-refining.png
toc_img: placeholder.jpg
tagged:
    - edges
see_also: 
    - Refining
nav_order: 2
---

{% include header_card_node %}

{% include img a='details/details-edges-refine.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Refinement           | This property lets you select which kind of refinement you want to apply to the input clusters.<br>**Specifics of the instanced module will be available under its inner Settings section.**<br>*See {% include lk id='Refining' %}.*  |
| **Graph Output Settings**           | *See {% include lk id='Working with Graphs' a='#graph-output-settings-' %}.* |

---
# Inputs & Outputs
See {% include lk id='Working with Graphs' %}

---
# Modules

## Available {% include lk id='Refining' %} modules
<br>
{% include card_any tagged="edgerefining" %}
