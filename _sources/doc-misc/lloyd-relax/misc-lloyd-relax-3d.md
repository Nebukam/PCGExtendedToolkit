---
layout: page
grand_parent: Misc
parent: Lloyd Relax
title: 3D Lloyd Relaxation
summary: The **2D Lloyd Relaxation** node applies the Lloyd Relaxation algorithm in 3D.
color: white
summary: Applies any number of Lloyd relaxation passes, in 3D space.
splash: icons/icon_edges-relax.svg
preview_img: docs/splash-lloyd-3d.png
tagged: 
    - node
    - misc
nav_order: 8
---

{% include header_card_node %}

See [Lloyd Relaxation](https://en.wikipedia.org/wiki/Lloyd%27s_algorithm) on Wikipedia.

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Output Normalized Index           | If enabled, the index will be written as a `double` *(instead of `int32`)*, as a normalized value in the range `[0..1]`.  |
| Output Attribute Name           | Name of the attribute to write the point index to. |