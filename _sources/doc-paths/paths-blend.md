---
layout: page
#grand_parent: All Nodes
parent: Paths
title: Blend
subtitle: Blend individual points between from paths' start and end points.
summary: The **Path Blend** node...
color: white
splash: icons/icon_paths-orient.svg
tagged: 
    - node
    - paths
nav_order: 2
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

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Closed Path           | If enabled, will process input paths as closed, effectively wrapping last and first point.  |
| **Blending**           | This property lets you select which kind of blending you want to apply to the input paths.<br>*See [Available Blending Modules](#available-blending-modules).*|

---
# Modules

## Available {% include lk id='Blending' %} Modules
<br>
{% include card_any tagged="blending" %}
