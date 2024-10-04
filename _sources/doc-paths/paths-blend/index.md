---
layout: page
family: Path
#grand_parent: All Nodes
parent: Paths
title: Blend
name_in_editor: "Path : Blend"
subtitle: Blend individual points between from paths' start and end points.
summary: The **Path Blend** node...
color: white
splash: icons/icon_paths-orient.svg
preview_img: previews/paths-blend.png
tagged: 
    - node
    - paths
nav_order: 2
has_children: true
inputs:
    -   name : Paths
        desc : Paths which points attributes & properties will be blended
        pin : points
outputs:
    -   name : Paths
        desc : Paths with attributes & properties blended
        pin : points
---

{% include header_card_node %}

# Properties
<br> 

{% include embed id='settings-index-safety' %}

|**Settings**||
| **Blending**           | This property lets you select which kind of blending you want to apply to the input paths.<br>*See [Available Blending Modules](#available-blending-modules).*|

---
## Available Blending Modules
<br>
{% include card_childs tagged="blending" %}