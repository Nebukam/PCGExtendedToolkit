---
layout: page
#grand_parent: All Nodes
parent: Paths
title: Write Paths Properties
subtitle: Compute extra path informations
color: white
summary: Writes a variety of path statistics such as length, time, normals, centroids etc
splash: icons/icon_edges-extras.svg
preview_img: placeholder.jpg
toc_img: placeholder.jpg
tagged: 
    - node
    - paths
nav_order: 4
inputs:
    -   name : Paths
        desc : Paths to compute information from
        pin : points
outputs:
    -   name : Paths
        desc : Paths with extra information
        pin : points
---

{% include header_card_node %}

# Properties
<br>

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
See {% include lk id='Working with Clusters' %}