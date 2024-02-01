---
layout: page
parent: Pathfinders
grand_parent: Nodes
title: Plot Edges Pathfinding
subtitle: Find a path that goes through multiple chained points.
color: white
#summary: summary_goes_here
splash: icons/icon_pathfinding-edges-plot.svg
preview_img: docs/splash-plot-edges-pathfinding.png
toc_img: placeholder.jpg
tagged: 
    - pathfinder
see_also: 
    - Pathfinding
nav_order: 2
---

{% include header_card_node %}

{% include img a='details/details-pathfinding-edges-plot.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Add Seed to Path           | Prepends the *seed position* at the beginning of the output path.<br>*This will create a point with the position of the seed.* |
| Add Goal to Path           | Appends the *goal position* at the end of the output path.<br>*This will create a point with the position of the goal.* |
| Add Plot Points to Path           | Include plot points positions as part of the output path. |

|**Modules**||
|**Search Algorithm**| The search algorithm that will be used to solve pathfinding.<br>*Each module has individual settings and documentation -- See [Available Search Algorithms](#available-search-modules).* |
|**Heuristics**| The base heuristics module that will be used during pathfinding.<br>*Each module has individual settings and documentation -- See [Available Heuristics](#available-heuristics-modules).* |
|**Heuristics Modifiers**| This property group is available no matter what **Heuristics** have been picked.<br>*See {% include lk id='Heuristic Modifiers' %}.*|
  
{% include_relative settings-statistics.md %}

|**Extra Weighting**||
|Weight up Visited| If enabled, points and edges will accumulate additional weight are paths are found.<br>This allows you to make "already in use" points & edges either more or less desirable for the next internal execution.<br>*Note that accumulated weight is consolidated between each plot points, as opposed to between each plotted path.* |
|Visited Points Weight Factor| The weight to add to points that have been visited. This is a multiplier of the Heuristic' Modifiers `Reference Weight`.<br>*The weight is added each time a point is used.*|
|Visited Edges Weight Factor| The weight to add to edges that have been visited. This is a multiplier of the Heuristic' Modifiers `Reference Weight`.<br>*The weight is added each time an edge is used.*|
|Global Visited Weight| If enabled, additive weight will be shared across the different plotted path. |

> **Important note on weighting up visited `Vtx` and `Edges`:**  
> - The weight is only computed for the pathfinding node and isn't carried over or cached.  
> - Enabling `Global Visited Weight` breaks parallelism. Tasks are still ran asynchronously, but each path must wait for the previous one to be computed. Impact is usually negligible, but if you have *lots* of paths, it may take noticeably more time to process.

---
# Inputs
## Plots
The plot input supports an unlimited amount of points dataset.  
Each Plot dataset is interpreted as a list of point that must be connected by a single path, in order, then merged into a single consolidated path.

---
# Outputs
## Paths
A point dataset for each path generated.  
Points in the dataset are ordered linearily from start to end.

---
# Modules

## Available {% include lk id='Search' %} modules
<br>
{% include card_any tagged="search" %}

---
## Available {% include lk id='Heuristics' %} modules
<br>
{% include card_any tagged="heuristics" %}