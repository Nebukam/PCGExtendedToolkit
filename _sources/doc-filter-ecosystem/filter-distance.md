---
layout: page
family: Filter
#grand_parent: Misc
parent: Filter Ecosystem
title: 🝖 Distance
name_in_editor: "Filter : Distance"
subtitle: Compare distance to closest target against a constant or attribute.
color: param
summary: "-"
splash: icons/icon_misc-sort-points.svg
tagged: 
    - node
    - filter
    - pointfilter
    - misc
nav_order: 21
inputs:
    -   name : Targets
        desc : Set of points which bounds will be checked against.
        pin : points
outputs:
    -   name : Filter
        extra_icon: OUT_Filter
        desc : A single filter definition
        pin : params
---

{% include header_card_node %}

The **Distance Filter** compares the distance to the closest target against a constant or attribute.
{: .fs-5 .fw-400 } 


{% include img a='details/filter-ecosystem/filter-distance-lead.png' %}

---
# Properties
<br>

TBD