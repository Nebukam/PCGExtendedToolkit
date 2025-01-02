---
layout: page
family: Heuristics
grand_parent: Pathfinding
parent: 🝰 Heuristics
title: 🝰 Steepness
subtitle: Favor flat trajectories.
summary: The **Steepness** heuristic uses the edge angle against an up vector to compute a dot product that is used to determine whether the edge should be considered flat or not.
splash: icons/icon_pathfinding-edges.svg
color: param
tagged: 
    - heuristics
nav_order: 3
outputs:
    -   name : Heuristics
        extra_icon: OUT_Heuristic
        desc : A single heuristics definition
        pin : params
---

{% include header_card_node %}

The **Steepness** heuristic favor edge directions that are perpendicular or opposite to a specified up vector.
{: .fs-5 .fw-400 } 

---
# Properties
<br>

{% include embed id='settings-heuristics' %}

|**Steepness Settings**||
| Up Vector     | Which direction is up. Traversed edge direction will be compared against it to measure how "steep" they are. |
| Absolute Steepness     | Whether the steepness goes both ways. If enabled, this favor generally flat terrain; if disabled, directions that mirror the Up Vector are considered even more desirable than flat. |

{% include embed id='settings-heuristics-local-weight' %}
