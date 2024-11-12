---
layout: page
family: Filter
#grand_parent: Misc
parent: Filter Ecosystem
title: 🝖 Spline Inclusion
name_in_editor: "Filter : Spline Inclusion"
subtitle: Checks against how a point is included in a spline.
color: param
summary: "-"
splash: icons/icon_misc-sort-points.svg
tagged: 
    - node
    - filter
    - pointfilter
    - misc
nav_order: 40
outputs:
    -   name : Splines
        desc : Splines to check against
        pin : splines
outputs:
    -   name : Filter
        desc : A single filter definition
        pin : params
---

{% include header_card_node %}

The **Spline Inclusion** filter checks the spatial relationship of a point against input splines; i.e are they inside, outside, on, or a combination.
{: .fs-5 .fw-400 } 

{% include img a='details/filter-ecosystem/filter-spline-inclusion-lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| **Settings**          ||
| Sample Inputs         | Lets you filter out input splines based on whether they're closed loops or not. |
| Check Type         | *See below.* |
| Pick         | Lets you choose which spline to test against. |
| Tolerance         | Distance to the spline within which a point is considered to be `On` the spline. |
| Spline Scales Tolerance         | If enabled, the `Tolerance` value will be scaled by the spline' `YZ` scale' length. |
| Invert         | If enabled, inverts the result of the filter. |

---
## Check Type
<br>

|: Check Type      ||
|:-------------|:------------------|
| {% include img a='details/filter-ecosystem/enum-incl-in.png' %}           | <span class="ebit">Is Inside</span><br>Check if point lies inside. |
| {% include img a='details/filter-ecosystem/enum-incl-inoron.png' %}           | <span class="ebit">Is Inside or On</span><br>Check if point lies inside or on. |
| {% include img a='details/filter-ecosystem/enum-incl-inandon.png' %}           | <span class="ebit">Is Inside and On</span><br>Check if point lies inside and on. |
| {% include img a='details/filter-ecosystem/enum-incl-out.png' %}           | <span class="ebit">Is Outside</span><br>Check if point lies outside. |
| {% include img a='details/filter-ecosystem/enum-incl-outoron.png' %}           | <span class="ebit">Is Outside or On</span><br>Check if point lies outside or on.|
| {% include img a='details/filter-ecosystem/enum-incl-outandon.png' %}           | <span class="ebit">Is Outside and On</span><br>Check if point lies outside and on.|
| {% include img a='details/filter-ecosystem/enum-incl-on.png' %}           | <span class="ebit">Is On</span><br>Check if point lies on.|
| {% include img a='details/filter-ecosystem/enum-incl-noton.png' %}           | <span class="ebit">Is not On</span><br>Check if the point does not lies on.|
{: .enum }