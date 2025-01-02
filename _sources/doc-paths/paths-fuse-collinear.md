---
layout: page
family: Path
#grand_parent: All Nodes
parent: Paths
title: Fuse Collinear
name_in_editor: "Path : Fuse Collinear"
subtitle: Fuse collinear points on a path
color: white
summary: The **Fuse Collinear** node removes points that are collinear, with control over thresholds. It can also optionally fuse points based on their proximity.
splash: icons/icon_path-fuse.svg
tagged: 
    - node
    - paths
nav_order: 3
inputs:
    -   name : Paths
        desc : Paths which points will be checked for collinearity
        pin : points
    -   name : Filter
        extra_icon: OUT_Filters
        desc : Optional filters used to determine whether a point can be removed or not
        pin : params
outputs:
    -   name : Paths
        desc : Simplified paths
        pin : points
---

{% include header_card_node %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|:**Settings**||
| Threshold           | Threshold in degree under which the deviation is considered small enough to be collinear.  |
| Fuse Distance           | In addition to collinearity, this value allows to fuse points that are close enough. |