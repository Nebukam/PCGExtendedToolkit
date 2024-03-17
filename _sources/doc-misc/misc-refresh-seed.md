---
layout: page
parent: Misc
#grand_parent: All Nodes
title: Refresh Seed
subtitle: Refreshes point seeds based on their position.
color: white
summary: Don't laugh, this is actually much more useful that you'd think.
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