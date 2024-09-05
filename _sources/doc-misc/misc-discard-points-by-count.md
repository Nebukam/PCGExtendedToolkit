---
layout: page
#grand_parent: All Nodes
parent: Misc
title: Discard Points by Count
subtitle: Filter point dataset by their point count.
summary: The **Discard Points by Count** node ...
color: white
splash: icons/icon_misc-discard-by-count.svg
preview_img: placeholder.jpg
toc_img: placeholder.jpg
tagged: 
    - node
    - misc
nav_order: 20
---

{% include header_card_node %}

# Properties
<br>

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