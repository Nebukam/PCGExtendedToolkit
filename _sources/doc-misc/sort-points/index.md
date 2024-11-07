---
layout: page
family: MiscWrite
#grand_parent: All Nodes
parent: Misc
title: Sort Points
subtitle: Sort points using any number of properties and attributes
summary: The **Sort Points** node organize points based on rules. Key elements include sorting direction (ascending/descending) and rules defined in a specific order. Each rule compares a selected attribute with a tolerance for equality. Note the warning on comparing values, emphasizing the default use of the first component for multi-component types.
color: white
splash: icons/icon_misc-sort-points.svg
preview_img: previews/index-sort.png
has_children: true
tagged:
    - node
    - misc
nav_order: 1
inputs:
    -   name : In
        desc : Points to sort
        pin : points
    -   name : Sorting Rules
        desc : Sorting rules to be processed. They will be sorted by their individual priorities.
        pin : params
outputs:
    -   name : Out
        desc : Sorted points
        pin : points
---

{% include header_card_node %}

{% include img a='details/sort-points/rule.png' %} 

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|:**Settings**||
| Sort Direction           | The output sorting direction -- `Ascending` *(small values first)* or `Descending` *(high values first)*.  |
| **Rules**           | A list of ordered individual rules used for sorting the points.|

### Rules ordering

Rules are compared **in the same order as in the property panel**, starting at index 0.  
The sorting goes through each rules until it finds a valid comparison (*non-equal values*) -- for each point.

### Sorting Rule



| Property       | Description          |
|:-------------|:------------------|
| Selector           | An attribute or property to compare. See {% include lk id='Attribute Selectors' %}. |
| Tolerance           | Equality tolerance used when comparing two values. |
| Invert Rule           | Switches from the default `<` comparison to `>`. |

>When selecting a value to compare, keep in mind that it will be broadcasted to a `double` type. This means that if you don't specify which component to use on multi-component type *(`Vectors`, `Transforms`, etc)*, it will default to the first one (`X`).
{: .warning }
