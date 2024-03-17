---
title: settings-extra-weight
has_children: false
nav_exclude: true
---

|**Extra Weighting**||
|Weight up Visited| If enabled, points and edges will accumulate additional weight are paths are found.<br>This allows you to make "already in use" points & edges either more or less desirable for the next internal execution. |
|Visited Points Weight Factor| The weight to add to points that have been visited. This is a multiplier of the Heuristic' Modifiers `Reference Weight`.<br>*The weight is added each time a point is used.*|
|Visited Edges Weight Factor| The weight to add to edges that have been visited. This is a multiplier of the Heuristic' Modifiers `Reference Weight`.<br>*The weight is added each time an edge is used.*|

> **Important note on weighting up visited `Vtx` and `Edges`:**  
> - The weight is only computed for the pathfinding node and isn't carried over or cached.  
> - Enabling this feature breaks parallelism. Task are still ran asynchronously, but each path must wait for the previous one to be computed. Impact is usually negligible, but if you have *lots* of paths, it may take noticeably more time to process.