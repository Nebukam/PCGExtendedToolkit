---
layout: page
#grand_parent: All Nodes
parent: Clusters
title: Copy Cluster to Points
name_in_editor: "Cluster : Copy to Points"
subtitle: Creates copies of entire clusters to target points; much like Copy Points does.
summary: The **Copy Cluster to Points** node duplicates entire clusters at specified target points, applying transformations like rotation and scale from the target points, similar to the Copy Points node but for clusters.
color: white
splash: icons/icon_graphs-sanitize.svg
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

{% include header_card_node %}

The **Copy Clusters to points** is very much like the vanilla `Copy Points` node, but for clusters: it will create transformed copies of any input cluster at the target points' location, accounting for rotation and scale.
{: .fs-5 .fw-400 } 

>This comes handy when you make use of {% include lk id='Packing Clusters' %} to store Clusters into data assets and want to "place" them somewhere specific.
{: .infos }

>Unlike `Copy Points`, this node outputs individual per-cluster datasets.
{: .comment }

{% include img a='placeholder-wide.jpg' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| **Transform Details**  | |
| Inherit Scale          | If enabled, copied points will be scaled by the target' scale. |
| Inherit Rotation          | If enabled, copied points will be scaled by the target' rotation. |
