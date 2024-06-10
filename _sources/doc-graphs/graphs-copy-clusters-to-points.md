---
layout: page
#grand_parent: All Nodes
parent: Graphs
title: Copy Clusters to Points
subtitle: Creates copies of entire clusters to target points; much like Copy Points does.
color: white
summary: The **Copy Cluster to Points** ...
splash: icons/icon_graphs-sanitize.svg
preview_img: placeholder.jpg
toc_img: placeholder.jpg
tagged:
    - node
    - graphs
see_also:
    - Working with Graphs
nav_order: 11
inputs:
    -   name : Vtx
        desc : Endpoints of the input Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the input Vtxs
        pin : points
    -   name : In Targets
        desc : Target points to copy input clusters to.
        pin : point
outputs:
    -   name : Vtx
        desc : Endpoints of the output Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the output Vtxs
        pin : points
---

> DOC TDB
{: .warning }