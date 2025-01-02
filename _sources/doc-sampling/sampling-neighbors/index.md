---
layout: page
family: Sampler
#grand_parent: All Nodes
parent: Sampling
title: Sample Neighbors
name_in_editor: "Cluster : Sample Neighbors"
subtitle: Sample points based on edge connections
color: white
summary: The **Sample Neighbors** sample data from connected neighbors inside a cluster.
splash: icons/icon_sampling-point.svg
preview_img: previews/index-sampling-neighbors.png
has_children: true
tagged: 
    - node
    - sampling
nav_order: 10
inputs:
    -   name : Vtx
        extra_icon: IN_Vtx
        desc : Endpoints of the input Edges
        pin : points
    -   name : Edges
        extra_icon: OUT_Edges
        desc : Edges associated with the input Vtxs
        pin : points
outputs:
    -   name : Vtx
        extra_icon: IN_Vtx
        desc : Endpoints of the output Edges
        pin : points
    -   name : Edges
        extra_icon: OUT_Edges
        desc : Edges associated with the output Vtxs
        pin : points
---

{% include header_card_node %}

# Properties
<br>

> DOC TDB
{: .warning }