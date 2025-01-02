---
layout: page
family: FilterHub
#grand_parent: All Nodes
parent: Filter Ecosystem
title: Uber Filter
name_in_editor: Uber Filter
subtitle: Combine multiple filters
summary: The **Uber Filter** node combines multiple filters to refine points within a dataset. You can either split the dataset based on filters results or write the result to an attribute.
color: white
splash: icons/icon_misc-sort-points.svg
#has_children: true
tagged: 
    - node
    - filter-ecosystem
nav_order: 0
inputs:
    -   name : In
        desc : Points to be filtered
        pin : points
    -   name : Filter
        extra_icon: OUT_Filters
        desc : Filters to be evaluated
        pin : params
outputs:
    -   name : Inside
        desc : Points that passed the filters
        pin : points
    -   name : Outside
        desc : Points that didn't pass the filters.
        pin : points
---

{% include header_card_node %}

The **Uber Filter** node leverages any and all the filters available as part of the ecosystem to filter out points inside individual datasets. Alternatively, you can choose to only write the combined filters result as a `bool` attribute.
{: .fs-5 .fw-400 } 

{% include img a='details/filter-ecosystem/filter-uber-filter-lead.png' %}

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