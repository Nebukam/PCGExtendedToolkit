---
layout: page
#grand_parent: All Nodes
parent: Graphs
title: Filter Clusters
subtitle: Filter out cluster based on proximity to target points
color: white
summary: The **Fuse Clusters** node ...
splash: icons/icon_graphs-sanitize.svg
preview_img: placeholder.jpg
toc_img: placeholder.jpg
tagged:
    - node
    - graphs
see_also:
    - Working with Graphs
nav_order: 10
inputs:
    -   name : Vtx
        desc : Endpoints of the input Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the input Vtxs
        pin : points
    -   name : In Targets
        desc : Target points used to test for proximity
        pin : point
outputs:
    -   name : Vtx
        desc : Endpoints of the output Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the output Vtxs
        pin : points
---

{% include header_card_node %}

> DOC TDB
{: .warning }