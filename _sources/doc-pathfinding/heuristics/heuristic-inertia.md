---
layout: page
family: Heuristics
grand_parent: Pathfinding
parent: 🝰 Heuristics
title: 🝰 Inertia
subtitle: Favor active direction preservation.
summary: The **Inertia** heuristic uses the ongoing traversal data to try and maintain a consistent direction, as if the algorithm had "inertia".
splash: icons/icon_pathfinding-edges.svg
color: param
tagged: 
    - heuristics
nav_order: 3
outputs:
    -   name : Heuristics
        desc : A single heuristics definition
        pin : params
---

{% include header_card_node %}

The **Inertia** heuristic samples the search stack of the algorithm to favor the averaged direction of the last few samples, adding "inertia" to the search algorithm.
{: .fs-5 .fw-400 } 

> Inertia can be hard to work with depending on the type of pathfinding you're working with, and may not produce results as "smooth" as one may expect given the name. However it does a good job when combined with other heuristics to *overshoot* the path from its starting point.

---
# Properties
<br>

{% include embed id='settings-heuristics' %}

|**Inertia Settings**||
| Samples     | THe number of samples that will be averaged from the current search location. |
| Ignore If Not Enough Samples     | Whether to use the fallback weight if there's not enough samples available. |

|**Fallback values**||
| Global Inertia Score     | Since inertia requires a search history, it cannot be used to compute global score some algorithms rely on (A*). The specified value will be used as fallback instead. |
| Fallback Inertia Score     | This fallback score value is the one used if there is no or not enough samples. |


{% include embed id='settings-heuristics-local-weight' %}
