---
layout: page
family: Pathfinding
#grand_parent: All Nodes
parent: Pathfinding
title: Plot Edges Pathfinding
subtitle: Find a path that goes through multiple chained points.
summary: The **Plot Edges Pathfinding** mode ...
color: white
splash: icons/icon_pathfinding-edges-plot.svg
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

The **Plot Edges Pathfinding** node takes multiple input dataset as "plot" and finds a path that connects each point in a given plot, in order.  It's very straightforward to use, and should generally be preferred over its {% include lk id='Edges Pathfinding' %} alternative.
{: .fs-5 .fw-400 } 

{% include img a='details/details-pathfinding-edges-plot.png' %} 

---
# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|**Plot inclusiong**||
| Add Seed to Path           | Prepends the *seed position* at the beginning of the output path.<br>*This will create a point with the position of the seed.* |
| Add Goal to Path           | Appends the *goal position* at the end of the output path.<br>*This will create a point with the position of the goal.* |
| Add Plot Points to Path           | Include plot points positions as part of the output path.<br>*Does not includes seed or goal points.* |

|**Settings**||
| Closed Loop           | Whether the plots should generate closed paths.<br>If enabled, the last plot point will create a path that wraps with the first plot point. |
| Seed Picking         | Lets you control how the seed node (`Vtx`) will be picked based on the provided seed position. |
| Goal Picking         | Lets you control how the goal node (`Vtx`) will be picked based on the provided goal position. |
| **Search Algorithm**         | Let you pick which {% include lk id='⊚ Search' %} algorithm to use to resolve pathfinding. |

> Seed/Goal picking is resolved for each pair of point in a given plot.

|**Misc**||
| Use Octree Search         | Whether or not to search for closest node using an octree. Depending on your dataset, enabling this may be either much faster, or much slower.<br>*I highly recommend enabling it if you resolve a lot of paths at the same time, but as a rule of thumb just profile it with/without and pick what works best in your setup.* |
| Omit Complete Path on Failed Plot         | If enabled, a single seed/goal pair fail will invalidate the full plotted path. *If disabled, failed segments will ungracefully connect plot points with a straight line.* |

|: **Tagging** ||
| Is Closed Loop Tag | If enabled, will tag closed loop paths data with the specified tag. |
| Is Open Path Tag | If enabled, will tag open paths data with the specified tag. |

## Available {% include lk id='⊚ Search' %} modules
<br>
{% include card_any tagged="search" %}

---
## Available {% include lk id='🝰 Heuristics' %} modules
<br>
{% include card_any tagged="heuristics" %}
