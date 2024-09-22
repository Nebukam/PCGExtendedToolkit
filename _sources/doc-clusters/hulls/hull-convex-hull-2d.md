---
layout: page
family: ClusterGen
grand_parent: Clusters
parent: Hulls
title: Convex Hull 2D
subtitle: Outputs a 2D convex hull.
summary: The **Convex Hull 2D** outputs the edges/graph of a 2D convex hull. Prune points to exclude non-hull vertices. Specify attributes and projection settings for customization.
color: white
splash: icons/icon_graphs-convex-hull.svg
tagged: 
    - node
    - clusters
nav_order: 4
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
    -   name : Paths
        desc : Hull points ordered as a path
        pin : points
---

{% include header_card_node %}

The 2D Convex Hull node capture the convex bounding shape of a set of point in two dimensions.  
Under-the-hood the points are first projected on a plane, and the hull is deducted from that projection.
{: .fs-5 .fw-400 } 

The node outputs a single cluster that contains the unordered hull & its edges, as well as an actual "canon" path, whose points are ordered in clockwise order.

{% include img a='details/hulls/hull-convex-hull-2d-lead.png' %}

# Properties
<br>

---
## Projection Settings
<br>
{% include embed id='settings-projection' %}

---
## Cluster Output Settings
*See [Working with Clusters - Cluster Output Settings](/PCGExtendedToolkit/doc-general/working-with-clusters.html#cluster-output-settings).*

