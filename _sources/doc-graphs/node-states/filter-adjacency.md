---
layout: page
parent: Find Node States
grand_parent: Graphs
title: 🝖 Adjacency
subtitle: Check if adjacent node meet specific conditions
color: white
summary: TBD
splash: icons/icon_misc-write-index.svg
preview_img: docs/splash-write-index.png
toc_img: placeholder.jpg
tagged: 
    - node
    - clusterfilter
    - graphs
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
# Outputs
## Out
Same as Inputs with the added metadata.  
*Reminder that empty inputs will be ignored & pruned*.