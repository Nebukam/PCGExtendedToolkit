---
layout: page
#grand_parent: All Nodes
parent: Clusters
title: Break to Paths
name_in_editor: "Cluster : Break to Paths"
subtitle: Breaks clusters edge chains into individual paths
summary: The **Break Clusters to Paths** node converts edge chains into individual paths, offering a quick way to extract paths or splines from a refined topology without requiring complex pathfinding, but is less suited for dense clusters with highly connected vertices.
color: blue
splash: icons/icon_graphs-convex-hull.svg
preview_img: placeholder.jpg
tagged: 
    - node
    - clusters
nav_order: 20
see_also:
    - Working with Clusters
inputs:
    -   name : Vtx
        desc : Endpoints of the input Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the input Vtxs
        pin : points
    -   name : Break Conditions
        desc : Filters used to know which points are 'break' points.
        pin : params
outputs:
    -   name : Packed Clusters
        desc : Individually packed vtx/edges pairs
        pin : points
---

{% include header_card_node %}

The **Break Cluster to Path** turns all `Edges` chains to individual paths. It's useful to quickly get a bunch of paths or splines out of a refined topology without the need for pathfinding.
{: .fs-5 .fw-400 } 

{% include img a='placeholder-wide.jpg' %}

> It is not recommended to use this node on very dense clusters (where a lot of `Vtx` have more than two connections), as it quickly ends up creating a single point data per individual edge.
{: .error }

---
# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| Operate On           | Lets you choose how to break cluster down.<br>- `Paths` : Operate on edge chains which form paths with no crossings.  e.g, nodes with only two neighbors.<br>- `Edges` : Operate on each edge individually (very expensive)  |
| Normal | If enabled, output the averaged normal of the `vtx` based on all connected `edges`.<br>*This output is hardly usable for highly 3-dimensional nodes.* |
| Min Point Count | This lets you filter out output paths that have less that the specified number of points. |
| Max Point Count | If enabled, this lets you filter out output paths that have more that the specified number of points. |

---
## Using Break Conditions

The break condition filters can be used to get tighter control over when to split a clean edge chain into more sub-paths than the default behavior. If the point pass the specified filters, it will stop whatever path was being created. If the point 

<br>
{% include card_any tagged="filter" %}