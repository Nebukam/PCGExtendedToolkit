---
layout: page
grand_parent: All Nodes
parent: Clusters
title: Write Vtx Properties
subtitle: Compute vtx extra data
color: white
summary: The **Write Vtx Extras** node ...
splash: icons/icon_edges-extras.svg
preview_img: docs/splash-edges-extras.png
toc_img: placeholder.jpg
has_children: true
tagged: 
    - node
    - edges
see_also: 
    - Interpolate
nav_order: 11
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

> DOC TDB
{: .warning }

---
## Extra Modules
<br>
{% include card_childs tagged='vtx-extras' %}