---
layout: page
family: Heuristics
grand_parent: Pathfinding
parent: 🝰 Heuristics
title: 🝰 Feedback
subtitle: Favor uncharted points & edges.
summary: The **Feedback** heuristic add/remove score value to points & edges that are "in use" by other previously computed paths.
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

The **Feedback** heuristic is very different from other heuristic nodes. It uses validated path within the scope of a single pathfinding node to make validated traversal more or less likely to be traversed by subsequent queries. There is two main "modes" : local & global feedback.
{: .fs-5 .fw-400 } 

---
# Properties
<br>

{% include embed id='settings-heuristics' %}

|**Feedback Settings**||
| Visited Point Weight Factor     | Multiplier to be applied to traversed nodes. |
| Visited Edge Weight Factor     | Multiplier to be applied to traversed edges. |
| Global Feedback     | Whether this feedback should be *global* or *local* |
| Affect All Connected Edges     | If enabled, the extra score affect all edges connected to the traversed `Vtx`, not just the one actually traversed. |

{% include embed id='settings-heuristics-local-weight' %}

---
## Global vs Local feedback
<br>
Global feedback is applied in the scope of the entire node: each completed query will score down the next ones.  
As soon as a query is resolved *(a `Seed` is connected to a `Goal`)*, the `Vtx` and `Edges` it traverses are scored by their specified factors.  

If global feedback is disabled, the extra scoring is only processed for the duration of the query -- **as such it only works with plots.**

> In order to be properly accounted for, global feedback requires queries to be processed one after another, greatly affecting processing time!
{: .warning }