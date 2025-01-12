---
layout: page
family: SamplerNeighbor
grand_parent: All Nodes
parent: Clusters
title: "Edge Order"
name_in_editor: "Cluster : Edge Order"
subtitle: Fix the order of edge start & end points
summary: The **Edge Order** node fixes the endpoints order of edges, effectively turning clusters into directed graphs.
color: blue
splash: icons/icon_custom-graphs-promote-edges.svg
tagged: 
    - node
    - clusters
see_also: 
    - Working with Clusters
    - Interpolate
nav_order: 11
inputs:
    -   name : Vtx
        extra_icon: IN_Vtx
        desc : Endpoints of the input Edges
        pin : points
    -   name : Edges
        extra_icon: OUT_Edges
        desc : Edges associated with the input Vtxs
        pin : points
    -   name : Sorting Rules
        extra_icon: OUT_SortRule
        desc : Sorting rules when this mode is selected
        pin : params
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

The **Edge Order** node lets you fix the order of the start & end point on the edge. This enable to have full control with edge filters that work on endpoint comparison.
{: .fs-5 .fw-400 } 

{% include img a='details/clusters-edge-properties/lead.png' %}

# Properties
<br>

---
## Direction (Order)

{% include embed id='settings-edge-direction' %}
{% include img a='explainers/edge-direction-method.png' %}