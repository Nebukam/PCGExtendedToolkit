---
layout: page
parent: Graphs
#grand_parent: All Nodes
title: Find Node States
subtitle: Find conditional-based states to nodes within a graph
color: white
summary: TBD
splash: icons/icon_misc-write-index.svg
preview_img: docs/splash-write-index.png
toc_img: placeholder.jpg
has_children: true
tagged: 
    - node
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
## Available cluster-specific filter nodes
<br>
{% include card_any tagged="clusterfilter" %}

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