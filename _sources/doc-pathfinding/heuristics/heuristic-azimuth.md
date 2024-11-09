---
layout: page
family: Heuristics
grand_parent: Pathfinding
parent: 🝰 Heuristics
title: 🝰 Azimuth
subtitle: Favor edges directed toward the goal.
summary: The **Azimuth** heuristic attempt to force the path to always aim toward the goal.
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

The **Azimuth** heuristic favors traversing edges that are directed toward the search goal.  
{: .fs-5 .fw-400 } 

From a purely result perspective, it may *look* like a shortest path because it tend to produce more "straight" results going from seed to goal (if the topologies allows for it), but under the hood it's a very different logic.  This heuristics works best when combined with other more intricate ones to enforce some visual stability to the path.  

---
# Properties
<br>

{% include embed id='settings-heuristics' %}

{% include embed id='settings-heuristics-local-weight' %}
