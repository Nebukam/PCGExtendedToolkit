---
layout: page
#grand_parent: All Nodes
parent: Paths
title: Write Tangents
subtitle: TBD
color: white
summary: The **Write Tangents** node computes and assign custom tangents to path points, with options for tangent math modules, scaling, and handling closed paths.
splash: icons/icon_path-tangents.svg
preview_img: previews/index-subdivide.png
has_children: true
tagged: 
    - node
    - paths
nav_order: 5
inputs:
    -   name : Paths
        desc : Paths which points will have tangents written on
        pin : points
    -   name : Point Filters
        desc : Filter input that let you choose which points get processed.
        pin : params
outputs:
    -   name : Paths
        desc : Paths with updated tangents attributes
        pin : points
---

{% include header_card_node %}

The **Write Tangents** help you compute & write custom tangents on your paths so you can easily build spline from them.
{: .fs-5 .fw-400 } 

{% include img a='placeholder-wide.jpg' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Closed Path           | If enabled, will process input paths as closed, effectively wrapping first and last point.  |
| Arrive Name<br>*FVector*           | Attribute to write the `Arrive` tangent to.  |
| Leave Name<br>*FVector*           | Attribute to write the `Leave` tangent to.  |

| **Tangents Modules**           ||
| Tangents           | Lets you pick which kind of tangent maths you want to apply to the input paths.<br>*See [Available Tangents Modules](#available-tangents-modules).*|
| Start Tangents           | Lets you pick which kind of tangent maths you want to use for the **first point** of the path.<br>*Will fall back to the default module if left empty.*|
| End Tangents           | Lets you pick which kind of tangent maths you want to use for the **last point** of the path.<br>*Will fall back to the default module if left empty.*|

| **Arrive Scale**           ||
| Arrive Scale Type           | Let you choose to scale the arrive tangents with either a `Constant` value or an `Attribute`.|
| Arrive Scale Attribute           | Attribute that will be used to scale the arrive tangent with.<br>*Will be broadcasted to FVector.* |
| Arrive Scale Constant           | Attribute that will be used to scale the arrive tangent with.<br>*Will be broadcasted to FVector.* |

| **Leave Scale**           ||
| Leave Scale Type           | Let you choose to scale the leave tangents with either a `Constant` value or an `Attribute`.|
| Leave Scale Attribute           | Attribute that will be used to scale the leave tangent with.<br>*Will be broadcasted to FVector.* |
| Leave Scale Constant           | Attribute that will be used to scale the leave tangent with.<br>*Will be broadcasted to FVector.* |

---
## Tangents modules
<br>
{% include card_any tagged="pathstangents" %}