---
layout: page
#grand_parent: All Nodes
parent: Graphs
title: Mesh to Clusters
subtitle: Convert mesh/geometry topology to usable graph clusters.
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
nav_order: 3
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