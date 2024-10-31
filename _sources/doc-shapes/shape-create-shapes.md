---
layout: page
family: Shapes
#grand_parent: All Nodes
parent: Shape Ecosystem
title: Create Shapes
name_in_editor: Create Shapes
subtitle: Create parametric shapes out of seed points
summary: The **Create Shapes** node takes any number of Shape Builders as input and turn input seeds into those shapes.
color: white
splash: icons/icon_misc-fuse-points.svg
#has_children: true
tagged: 
    - node
    - shape-ecosystem
nav_order: 0
inputs:
    -   name : Seeds
        desc : Seed points for shape builders
        pin : points
    -   name : Shape builders
        desc : Individual shape builder
        pin : params
outputs:
    -   name : Shapes
        desc : Shape points
        pin : points
---

{% include header_card_node %}

WIP
{: .fs-5 .fw-400 } 

{% include img a='details/shapes/shape-create-shapes-lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| **Settings**          ||
| Mode         | How to ouput data.<br>*See below.*|
| Result Attribute Name<br>`bool`         | Name of the attribute the filter result will be written to. |
| Swap         | If enabled, inverts the combined result of the filters. |

|: Mode      ||
| <span class="ebit">Partition</span>           | Split input dataset in either `Inside` (filter passed) or `Outside` (filters failed) outputs.  |
| <span class="ebit">Write</span>           | Preserve input and write the result of the filter to an attribute. |
{: .enum }

---
## Available Filters
<br>
{% include card_childs reference='Filter Ecosystem' tagged='filter' %}