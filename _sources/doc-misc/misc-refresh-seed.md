---
layout: page
#grand_parent: All Nodes
parent: Misc
title: Refresh Seed
subtitle: Refreshes point seeds based on their position.
summary: The **Refresh Seed** node ...
color: white
splash: icons/icon_misc-write-index.svg
preview_img: docs/splash-seed-refresh.png
toc_img: placeholder.jpg
tagged: 
    - node
    - misc
nav_order: 15
---

{% include header_card_node %}

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Base           | A value added to the seed calculation to offset the output value.  |

---
# Inputs
## In
Any number of point datasets.

---
# Outputs
## Out
Same as Inputs with the refreshed seed value.    
*Reminder that empty inputs will be ignored & pruned*.