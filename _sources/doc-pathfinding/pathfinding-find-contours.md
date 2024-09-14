---
layout: page
#grand_parent: All Nodes
parent: Pathfinding
title: Find Contours
subtitle: Find edge contours & outlines
summary: The **Find Contours** node finds hole/outlines contours in a graph, using points as proximity seeds.
color: white
splash: icons/icon_edges-extras.svg
tagged: 
    - node
    - edges
nav_order: 4
inputs:
    -   name : Vtx
        desc : Endpoints of the input Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the input Vtxs
        pin : points
    -   name : Seed
        desc : Seed points used to find contours based on proximity
        pin : points
outputs:
    -   name : Paths
        desc : One or multiple paths per seed points
        pin : points
---

{% include header_card_node %}

# Properties
<br>

> DOC TDB
{: .warning }

---
## Projection Settings
<br>
{% include embed id='settings-projection' %}
