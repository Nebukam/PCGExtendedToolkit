---
layout: page
family: Path
#grand_parent: All Nodes
parent: Paths
title: Orient
name_in_editor: "Path : Orient"
subtitle: Orient points in relation to their neighbors
color: white
summary: The **Orient** node compute individual point transforms & orientation based on its next & previous neighbors.
splash: icons/icon_paths-orient.svg
preview_img: previews/index-orient.png
has_children: true
tagged: 
    - node
    - paths
nav_order: 2
inputs:
    -   name : Paths
        desc : Paths which points will be oriented
        pin : points
outputs:
    -   name : Paths
        desc : Paths with updated points orientation
        pin : points
---

{% include header_card_node %}

# Properties
<br>

{% include embed id='settings-index-safety' %}

|**Settings**||
| **Orientation**           | This property lets you select which kind of orientation arithmetics you want to apply to the input paths.<br>*See [Available Orientation Modules](#available-orienting-modules).*|

---
## Orientation modules
<br>
{% include card_any tagged="pathsorient" %}