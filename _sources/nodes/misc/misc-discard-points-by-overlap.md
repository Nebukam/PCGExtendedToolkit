---
layout: page
parent: Misc
grand_parent: All Nodes
title: Discard Points by Overlap
subtitle: Self-pruning but for collections.
color: white
#summary: summary_goes_here
splash: icons/icon_misc-discard-by-overlap.svg
preview_img: docs/splash-discard-by-overlap.png
toc_img: placeholder.jpg
tagged: 
    - misc
nav_order: 6
---

{% include header_card_node %}

{% include img a='details/details-discard-by-point-overlap.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Min Point Count      | If enabled, does not output data with a point count smaller than the specified amount.  |
| Max Point Count      | If enabled, does not output data with a point count larger than the specified amount. |

---
# Inputs
## In
Any number of point datasets.

---
# Outputs
## Out
Inputs that passed the filter.
*Reminder that empty inputs will be ignored & pruned*.