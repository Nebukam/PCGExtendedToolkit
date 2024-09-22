---
layout: page
family: FilterHub
#grand_parent: All Nodes
parent: Filter Ecosystem
title: Uber Filter (Collection)
name_in_editor: Uber Filter (Collection)
subtitle: Combine multiple filters
summary: The **Uber Filter (Collection)** node utilizes all available filters to evaluate points within collections, filtering the entire collection based on the aggregated results of individual points.
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
        desc : Collections that passed the filters
        pin : points
    -   name : Outside
        desc : Collections that didn't pass the filters.
        pin : points
---

{% include header_card_node %}

The **Uber Filter (Collection)** node combines various filters to assess collections of points, determining whether each collection passes based on the results of its constituent points.
{: .fs-5 .fw-400 } 

{% include img a='details/filter-ecosystem/filter-uber-filter-collection-lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| **Settings**          ||
| Mode         | Choose how to deal with the combined results.<br>*See below.*|
| Measure         | Measure to use when using `Partial` filter requirements.<br>Either `Relative` to the total number of points in the collection, or an absolute, `Discrete` value. |
| Comparison         | How to compare the number of points that passed the filters against the specified threshold. |

| **Threshold**          ||
| Dbl Threshold         | Relative threshold over a `0..1` range.<br>*0 = 0% of the points passed, 1 = 100% of the points passed.* |
| Int Threshold         | Discrete threshold. |
| Tolerance         | Tolerance used with approximative comparison modes. |

| Swap         | Invert the result of the threshold test. |

|: Mode      ||
| <span class="ebit">All</span>           | All points within the collection must pass the filters for the entire collection to pass. |
| <span class="ebit">Any</span>           | Any point within the collection has to pass the filters for the entire collection to pass. |
| <span class="ebit">Partial</span>           | A partial number of points must pass the filters for the entire collection to pass. |
{: .enum }

---
## Available Filters
<br>
{% include card_childs reference='Filter Ecosystem' tagged='filter' %}