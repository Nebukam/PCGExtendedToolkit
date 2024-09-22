---
layout: page
family: Path
#grand_parent: All Nodes
parent: Paths
title: Bevel
name_in_editor: "Path : Bevel"
subtitle: Bevel path points
color: white
summary: The **Fuse Collinear** node removes points that are collinear, with control over thresholds. It can also optionally fuse points based on their proximity.
splash: icons/icon_path-fuse.svg
tagged: 
    - node
    - paths
nav_order: 3
inputs:
    -   name : Paths
        desc : Paths which points will be bevelled
        pin : points
    -   name : Bevel Conditions
        desc : Optional filters used to determine whether a point will be bevelled or not
        pin : params
    -   name : Profile
        desc : Custom profile path
        pin : points
outputs:
    -   name : Paths
        desc : Bevelled paths
        pin : points
---

{% include header_card_node %}

# Properties
<br>

> DOC TDB
{: .warning }