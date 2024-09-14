---
layout: page
#grand_parent: All Nodes
parent: Misc
title: Uber Filter
subtitle: Combine multiple filters
summary: The **Uber Filter** node is a one-stop node for all your filtering needs.
color: white
splash: icons/icon_misc-write-index.svg
preview_img: previews/index-uber-filter.png
has_children: true
tagged: 
    - node
    - misc
nav_order: 7
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

---
## Available Filters
<br>
{% include card_childs tagged='filter' %}