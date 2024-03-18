---
layout: page
parent: Misc
#grand_parent: All Nodes
title: Uber Filter
subtitle: Combine multiple filters
color: white
summary: Don't laugh, this is actually much more useful that you'd think.
splash: icons/icon_misc-write-index.svg
preview_img: docs/splash-write-index.png
toc_img: placeholder.jpg
has_children: true
tagged: 
    - node
    - misc
nav_order: 7
---

{% include header_card_node %}

{% include img a='details/details-write-index.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Output Normalized Index           | If enabled, the index will be written as a `double` *(instead of `int32`)*, as a normalized value in the range `[0..1]`.  |
| Output Attribute Name           | Name of the attribute to write the point index to. |

---
## Available filter nodes
<br>
{% include card_any tagged="filter" %}

---
# Inputs
## In
Any number of point datasets.

---
# Outputs
## Out
Same as Inputs with the added metadata.  
*Reminder that empty inputs will be ignored & pruned*.