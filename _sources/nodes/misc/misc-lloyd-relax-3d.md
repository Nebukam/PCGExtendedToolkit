---
layout: page
parent: Misc
grand_parent: All Nodes
title: 3D Lloyd Relaxation
subtitle: Applies the Lloyd Relaxation algorithm.
color: white
summary: Applies any number of Lloyd relaxation passes, in 3D space.
splash: icons/icon_edges-relax.svg
preview_img: docs/splash-lloyd-3d.png
toc_img: placeholder.jpg
tagged: 
    - misc
nav_order: 7
---

{% include header_card_node %}

See [Lloyd Relaxation](https://en.wikipedia.org/wiki/Lloyd%27s_algorithm) on Wikipedia.

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