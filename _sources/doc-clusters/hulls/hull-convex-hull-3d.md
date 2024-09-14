---
layout: page
grand_parent: Clusters
parent: Hulls
title: Convex Hull 3D
subtitle: Outputs a 3D convex hull.
summary: The **Convex Hull 3D** outputs the edges/graph of a 3D convex hull. Prune points to exclude non-hull vertices. Specify attributes and projection settings for customization.
color: blue
splash: icons/icon_graphs-convex-hull.svg
tagged: 
    - node
    - clusters
nav_order: 1
see_also:
    - Working with Clusters
inputs:
    -   name : In
        desc : Points clouds that will be triangulated
        pin : points
outputs:
    -   name : Vtx
        desc : Endpoints of the output Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the output Vtxs
        pin : points
---

{% include header_card_node %}

The 3D Convex Hull node capture the convex bounding shape of a set of point in three dimensions.
{: .fs-5 .fw-400 } 

{% include img a='placeholder-wide.jpg' %}

# Properties
<br>

---
## Cluster Output Settings
*See [Working with Clusters](/PCGExtendedToolkit/doc-general/working-with-clusters.html).*
<br>
{% include embed id='settings-cluster-output' %}
