---
layout: page
parent: Paths
grand_parent: All Nodes
title: Fuse Collinear
subtitle: Fuse collinear points on a path
color: white
summary: The **Fuse Collinear** node removes points that are collinear, with control over thresholds. It can also optionally fuse points based on their proximity.
splash: icons/icon_path-fuse.svg
preview_img: docs/splash-path-collinear.png
toc_img: placeholder.jpg
tagged: 
    - paths
nav_order: 3
---

{% include header_card_node %}

{% include img a='details/details-fuse-collinear.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Threshold           | Threshold in degree under which the deviation is considered small enough to be collinear.  |
| Fuse Distance           | In addition to collinearity, this value allows to fuse points that are close enough. |

---
# Inputs
## In
Any number of point datasets assumed to be paths.

---
# Outputs
## Out
A smaller dataset for each input dataset.  
*Reminder that empty inputs will be ignored & pruned*.