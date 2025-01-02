---
layout: page
family: ClusterGen
#grand_parent: All Nodes
parent: Clusters
title: Subdivide Edges
name_in_editor: "Cluster : Subdivide Edges"
subtitle: Subdivides clusters' edges.
summary: The **Subdivide Edges** node subdivides clusters edges by inserting new nodes between each edge' endpoints.
color: white
splash: icons/icon_edges-bridge.svg
tagged: 
    - node
    - clusters
nav_order: 100
see_also: 
    - Working with Clusters
inputs:
    -   name : Vtx
        extra_icon: IN_Vtx
        desc : Endpoints of the input Edges
        pin : points
    -   name : Edges
        extra_icon: OUT_Edges
        desc : Edges associated with the input Vtxs
        pin : points
    -   name : Edge Filter
        extra_icon: OUT_FilterEdgess
        extra_icon: OUT_FilterEdges
        desc : Filters to drive which edges should be subdivided
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

WIP
{: .fs-5 .fw-400 } 

{% include img a='details/clusters-subdivide-edges/lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| Bridge Method           | The method that will be used to identify and create bridges between clusters.|

> Note that no matter what method is selected, **a bridge will always connect the two closest points between two clusters.**  The chosen method only drives which cluster is connected to which other cluster.
{: .comment }

---
## Cluster Output Settings
*See [Working with Clusters - Cluster Output Settings](/PCGExtendedToolkit/doc-general/working-with-clusters.html#cluster-output-settings).*