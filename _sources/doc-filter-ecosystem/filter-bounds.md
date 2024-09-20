---
layout: page
#grand_parent: Misc
parent: Filter Ecosystem
title: 🝖 Bounds
name_in_editor: "Filter : Bounds"
subtitle: Checks if a point is inside or outside the provided bounds, with options for bounds types and an epsilon adjustment.
color: white
summary: TBD
splash: icons/icon_misc-sort-points.svg
tagged: 
    - node
    - filter
    - misc
nav_order: 21
inputs:
    -   name : Bounds
        desc : Set of points which bounds will be checked against.
        pin : points
outputs:
    -   name : Filter
        desc : A single filter definition
        pin : params
---

{% include header_card_node %}

The **Bounds Filter** checks whether the test points are inside the provided bounds.
{: .fs-5 .fw-400 } 


{% include img a='details/filter-ecosystem/filter-bounds-lead.png' %}

---
# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| Bounds Source          | Lets you pick which point' bounds will be used for the Bounds inputs.<br>-`Scaled Bounds` is closest to what you see using a `Debug` node.<br>-`Density Bounds` are the same as `Scaled Bounds` but factor in the local density of the point.<br>-`Bounds` uses unscaled bounds. |
| Check if Inside | If enabled, the filter **passes if the tested point is inside the provided bounds**; otherwise it passes if the tested point is **outside**.  |
| Inside Epsilon | Lets you set a small value that will be used to expand the bounds in order to rule out perfect surface overlaps. |