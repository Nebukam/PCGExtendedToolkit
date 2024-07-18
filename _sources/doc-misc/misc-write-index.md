---
layout: page
#grand_parent: All Nodes
parent: Misc
title: Write Index
subtitle: Write current point index to an attribute
summary: The **Write Index** node write point index as an attribute, either discrete or normalized. Don't laugh, this is actually much more useful that you'd think.
color: white
splash: icons/icon_misc-write-index.svg
preview_img: docs/splash-write-index.png
toc_img: placeholder.jpg
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
# Inputs
## In
Any number of point datasets.

---
# Outputs
## Out
Same as Inputs with the added metadata.  
*Reminder that empty inputs will be ignored & pruned*.