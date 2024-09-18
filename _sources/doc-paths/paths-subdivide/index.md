---
layout: page
#grand_parent: All Nodes
parent: Paths
title: Subdivide
subtitle: Create sub-points between existing points
color: white
summary: The **Subdivide Path** nodes create new point between existing ones on a path. Define closure behavior, choose a subdivide method (Distance or Count), and specify an amount. Opt for blending options to refine subpoints further.
splash: icons/icon_path-subdivide.svg
preview_img: previews/index-subdivide.png
tagged: 
    - node
    - paths
nav_order: 4
has_children: true
inputs:
    -   name : Paths
        desc : Paths which segments will be subdivided
        pin : points
outputs:
    -   name : Paths
        desc : Subdivided paths
        pin : points
---

{% include header_card_node %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Closed Path           | If enabled, will process input paths as closed, effectively wrapping first and last point.  |
| Subdivide Method      | Method to be used to define how many points are going to be inserted between existing ones.<br>See [Subdivide Method](#subdivide-method)   |
| Distance *or* Count      | Based on the method, specifies how many points will be created. |
| **Blending**           | This property lets you select which kind of blending you want to apply to the input paths.<br>*See [Available Blending Modules](#available-blending-modules).*|

## Subdivide Method

| Method       | Description          |
|:-------------|:------------------|
| **Distance**           | will create a new point every `X` units inside existing segments, as specified in the `Distance` property.<br>*Smaller values will create more points, larger values will create less points.*  |
| **Count**           | will create `X` new points for each existing segments, as specified in the `Count` property.  |

> `Distance` will create more uniform looking subdivisions, while `Count` is more predictable.

---
# Modules

## {% include lk id='Blending' %}
<br>
{% include card_any tagged="blending" %}