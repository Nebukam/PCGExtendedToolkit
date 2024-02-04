---
layout: page
parent: Misc
grand_parent: All Nodes
title: Sort Points
subtitle: Sort points using any number of properties and attributes
color: white
summary: The **Sort Points** node organize points based on rules. Key elements include sorting direction (ascending/descending) and rules defined in a specific order. Each rule compares a selected attribute with a tolerance for equality. Note the warning on comparing values, emphasizing the default use of the first component for multi-component types.
splash: icons/icon_misc-sort-points.svg
preview_img: docs/splash-sort-points.png
toc_img: placeholder.jpg
tagged:
    - misc
nav_order: 1
---

{% include header_card_node %}

{% include img a='details/details-sort-points.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Sort Direction           | The output sorting direction -- `Ascending` *(small values first)* or `Descending` *(high values first)*.  |
| **Rules**           | A list of ordered individual rules used for sorting the points.|

### Rules ordering

Rules are compared **in the same order as in the property panel**, starting at index 0.  
The sorting goes through each rules until it finds a valid comparison (*non-equal values*) -- for each point.

### Sorting Rule

{% include img a='details/details-sort-points-rule.png' %} 

| Property       | Description          |
|:-------------|:------------------|
| Selector           | An attribute or property to compare. See {% include lk id='Attribute Selectors' %}. |
| Tolerance           | Equality tolerance used when comparing two values. |
| Invert Rule           | Switches from the default `<` comparison to `>`. |

>When selecting a value to compare, keep in mind that it will be broadcasted to a `double` type. This means that if you don't specify which component to use on multi-component type *(`Vectors`, `Transforms`, etc)*, it will default to the first one (`X`).
{: .warning }

---
# Inputs
## In
Any number of point datasets.

---
# Outputs
## Out
Same as Inputs but re-ordered.  
*Reminder that empty inputs will be ignored & pruned*.