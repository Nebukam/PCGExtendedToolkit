---
layout: page
parent: Edges
#grand_parent: All Nodes
title: Write Paths Extras
subtitle: Compute path extra data
color: white
summary: Writes a variety of path statistics such as length, time, normals, centroids etc
splash: icons/icon_edges-extras.svg
preview_img: placeholder.jpg
toc_img: placeholder.jpg
tagged: 
    - node
    - paths
nav_order: 4
---

{% include header_card_node %}

{% include img a='details/details-edge-extras.png' %} 

> DOC TDB
{: .warning }

| Property       | Description          |
|:-------------|:------------------|
|**Blending Settings**| When enabled, the edge will inherit properties and attributes from its `Start` and `End` point.<br>It uses {% include lk id='Interpolate' %} blending under the hood. |

| **Output**           ||
| **Edge Length** Attribute Name           | When enabled, the `length` of the edge will be written to the specified attribute.<br>*The length of an edge is the distance between its start and end point.* |

---
# Inputs & Outputs
## Vtx & Edges
See {% include lk id='Working with Graphs' %}