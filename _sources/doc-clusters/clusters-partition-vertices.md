---
layout: page
family: Cluster
#grand_parent: All Nodes
parent: Clusters
title: Partition Vertices
name_in_editor: "Cluster : Partition Vtx"
subtitle: Create per-cluster Vtx datasets
summary: The **Partition Vertices** splits input vtx into separate output groups, so that each Edge dataset is associated to a unique Vtx dataset (as opposed to a shared Vtx dataset for multiple edge groups)
color: white
splash: icons/icon_misc-partition-by-values.svg
tagged:
    - node
    - clusters
see_also:
    - Working with Clusters
nav_order: 50
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

The **Partition Vertices** node converts processes the input clusters and outputs matching pairs of dataset for each cluster. This guarantees that each `Edges` have a single, **unshared** `Vtx` dataset.
{: .fs-5 .fw-400 } 

{% include img a='details/clusters-partition-vertices/lead.png' %}

> Contrary to other edge & cluster processors, this node does **not** produce a sanitized result.  
> *If the input is unsanitized, you may have unexpected results.*  
{: .warning }

This node primarily exists to allow certain advanced operations such as easily finding individual convex hull of isolated clusters.  
*This is not a default behavior as doing so can lead to greater cluster processing times.*