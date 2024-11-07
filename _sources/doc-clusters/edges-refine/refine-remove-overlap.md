---
layout: page
grand_parent: Clusters
parent: Refine
title: 🝔 Remove Overlap
subtitle: Removes overlapping edges
#summary: The **Remove Overlap** refinement ...
color: blue
splash: icons/icon_edges-refine.svg
see_also:
    - Refine
tagged: 
    - edgerefining
nav_order: 5
---

{% include header_card_toc %}

This test each edge for possible overlap/intersection with other edges in the same cluster.
*By design, the output is guaranteed to be sanitized (e.g, each cluster will retains its existing connectivity properties).*
{: .fs-5 .fw-400 } 

{% include img a='placeholder-wide.jpg' %}

# Properties

| Property       | Description          |
|:-------------|:------------------|
|:**Settings** :|
| Keep          | When checking two edges, defines which one will be preserved, and which one will be removed. |
| Tolerance           | The distance at which two edges are considered to be overlapping.<br>*The higher the value, the more aggressive the pruning.* |

|: **Limits** :|
| Min Angle           | If enabled, will only check overlap if the two edges have **at least** the specified angle between each other. |
| Max Angle           | If enabled, will only check overlap if the two edges hace **at most** the specified angle between each other. |

> This refinement can be rather expensive depending on the topology of the clusters.
{: .warning }