---
layout: page
#grand_parent: All Nodes
parent: Sampling
title: Sample Neighbors
subtitle: Sample points based on edge connections
color: white
#summary: The **Sample Nearest Points** node explore points within a range using various methods. Define sampling range, weight targets, and obtain useful attributes.
splash: icons/icon_sampling-point.svg
preview_img: placeholder.jpg
toc_img: placeholder.jpg
tagged: 
    - node
    - sampling
nav_order: 4
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

# Properties
<br>

> DOC TDB
{: .warning }