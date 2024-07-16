---
layout: page
#grand_parent: All Nodes
parent: Clusters
title: Connect Points
subtitle: Connect points according to user-defined probes.
summary: The **Connect Points** node ...
color: white
splash: icons/icon_custom-graphs-build.svg
preview_img: placeholder.jpg
toc_img: placeholder.jpg
has_children: true
tagged: 
    - clusters
    - node
nav_order: 0
use_child_thumbnails: true
inputs:
    -   name : Points
        desc : Points that will be connected togethers
        pin : points
    -   name : Probes
        desc : Probes used to build connections
        pin : params
    -   name : Generators
        desc : Filters used to determine which points are allowed to generate connections
        pin : params
    -   name : Connectables
        desc : Filters used to determine which points are allowed to receive connections
        pin : params
outputs:
    -   name : Vtx
        desc : Endpoints of the output Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the output Vtxs
        pin : points
---

{% include header_card_node %}

> DOC TDB
{: .warning }

---
## Probes
<br>
{% include card_childs tagged='probe' %}