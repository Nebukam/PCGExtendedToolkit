---
layout: page
grand_parent: Clusters
parent: Packing Clusters
title: Unpack Clusters
name_in_editor: "Cluster : Unpack"
subtitle: Unpack Clusters
summary: The **Unpack Clusters** node restores individual vertex-edge pairs from packed data, making clusters usable again after being processed by the Pack Clusters node.
color: blue
splash: icons/icon_graphs-convex-hull.svg
preview_img: placeholder.jpg
tagged: 
    - node
    - clusters
nav_order: 201
see_also:
    - Working with Clusters
inputs:
    -   name : Packed Clusters
        desc : Individually packed vtx/edges pairs
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

The **Unpack Cluster** nodes recreates clusters' `Vtx` and `Edges` pairs from packed point data created by the {% include lk id='Pack Clusters' %} node.
{: .fs-5 .fw-400 } 

... and that's it. That's the node.

{% include img a='placeholder-wide.jpg' %}
