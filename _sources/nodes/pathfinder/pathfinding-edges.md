---
layout: page
parent: Pathfinders
grand_parent: All Nodes
title: Edges Pathfinding
subtitle: Find paths connecting seeds to goals.
color: white
#summary: summary_goes_here
splash: icons/icon_pathfinding-edges.svg
preview_img: docs/splash-edges-pathfinding.png
toc_img: placeholder.jpg
tagged: 
    - pathfinder
see_also: 
    - Pathfinding
nav_order: 1
---

{% include header_card_node %}

{% include img a='details/details-pathfinding-edges.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Goal Picker**| The module that will be used to interpret and manipulate goals and seeds inputs.<br>*Each module has individual settings and documentation -- See [Available Goal Pickers](#available-goal-pickers-modules).*|

|**Settings**||
| Add Seed to Path           | Prepends the *seed position* at the beginning of the output path.<br>*This will create a point with the position of the seed.* |
| Add Goal to Path           | Appends the *goal position* at the end of the output path.<br>*This will create a point with the position of the goal.* |

|**Modules**||
|**Search Algorithm**| The search algorithm that will be used to solve pathfinding.<br>*Each module has individual settings and documentation -- See [Available Search Algorithms](#available-search-modules).* |
|**Heuristics**| The base heuristics module that will be used during pathfinding.<br>*Each module has individual settings and documentation -- See [Available Heuristics](#available-heuristics-modules).* |
|**Heuristics Modifiers**| This property group is available no matter what **Heuristics** have been picked.<br>*See {% include lk id='Heuristic Modifiers' %}.*|

{% include embed id='settings-statistics' %}

|**Extra Weighting**||
|Weight up Visited| If enabled, points and edges will accumulate additional weight are paths are found.<br>This allows you to make "already in use" points & edges either more or less desirable for the next internal execution. |
|Visited Points Weight Factor| The weight to add to points that have been visited. This is a multiplier of the Heuristic' Modifiers `Reference Weight`.<br>*The weight is added each time a point is used.*|
|Visited Edges Weight Factor| The weight to add to edges that have been visited. This is a multiplier of the Heuristic' Modifiers `Reference Weight`.<br>*The weight is added each time an edge is used.*|

> **Important note on weighting up visited `Vtx` and `Edges`:**  
> - The weight is only computed for the pathfinding node and isn't carried over or cached.  
> - Enabling this feature breaks parallelism. Tasks are still ran asynchronously, but each path must wait for the previous one to be computed. Impact is usually negligible, but if you have *lots* of paths, it may take noticeably more time to process.
<br>

---
# Modules

## Available {% include lk id='Goal Pickers' %} modules
<br>
{% include card_any tagged="goalpicker" %}

---
## Available {% include lk id='Search' %} modules
<br>
{% include card_any tagged="search" %}

---
## Available {% include lk id='Heuristics' %} modules
<br>
{% include card_any tagged="heuristics" %}

---
# Inputs
## Seeds & Goals
Each of these input data will be forwarded to the selected `Goal Picker`.  

---
# Outputs
## Paths
A point dataset for each path generated.  
Points in the dataset are ordered linearily from start to end.