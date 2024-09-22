---
layout: page
parent: Pathfinding
title: 🝰 Heuristics
subtitle: An inventory of the available heuristics modules.
#summary: summary_goes_here
splash: icons/icon_pathfinding-edges.svg
preview_img: previews/index-heuristics.png
nav_order: 20
color: red
has_children: true
tagged: 
    - module
    - node
---

{% include header_card %}

Heuristics modules are primarily used by {% include lk id='Pathfinding' %} nodes, such as {% include lk id='Edges Pathfinding' %} and {% include lk id='Plot Edges Pathfinding' %}
{: .fs-5 .fw-400 }

Heuristics are basically some under-the-hood maths used by {% include lk id='⊚ Search' %} Algorithms to gauge whether one path is better than another.  Different algorithms use heuristics differently, but their values is computed consistently.

## Modules
<br>
{% include card_childs tagged="heuristics" %}