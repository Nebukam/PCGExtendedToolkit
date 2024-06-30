---
layout: page
#grand_parent: All Nodes
parent: Clusters
title: Pick Closest
summary: The **Pick Closest Clusters** node filters out input clusters based on proximity to target points.
color: white
splash: icons/icon_graphs-sanitize.svg
preview_img: placeholder.jpg
toc_img: placeholder.jpg
tagged:
    - node
    - graphs
see_also:
    - Working with Clusters
nav_order: 30
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