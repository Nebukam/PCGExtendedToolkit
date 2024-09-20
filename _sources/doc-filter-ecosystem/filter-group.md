---
layout: page
#grand_parent: Misc
parent: Filter Ecosystem
title: 🝖 AND / OR (Group)
name_in_editor: "Filter Group"
subtitle: Group multiple filters to set up complex AND/OR branches.
color: white
summary: TBD
splash: icons/icon_misc-sort-points.svg
tagged: 
    - node
    - filter
    - misc
nav_order: 5
outputs:
    -   name : Filters
        desc : Any number of filters, including groups.
        pin : params
outputs:
    -   name : Filter
        desc : A single filter definition
        pin : params
---

{% include header_card_node %}

The **Group Filter** node allows grouping & chaining filter conditions to create complex conditional branching; they behave like the BP `AND (boolean)` and `OR (boolean)` nodes.
{: .fs-5 .fw-400 } 

{% include img a='details/filter-ecosystem/filter-group-lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| **Settings**          ||
| Mode         | Defines how the group handle its filters. |
| Invert         | If enabled, inverts the result of the group. |

|: Mode      ||
| <span class="ebit">AND</span>           | Requires all the input filters to pass in order for the group to pass.<br>*Prioritize filters that are the most likely to fail in order to exit early.*  |
| <span class="ebit">OR</span>           | Only need a single input filter to pass in order for the group to pass.<br>*Prioritize filters that are the most likely to pass in order to exit early.* |
{: .enum }