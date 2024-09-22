---
layout: page
family: Misc
grand_parent: Misc
parent: Lloyd Relax
title: 2D Lloyd Relaxation
name_in_editor: "Lloyd Relax 2D"
subtitle: Applies the Lloyd Relaxation algorithm.
summary: The **2D Lloyd Relaxation** node applies any number of Lloyd relaxation passes, in 2D space.
color: white
splash: icons/icon_edges-relax.svg
tagged: 
    - node
    - misc
nav_order: 7
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