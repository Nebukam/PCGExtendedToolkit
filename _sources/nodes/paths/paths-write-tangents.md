---
layout: page
parent: Paths
grand_parent: All Nodes
title: Write Tangents
subtitle: Subtitle
color: white
#summary: summary_goes_here
splash: icons/icon_path-tangents.svg
preview_img: docs/splash-tangents.png
toc_img: placeholder.jpg
tagged: 
    - paths
nav_order: 5
---

{% include header_card_node %}

{% include img a='details/details-write-tangents.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Closed Path           | If enabled, will process input paths as closed, effectively wrapping last and first point.  |
| Arrive Name           | Attribute to write the `Arrive` tangent to.  |
| Leave Name           | Attribute to write the `Leave` tangent to.  |
| **Tangents**           | This property lets you select which kind of tangent maths you want to apply to the input paths.<br>*See [Available Tangents Modules](#available-tangents-modules).*|

> The name used for `Arrive` & `Leave` should be used as custom tangent attributes when using the `Create Spline` PCG Node.

---
# Modules

## Available {% include lk id='Tangents' %} Modules
<br>
{% include card_any tagged="pathstangents" %}

---
# Inputs
## In
Any number of point datasets assumed to be paths.

---
# Outputs
## Out
Same as Inputs with the added metadata.  
*Reminder that empty inputs will be ignored & pruned*.