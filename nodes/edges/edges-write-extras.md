---
layout: page
parent: Edges
grand_parent: Nodes
title: Write Extras
subtitle: Compute edge extra data from its vertices
color: white
#summary: summary_goes_here
splash: icons/icon_edges-extras.svg
preview_img: docs/splash-edges-extras.png
toc_img: placeholder.jpg
tagged: 
    - edges
see_also: 
    - Interpolate
nav_order: 4
---

{% include header_card_node %}

{% include img a='details/details-edge-extras.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Blending Settings**| When enabled, the edge will inherit properties and attributes from its `Start` and `End` point.<br>It uses {% include lk id='Interpolate' %} blending under the hood. |

| **Output**           ||
| **Edge Length** Attribute Name           | When enabled, the `length` of the edge will be written to the specified attribute.<br>*The length of an edge is the distance between its start and end point.* |

---
# Inputs & Outputs
See {% include lk id='Working with Graphs' %}