---
layout: page
parent: Pathfinding
title: Heuristics
subtitle: An inventory of the available heuristics modules.
color: white
#summary: summary_goes_here
splash: icons/icon_component.svg
preview_img: placeholder.jpg
nav_order: 3
has_children: true
tagged: 
    - module
---

{% include header_card %}

Heuristics modules are primarily used by {% include lk id='Pathfinding' %} nodes, such as {% include lk id='Edges Pathfinding' %} and {% include lk id='Plot Edges Pathfinding' %}
{: .fs-5 .fw-400 }

Heuristics are basically some under-the-hood maths used by {% include lk id='Search' %} Algorithms to gauge whether one path is better than another.  Different algorithms use heuristics differently, but their values is computed consistently.

## Modules
<br>
{% include card_childs tagged="heuristics" %}