---
layout: page
#grand_parent: All Nodes
parent: Filter Ecosystem
title: Uber Filter (Collection)
name_in_editor: Uber Filter (Collection)
subtitle: Combine multiple filters
summary: The **Uber Filter** node is a one-stop node for all your filtering needs.
color: white
splash: icons/icon_misc-sort-points.svg
#has_children: true
tagged: 
    - node
    - filter-ecosystem
nav_order: 1
inputs:
    -   name : In
        desc : Points to be filtered
        pin : points
    -   name : Filters
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

# Properties
<br>

> DOC TDB
{: .warning }

{% include img a='details/filter-ecosystem/filter-uber-filter-collection-lead.png' %}

---
## Available Filters
<br>
{% include card_childs tagged='filter' %}