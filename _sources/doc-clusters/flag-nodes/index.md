---
layout: page
#grand_parent: All Nodes
parent: Clusters
title: Flag Nodes
subtitle: Find conditional-based states to nodes within a graph
summary: The **Flag Nodes** node ...
color: white
splash: icons/icon_misc-write-index.svg
preview_img: placeholder.jpg
toc_img: placeholder.jpg
has_children: true
tagged: 
    - node
    - clusters
nav_order: 13
inputs:
    -   name : Vtx
        desc : Endpoints of the input Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the input Vtxs
        pin : points
    -   name : Flags
        desc : Node flags and their associated filters
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

> DOC TDB
{: .warning }