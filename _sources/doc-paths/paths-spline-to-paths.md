---
layout: page
family: Path
#grand_parent: All Nodes
parent: Paths
title: Spline to Path
name_in_editor: "Spline to Path"
subtitle: Converts a spline into a path.
color: white
summary: The **Spline to Path** node converts a raw spline into a 1:1 path. It is primarily designed to enable direct match of splines to spline meshes, without the overhead of using a spline sampler.
splash: icons/icon_sampling-guided.svg
tagged: 
    - node
    - paths
nav_order: 3
inputs:
    -   name : Splines
        desc : Input splines to be converted to paths
        pin : splines
outputs:
    -   name : Paths
        desc : Output paths
        pin : points
---

{% include header_card_node %}

# Properties
<br>

> DOC TDB
{: .warning }