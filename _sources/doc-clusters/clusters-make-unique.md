---
layout: page
#grand_parent: All Nodes
parent: Clusters
title: Make Unique
name_in_editor: "Cluster : Make Unique"
subtitle: Forward clusters with a new unique pair of tags. It's like creating a copy, only much faster.
summary: The **Make Unique** node assigns new unique IDs to clusters without duplicating the data, enabling advanced operations like copying and modifying existing clusters before merging them with the original.
color: white
splash: icons/icon_graphs-sanitize.svg
tagged:
    - node
    - clusters
see_also:
    - Working with Clusters
nav_order: 60
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

The **Make Unique** node is a handy helper to slap a new pair of unique IDs to an existing cluster pair, without actually duplicating the data. This node primarily exists to allow certain advanced operations such as copying an existing cluster configuration, modify it and then fuse it with the original one.
{: .fs-5 .fw-400 } 

