---
layout: page
#grand_parent: All Nodes
parent: Paths
title: Orient
subtitle: Orient points in relation to their neighbors
color: white
summary: The **Orient** node compute individual point transforms & orientation based on its next & previous neighbors.
splash: icons/icon_paths-orient.svg
preview_img: docs/splash-orienting.png
toc_img: placeholder.jpg
has_children: true
tagged: 
    - node
    - paths
nav_order: 2
---

{% include header_card_node %}

{% include img a='details/details-orient.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Closed Path           | If enabled, will process input paths as closed, effectively wrapping last and first point.  |
| **Orientation**           | This property lets you select which kind of orientation arithmetics you want to apply to the input paths.<br>*See [Available Orientation Modules](#available-orienting-modules).*|

---
## Orientation modules
<br>
{% include card_any tagged="pathsorient" %}

---
# Inputs
## In
Any number of point datasets assumed to be paths.

---
# Outputs
## Out
Same as Inputs with the transformation applied.  
*Reminder that empty inputs will be ignored & pruned*.