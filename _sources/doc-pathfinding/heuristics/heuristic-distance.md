---
layout: page
family: Heuristics
grand_parent: Pathfinding
parent: 🝰 Heuristics
title: 🝰 Shortest Distance
subtitle: Favor shortest distance.
summary: The **Shortest Distance** heuristic node ...
splash: icons/icon_pathfinding-edges.svg
color: param
tagged: 
    - heuristics
nav_order: 2
outputs:
    -   name : Heuristics
        desc : A single heuristics definition
        pin : params
---

{% include header_card_node %}

The **Shortest Distance** is the most basic heuristics of all. Scores are directly tied to edge' length, favoring shorter distances at all times.
{: .fs-5 .fw-400 } 

---
# Properties
<br>

{% include embed id='settings-heuristics' %}

|**Distance Settings**||
| TBD     | TBD |

{% include embed id='settings-heuristics-local-weight' %}
