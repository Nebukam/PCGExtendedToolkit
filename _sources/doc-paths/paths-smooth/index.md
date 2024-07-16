---
layout: page
#grand_parent: All Nodes
parent: Paths
title: Smooth
subtitle: Smooth points properties and attributes
color: white
summary: The **Smooth** node enhances path appearance. Customize closed paths, protect start and end points. Adjust global influence for overall smoothing. Use local influence to tailor per-point impact. Explore different smoothing types for varied effects.
splash: icons/icon_path-smooth.svg
preview_img: docs/splash-smoothing.png
toc_img: placeholder.jpg
has_children: true
tagged: 
    - node
    - paths
nav_order: 1
inputs:
    -   name : Paths
        desc : Paths which points will be smoothed
        pin : points
outputs:
    -   name : Paths
        desc : Paths with updated points positions
        pin : points
---

{% include header_card_node %}

{% include img a='details/details-smooth.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Closed Path           | If enabled, will process input paths as closed, effectively wrapping last and first point.  |
| Preserve Start           | If enabled, the first point will be unaffected by the smoothing<br>*Same as if its local influence was `0`.* |
| Preserve End           | If enabled, the last point will be unaffected by the smoothing<br>*Same as if its local influence was `0`.* |
| Influence           | Global influence.<br>This is used as a value to lerp the smoothed points properties with the unsmoothed one.<br>- `0` = Not smoothed<br>- `1` = Fully smoothed |
| Local Influence           | If enabled, the influence property is applied per-point using the specified attribute from the point being smoothed. |

| **Smoothing**           | This property lets you select which kind of smoothing you want to apply to the input paths.<br>*See [Available Smoothing Modules](#available-smoothing-modules).*|

---
## Smoothing modules
<br>
{% include card_any tagged="pathsmoothing" %}

---
# Inputs
## In
Any number of point datasets assumed to be paths.

---
# Outputs
## Out
Same as Inputs with the transformation applied.  
*Reminder that empty inputs will be ignored & pruned*.