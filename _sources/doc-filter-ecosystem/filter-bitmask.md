---
layout: page
#grand_parent: Misc
parent: Filter Ecosystem
title: 🝖 Bitmask
name_in_editor: "Filter : Bitmask"
subtitle: The **Bitmask Filter** compares a bitmask against another
color: white
summary: TBD
splash: icons/icon_misc-sort-points.svg
tagged: 
    - node
    - filter
    - misc
nav_order: 22
outputs:
    -   name : Filter
        desc : A single filter definition
        pin : params
---

{% include header_card_node %}

The **Bitmask Filter** lets you check whether certain flags are set or not in an `int64` bitmask attribute.
{: .fs-5 .fw-400 } 

{% include img a='details/filter-ecosystem/filter-bitmask-lead.png' %}

---
# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| Flags Attributes          | The is the name of the attribtue which value will be tested.<br>*It is expected to be an `int64`.* |
| Comparison | If enabled, output the averaged normal of the `vtx` based on all connected `edges`.<br>*This output is hardly usable for highly 3-dimensional nodes.* |
| Min Point Count | This lets you filter out output paths that have less that the specified number of points. |
| Max Point Count | If enabled, this lets you filter out output paths that have more that the specified number of points. |