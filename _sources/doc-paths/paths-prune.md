---
layout: page
#grand_parent: All Nodes
parent: Paths
title: Prune Path
subtitle: Special path pruning
color: white
summary: The **Prune Path** nodes prune paths points based on a toggle/switch condition
splash: icons/icon_path-fuse.svg
preview_img: docs/splash-path-collinear.png
toc_img: placeholder.jpg
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