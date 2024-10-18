---
layout: page
family: Filter
#grand_parent: Misc
parent: Filter Ecosystem
title: 🝖 Bounds
name_in_editor: "Filter : Bounds"
subtitle: Checks if a point is inside or outside the provided bounds, with options for bounds types and an epsilon adjustment.
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
| Bounds Source          | Defines the bounds to be used for tested points. |
| Bounds Target          | Defines the bounds to be used for the input `Bounds` points. |
| Check Type | The type of bounds x bounds check the filter will do. |
| Inside Epsilon | Lets you set a small value that will be used to expand the target bounds. |

---
## Bounds types
<br>
{% include embed id='settings-bounds-type' %}

---
## Check Type

Defines how source bounds are checked against the target bounds. These are fairly standard box/box operations.  

> Note that the source bounds are first transformed by their point' transform, and then inverse transformed by the target' point transform; meaning there can be some artifacts and mathematical variations as a result of applying the matrices.

|: Check Type     ||
| {% include img a='details/filter-ecosystem/enum-check-type-intersect.png' %}           | <span class="ebit">Intersect</span><br>TBD |
| {% include img a='details/filter-ecosystem/enum-check-type-is-inside.png' %}           | <span class="ebit">Is Inside</span><br>TBD |
| {% include img a='details/filter-ecosystem/enum-check-type-is-inside-or-on.png' %}           | <span class="ebit">Is Inside or On</span><br>TBD |
| {% include img a='details/filter-ecosystem/enum-check-type-is-inside-or-intersect.png' %}           | <span class="ebit">Is Inside or On</span><br>TBD |
{: .enum }