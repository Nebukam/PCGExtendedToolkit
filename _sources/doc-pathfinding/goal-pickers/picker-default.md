---
layout: page
grand_parent: Pathfinding
parent: Goal Pickers
title: Default
subtitle: Match seeds to goals at the same index.
summary: The **Default** picker ...
color: white
splash: icons/icon_placement-center.svg
preview_img: docs/splash-picker-default.png
toc_img: placeholder.jpg
tagged: 
    - goalpicker
nav_order: 1
---

{% include header_card_node %}

The default goal picker attempts to match input `Seeds` and `Goals` in a 1:1 fashion.  
Seed index `0` will be matched to goal index `0`, and so on.  

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Index Safety           | Failsafe method if there are more `Seeds` than there are `Goals`.<br>Note that extra `Goals` are simply ignored.<br>*See [Index Safety](#index-safety)* |

{% include embed id='settings-index-safety' %}