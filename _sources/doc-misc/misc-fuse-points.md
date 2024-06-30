---
layout: page
#grand_parent: All Nodes
parent: Misc
title: Fuse Points
subtitle: Proximity-based point pruning and blending.
summary: The **Fuse Points** reduces the number of point within a group by merging points that are within a set radius of each others; and allows you to control how the resulting properties and attributes are blended.
color: white
splash: icons/icon_misc-fuse-points.svg
preview_img: docs/splash-point-blending.png
toc_img: placeholder.jpg
tagged: 
    - node
    - misc
nav_order: 3
---

{% include header_card_node %}

{% include img a='docs/fuse/image.png' %} 
{% include img a='details/details-fuse-points.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Component Wise Radius           | Whether to use a component-wise radius or a spherical one.<br>When component-wise is enabled, distance is checked individually on `X`, `Y` and `Z` axis in world-space.  |
| Radius          | Radius within which multiple points are to be fused into a single one. |
| Preserve Order          | If enabled, fused points will be sorted to maintain their original order. |
|**Blending Settings**| Control how removed points' properties and attributes are blended into the point they are fused to.<br>See {% include lk id='Blending' %}|

---
# Inputs
## In
Any number of point datasets.

---
# Outputs
## Out
A processed point dataset for each input dataset.  
*Reminder that empty inputs will be ignored & pruned*.