---
layout: page
grand_parent: Paths
parent: Orient
title: ⋋ Weighted
subtitle: Distance-weighted interpolation
color: blue
#summary: summary_goes_here
splash: icons/icon_paths-orient.svg
tagged: 
    - pathsorient
nav_order: 1
---

{% include header_card_node %}

Weighted orientation balances the orientation of the point between the previous and next point based on perpendicular distance.
{% include img a='docs/orient/weight.png' %}   

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Inverse Weight           | Reverse the orientation weight.<br>*Weight to next will be used for previous, and vice-versa.* |  

|**Orienting**||
| Orient Axis           | The transform' axis that will be *oriented*. |
| Up Axis           | The Up axis used for the orientation maths. |
