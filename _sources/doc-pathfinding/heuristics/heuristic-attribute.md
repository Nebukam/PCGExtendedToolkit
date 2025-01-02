---
layout: page
family: Heuristics
grand_parent: Pathfinding
parent: 🝰 Heuristics
title: 🝰 Heuristic Attribute
subtitle: Attribute-driven heuristics
summary: The **Attribute** heuristics uses custom point or edge value as raw score.
splash: icons/icon_pathfinding-edges.svg
nav_order: -1
color: param
tagged:
    - heuristics
outputs:
    -   name : Heuristics
        extra_icon: OUT_Heuristic
        desc : A single heuristics definition
        pin : params
#has_children: true
---

{% include header_card_node %}

Heuristics Attribute allows fine-grained and precise control over pathfinding constraints by leveraging user-defined attributes.
{: .fs-5 .fw-400 } 

>When dealing with values, keep in mind that **lower numbers are considered more desirable** by the {% include lk id='⊚ Search' %} algorithms.
{: .error }

---
# Properties
<br>

{% include embed id='settings-heuristics' %}

|**Attribute Settings**||
| Source     | Whether to read the attribute from `Vtx` or `Edges` |
| Attribute     | The attribute that will be used as score. |

{% include embed id='settings-heuristics-local-weight' %}
