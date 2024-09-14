---
layout: page
#grand_parent: All Nodes
parent: Clusters
title: Fuse Clusters
subtitle: Fuse clusters together by finding Point/Edge and Edge/Edge intersections.
summary: The **Fuse Clusters** node ...
color: white
splash: icons/icon_graphs-sanitize.svg
tagged:
    - node
    - clusters
see_also:
    - Working with Clusters
nav_order: 21
has_children: false
inputs:
    -   name : Vtx
        desc : Endpoints of the input Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the input Vtxs
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

# Properties
<br>

> Current implementation is **WIP**: not all attributes from the inputs are not forwarded to the output cluster.
{: .error }