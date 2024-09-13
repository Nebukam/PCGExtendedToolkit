---
layout: page
#grand_parent: All Nodes
parent: Clusters
title: Find Clusters
subtitle: Find matching cluster data.
summary: The **Find Cluster** data is designed to find matching vtx/edge pairs, either as a whole, or in isolation. It is especially useful to loop over individual clusters.
color: white
splash: icons/icon_graphs-sanitize.svg
preview_img: placeholder.jpg
toc_img: placeholder.jpg
tagged:
    - node
    - clusters
see_also:
    - Working with Clusters
nav_order: 20
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