---
layout: page
#grand_parent: All Nodes
parent: Paths
title: Write Tangents
subtitle: Subtitle
color: white
#summary: summary_goes_here
splash: icons/icon_path-tangents.svg
preview_img: docs/splash-tangents.png
has_children: true
tagged: 
    - node
    - paths
nav_order: 5
inputs:
    -   name : Paths
        desc : Paths which points will have tangents written on
        pin : points
outputs:
    -   name : Paths
        desc : Paths with updated tangents attributes
        pin : points
---

{% include header_card_node %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Closed Path           | If enabled, will process input paths as closed, effectively wrapping last and first point.  |
| Arrive Name           | Attribute to write the `Arrive` tangent to.  |
| Leave Name           | Attribute to write the `Leave` tangent to.  |
| **Tangents**           | This property lets you select which kind of tangent maths you want to apply to the input paths.<br>*See [Available Tangents Modules](#available-tangents-modules).*|

> The name used for `Arrive` & `Leave` should be used as custom tangent attributes when using the `Create Spline` PCG Node.

---
## Tangents modules
<br>
{% include card_any tagged="pathstangents" %}