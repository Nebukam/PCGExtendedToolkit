---
layout: page
family: ClusterGen
#grand_parent: All Nodes
parent: Clusters
title: Find point on Bounds
name_in_editor: "Cluster : Find point on Bounds"
subtitle: Find a point in each cluster that is the closest to that cluster' bounds.
summary: The **Find point on Bounds** node find the point inside either edge or vtx points that is closest to that cluster' bounds. This is especially useful to use as a seed to find outer contours of individual clusters.
color: white
splash: icons/icon_edges-bridge.svg
tagged: 
    - node
    - edges
nav_order: 100
see_also: 
    - Working with Clusters
inputs:
    -   name : Vtx
        desc : Endpoints of the input Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the input Vtxs
        pin : points
outputs:
    -   name : Out
        desc : Points found. One dataset per input cluster.
        pin : points
---

{% include header_card_node %}

WIP
{: .fs-5 .fw-400 } 

{% include img a='details/clusters-find-point-on-bounds/lead.png' %}

# Properties
<br>

WIP / TBD

---
## Cluster Output Settings
*See [Working with Clusters - Cluster Output Settings](/PCGExtendedToolkit/doc-general/working-with-clusters.html#cluster-output-settings).*