---
layout: page
family: Path
#grand_parent: All Nodes
parent: Paths
title: Subdivide
name_in_editor: "Path : Subdivide"
subtitle: Create sub-points between existing points
color: white
summary: The **Subdivide Path** nodes create new point between existing ones on a path. Define closure behavior, choose a subdivide method (Distance or Count), and specify an amount. Opt for blending options to refine subpoints further.
splash: icons/icon_path-subdivide.svg
tagged: 
    - node
    - paths
nav_order: 4
inputs:
    -   name : Paths
        desc : Paths which segments will be subdivided
        pin : points
    -   name : Point Filters
        desc : Filter which segments will be subdivided.
        pin : params
outputs:
    -   name : Paths
        desc : Subdivided paths
        pin : points        
---

{% include header_card_node %}

# Properties
<br>

{% include embed id='settings-index-safety' %}

|**Settings**||
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
## Available Blending Modules
<br>
{% include card_any reference='Blend' tagged='blending' %}