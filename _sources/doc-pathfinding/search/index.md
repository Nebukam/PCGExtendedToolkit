---
layout: page
parent: Pathfinding
title: Search
subtitle: An inventory of the available search modules.
#summary: summary_goes_here
color: white
splash: icons/icon_pathfinding-navmesh.svg
preview_img: placeholder.jpg
nav_order: 10
has_children: true
tagged: 
    - module
---

{% include header_card %}

Search algorithms are at the core of PCGEx Pathfinding nodes & capabilities. They are responsible for traversing individual clusters in search for the ideal path between the seed and goal points.
{: .fs-5 .fw-400 }

*At the time of writing, there are only two algorithms implemented, **A Start** and **Dijkstra**. The next implementation candidates will be **DFS** and **BFS** as they yield slightly different results, althought less friendly to modifiers.*

## Modules
<br>
{% include card_childs tagged="search" %}