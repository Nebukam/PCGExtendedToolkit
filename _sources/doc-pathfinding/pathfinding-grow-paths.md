---
layout: page
family: Pathfinding
#grand_parent: All Nodes
parent: Pathfinding
title: Grow Paths
name_in_editor: "Pathfinding : Grow Paths"
subtitle: Grows paths from seeds alone.
summary: The **Grow Paths** node randomly grows paths inside a cluster, using seeds and multiple contraints & heuristics.
color: white
splash: icons/icon_pathfinding-edges.svg
tagged: 
    - node
    - pathfinder
see_also: 
    - Pathfinding
nav_order: 10
inputs:
    -   name : Vtx
        desc : Endpoints of the input Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the input Vtxs
        pin : points
    -   name : Seeds
        desc : Seed points
        pin : point
    -   name : Heuristics
        desc : 🝰 Heuristics nodes that will be used by the pathfinding search algorithm
        pin : params
outputs:
    -   name : Paths
        desc : A point collection per path found
        pin : points
---

{% include header_card_node %}

The **Grow Path** node is still heavily WIP. It's an old node that wasn't properly updated after a major heuristic refactor, and doesn't handle them really well. It can still produce interesting results, but it's *very* hard to control.
{: .fs-5 .fw-400 } 

{% include img a='details/pathfinding/pathfinding-grow-paths-lead.png' %}
