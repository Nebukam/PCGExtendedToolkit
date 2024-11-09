---
layout: page
family: Heuristics
grand_parent: Pathfinding
parent: 🝰 Heuristics
title: 🝰 Least Nodes
subtitle: Favor traversing the least amount of nodes.
summary: The **Least Nodes** heuristic favor node count traversal over anything else. 
splash: icons/icon_pathfinding-edges.svg
color: param
tagged: 
    - heuristics
nav_order: 5
outputs:
    -   name : Heuristics
        desc : A single heuristics definition
        pin : params
---

{% include header_card_node %}

The **Least Node** heuristic score the number of traversed nodes with an equal, constant value, favoring paths traverse the fewest amount of node.
{: .fs-5 .fw-400 } 

> Note that fewer nodes doesn't means shortest paths!

---
# Properties
<br>

{% include embed id='settings-heuristics' %}

{% include embed id='settings-heuristics-local-weight' %}
