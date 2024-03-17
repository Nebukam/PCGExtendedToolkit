---
layout: page
parent: Paths
#grand_parent: All Nodes
title: Blend
subtitle: Blend individual points between from paths' start and end points.
color: white
#summary: summary_goes_here
splash: icons/icon_paths-orient.svg
preview_img: docs/splash-orienting.png
toc_img: placeholder.jpg
tagged: 
    - node
    - paths
nav_order: 2
---

{% include header_card_node %}

{% include img a='details/details-blend.png' %} 

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

---
# Inputs
## In
Any number of point datasets assumed to be paths.

---
# Outputs
## Out
Same as Inputs with the transformation applied.  
*Reminder that empty inputs will be ignored & pruned*.
