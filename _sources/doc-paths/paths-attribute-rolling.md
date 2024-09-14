---
layout: page
#grand_parent: All Nodes
parent: Paths
title: Attribute Rolling
subtitle: Does a rolling operation over properties & attributes along a path
color: white
summary: The **Attribute Rolling** nodes allows to do "rolling" operations, where the base value of each operation is the result of the previous one.
splash: icons/icon_path-fuse.svg
tagged: 
    - node
    - paths
nav_order: 3
inputs:
    -   name : Paths
        desc : Paths which points will be bevelled
        pin : points
    -   name : Trigger Conditions
        desc : Optional filters used to determine whether a point will be bevelled or not
        pin : params
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