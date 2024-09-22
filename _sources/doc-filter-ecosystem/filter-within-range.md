---
layout: page
family: Filter
#grand_parent: Misc
parent: Filter Ecosystem
title: 🝖 Within Range
name_in_editor: "Filter : Within Range"
subtitle: Checks if an attribute value falls within a specified range.
color: white
summary: "-"
splash: icons/icon_misc-sort-points.svg
tagged: 
    - node
    - filter
    - misc
nav_order: 15
outputs:
    -   name : Filter
        desc : A single filter definition
        pin : params
---

{% include header_card_node %}

The **Within Range** filter checks if an attribute value is within a specified constant range.
{: .fs-5 .fw-400 } 

{% include img a='details/filter-ecosystem/filter-within-range-lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| **Operand A**          ||
| Operand A          | The attribute that will be checked. |

| **Range**          ||
| Range Min | Start of the range. |
| Range Max | End of the range. |
| Inclusive | If enabled, values that are equal to the range' min/max values will be considered *within range*. |
| Invert         | If enabled, inverts the result of the filter, effectively making the filter an "Outside Range" check. |
