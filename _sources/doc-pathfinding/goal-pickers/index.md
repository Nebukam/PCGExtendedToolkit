---
layout: page
parent: Pathfinding
title: 🝓 Goal Pickers
subtitle: An inventory of the available goal pickers modules.
#summary: The **Write Index** picker ...
color: red
splash: icons/icon_placement-center.svg
preview_img: previews/index-goal-pickers.png
nav_order: 30
has_children: true
tagged: 
    - module
    - node
---

{% include header_card %}

Heuristics modules are primarily used by {% include lk id='Pathfinding' %} nodes, such as {% include lk id='Edges Pathfinding' %} and {% include lk id='Plot Edges Pathfinding' %}
{: .fs-5 .fw-400 }

## Modules
<br>
{% include card_childs tagged="goalpicker" %}