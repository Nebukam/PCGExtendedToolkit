---
layout: page
#grand_parent: All Nodes
parent: Clusters
title: Mesh to Clusters
subtitle: Convert mesh/geometry topology to usable cluster clusters.
summary: The **Mesh To Cluster** ...
color: white
splash: icons/icon_graphs-sanitize.svg
preview_img: placeholder.jpg
toc_img: placeholder.jpg
tagged:
    - node
    - clusters
see_also:
    - Working with Clusters
nav_order: 2
inputs:
    -   name : In Targets
        desc : Target points to copy mesh cluster to.
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