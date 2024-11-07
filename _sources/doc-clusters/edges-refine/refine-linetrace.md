---
layout: page
grand_parent: Clusters
parent: Refine
title: 🝔 Line Trace
subtitle: Removes edges by doing linetrace and checking for collisions.
#summary: The **Remove Overlap** refinement ...
color: blue
splash: icons/icon_edges-refine.svg
see_also:
    - Refine
tagged: 
    - edgerefining
nav_order: 3
---

{% include header_card_toc %}

This does linetraces using the edge' start & end point.
{: .fs-5 .fw-400 } 

{% include img a='placeholder-wide.jpg' %}

# Properties

| Property       | Description          |
|:-------------|:------------------|
|:**Settings** :|
| Collision Settings         | *See details below.* |
| Two Way Check           | If enabled, when the first linecast (From the edge' start to end) fails, it tries the other way around (From the edge' end to start).<br>*While expensive, this ensures the refinement doesn't fail on backface hits.*|


---
## Collision Settings
<br>
{% include embed id='settings-collisions' %}


> This refinement can be rather expensive depending on the number of edges.
{: .warning }