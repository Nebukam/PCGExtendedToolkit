---
layout: page
grand_parent: Clusters
parent: Refine
title: 🝔 Remove Highest Score
subtitle: Removes edges with the highest heuristic scores
#summary: The **Remove Highest Score** refinement ...
color: red
splash: icons/icon_edges-refine.svg
see_also:
    - Refine
    - 🝔 Remove Lowest Score
tagged: 
    - edgerefining
nav_order: 12
---

{% include header_card_toc %}

This refinement **removes a single connected edge for each point** for each point: the one with the **highest** score based on connected {% include lk id='🝰 Heuristics' %}.
{: .fs-5 .fw-400 } 

>Note that the removed `Edge` can be the same for multiple, different `Vtx`.

>Not all heuristics can be yield usable scores outside of a search context. For example, {% include lk id='🝰 Azimuth' %} and {% include lk id='🝰 Inertia' %} require clear seed/goals and search history to assign a given score. Such heuristics will yield a default score based on their own settings.
{: .warning }

{% include img a='placeholder-wide.jpg' %}

---
## Available Heuristics Modules
<br>
{% include card_any tagged="heuristics" %}
