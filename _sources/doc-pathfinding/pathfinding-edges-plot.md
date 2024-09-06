---
layout: page
#grand_parent: All Nodes
parent: Pathfinding
title: Plot Edges Pathfinding
subtitle: Find a path that goes through multiple chained points.
summary: The **Plot Edges Pathfinding** mode ...
color: white
splash: icons/icon_pathfinding-edges-plot.svg
preview_img: docs/splash-plot-edges-pathfinding.png
toc_img: placeholder.jpg
tagged: 
    - node
    - pathfinder
see_also: 
    - Pathfinding
nav_order: 2
inputs:
    -   name : Vtx
        desc : Endpoints of the input Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the input Vtxs
        pin : points
    -   name : Plots
        desc : Plot points in the form of points collections.
        pin : points
    -   name : Heuristics
        desc : Heuristics nodes that will be used by the pathfinding search algorithm
        pin : params
outputs:
    -   name : Paths
        desc : A single path per plot collection input
        pin : points
---

{% include header_card_node %}

> DOC TDB -- Heuristics underwent a thorough refactor that isn't documented yet. Each heuristic has its own node and they can be combined into the heuristic input of the pathfinding node. See the example project!
{: .warning }

{% include img a='details/details-pathfinding-edges-plot.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Add Seed to Path           | Prepends the *seed position* at the beginning of the output path.<br>*This will create a point with the position of the seed.* |
| Add Goal to Path           | Appends the *goal position* at the end of the output path.<br>*This will create a point with the position of the goal.* |
| Add Plot Points to Path           | Include plot points positions as part of the output path. |

|**Modules**||
|**Search Algorithm**| The search algorithm that will be used to solve pathfinding.<br>*Each module has individual settings and documentation -- See [Available Search Algorithms](#available-search-modules).* |
|**Heuristics**| The base heuristics module that will be used during pathfinding.<br>*Each module has individual settings and documentation -- See [Available Heuristics](#available--heuristics-modules).* |
|**Heuristics Modifiers**| This property group is available no matter what **Heuristics** have been picked.<br>*See {% include lk id='🝰 Heuristic Attribute' %}.*|
  
{% include embed id='settings-statistics' %}

|**Extra Weighting**||
|Weight up Visited| If enabled, points and edges will accumulate additional weight are paths are found.<br>This allows you to make "already in use" points & edges either more or less desirable for the next internal execution.<br>*Note that accumulated weight is consolidated between each plot points, as opposed to between each plotted path.* |
|Visited Points Weight Factor| The weight to add to points that have been visited. This is a multiplier of the Heuristic' Modifiers `Reference Weight`.<br>*The weight is added each time a point is used.*|
|Visited Edges Weight Factor| The weight to add to edges that have been visited. This is a multiplier of the Heuristic' Modifiers `Reference Weight`.<br>*The weight is added each time an edge is used.*|
|Global Visited Weight| If enabled, additive weight will be shared across the different plotted path. |

> **Important note on weighting up visited `Vtx` and `Edges`:**  
> - The weight is only computed for the pathfinding node and isn't carried over or cached.  
> - Enabling `Global Visited Weight` breaks parallelism. Tasks are still ran asynchronously, but each path must wait for the previous one to be computed. Impact is usually negligible, but if you have *lots* of paths, it may take noticeably more time to process.

---
# Modules

## Available {% include lk id='⊚ Search' %} modules
<br>
{% include card_any tagged="search" %}

---
## Available {% include lk id='🝰 Heuristics' %} modules
<br>
{% include card_any tagged="heuristics" %}
