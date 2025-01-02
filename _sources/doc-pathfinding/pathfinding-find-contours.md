---
layout: page
family: Pathfinding
#grand_parent: All Nodes
parent: Pathfinding
title: Find Contours
subtitle: Find edge contours & outlines
summary: The **Find Contours** node finds hole/outlines contours in a graph, using points as proximity seeds.
color: white
splash: icons/icon_custom-graphs-find-clusters.svg
tagged: 
    - node
    - pathfinder
nav_order: 4
inputs:
    -   name : Vtx
        extra_icon: IN_Vtx
        desc : Endpoints of the input Edges
        pin : points
    -   name : Edges
        extra_icon: OUT_Edges
        desc : Edges associated with the input Vtxs
        pin : points
    -   name : Seed
        desc : Seed points used to find contours based on proximity
        pin : points
outputs:
    -   name : Paths
        desc : One or multiple paths per seed points
        pin : points
---

{% include header_card_node %}



The **Find Contour** node attempts to find the contours of connected edges using a seed as a starting search point. It works by projecting the cluster on a plane and doing a clockwise search of the next best angle. *This is not a bulletproof approach but it works very well on projectable clusters with no overlapping edges.*
{: .fs-5 .fw-400 } 

{% include img a='details/pathfinding/pathfinding-find-contours.png' %} 

---
# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|:**Settings**||
| Seed Picking         | Lets you control how the seed node (`Vtx`) will be picked based on the provided seed position. |
| Duplicate Deadend points         | Whether or not to duplicate dead end points. Useful if you plan on offsetting the generated contours. |

|: **Output**||
| Output Type         | Filter which contours will be considered valid; either `Convex` only, `Concave` only, or `Both`. |
| Dedupe Paths         | If enabled, ensure that no two contours are the same.<br>*This is important because if your have poor seeds, they may generate the same contours.* |
| Keep Only Graceful Contours         | Whether to keep only contours that closed gracefully; *i.e connect to their start node* |
| Keep Contours with Deadends         | Whether to keep contour that include dead ends wrapping. |
| Output Filtered Seeds        | Whether to keep contour that include dead ends wrapping. |

|: **Pruning**||
| Min Point Count      | If enabled, does not output paths with a point count smaller than the specified amount.  |
| Max Point Count      | If enabled, does not output paths with a point count larger than the specified amount. |

|**Projection Settings**| *See below* |

|: **Tagging** ||
| <span class="etag">Concave Tag</span> | If enabled, will tag concave paths data with the specified tag. |
| <span class="etag">Convex Tag</span> | If enabled, will tag convex paths data with the specified tag. |
| <span class="etag">Is Closed Loop Tag</span> | If enabled, will tag closed loop paths data with the specified tag. |
| <span class="etag">Is Open Path Tag</span> | If enabled, will tag open paths data with the specified tag. |
| <span class="eout">DeadEnd Flag</span><br>`bool` | If enabled, will flag path points generated from dead end edges. |

|: **Forwarding** ||
| Seed Attributes to Path Tags | Let you pick and choose Seed point attributes and turn them to tags added to their associated output contours.<br>*See below.* |
| Seed Forwarding | Let you pick and choose which Seed attributes are transferred to path points.<br>*See below.* |

---
## Projection Settings
<br>
{% include embed id='settings-projection' %}

---
## Seed Attributes to Path Tags
<br>

{% include embed id='settings-forward' %}

---
## Seed Forwarding
<br>

{% include embed id='settings-forward' %}


