---
layout: page
parent: Paths
grand_parent: Nodes
title: Orient
subtitle: Orient points in relation to their neighbors
color: white
summary: The **Orient** node compute individual point transforms & orientation based on its next & previous neighbors.
splash: icons/icon_paths-orient.svg
preview_img: docs/splash-orienting.png
toc_img: placeholder.jpg
tagged: 
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
# Modules

## Available {% include lk id='Orienting' %} Modules
<br>
{% include card_any tagged="pathsorient" %}
