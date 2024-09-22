---
layout: page
family: SamplerNeighbor
grand_parent: All Nodes
parent: Clusters
title: "Properties : Edge"
name_in_editor: "Cluster : Edge Properties"
subtitle: Compute edge extra data from its vertices
summary: The **Edge Properties** node allows you to compute and write additional cluster-related data for edges, as well as "solidify" their bounds, giving them a more defined shape. The direction of the edge, from start to end, is determined by the chosen direction method and is used to influence outputs.
color: blue
splash: icons/icon_custom-graphs-promote-edges.svg
tagged: 
    - node
    - edges
see_also: 
    - Working with Clusters
    - Interpolate
nav_order: 5
inputs:
    -   name : Vtx
        desc : Endpoints of the input Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the input Vtxs
        pin : points
outputs:
    -   name : Vtx
        desc : Endpoints of the output Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the output Vtxs
        pin : points
---

{% include header_card_node %}

The **Edge Properties** node lets you extract & write cluster-related properties to individual Edges points, as well as "solidify" the edge points (i.e giving their bounds an decent shape).
{: .fs-5 .fw-400 } 

{% include img a='details/clusters-edge-properties/lead.png' %}

# Properties
<br>

---
## Direction

{% include embed id='settings-edge-direction' %}
{% include img_link a='explainers/edge-direction-method.png' %}

---
## Outputs

| Property       | Description          |
|:-------------|:------------------|
|**Outputs**||
| <span class="eout">Edge Length</span><br>`double`     |  |
| <span class="eout">Edge Direction</span><br>`FVector` |  |
| Endpoints Blending |  |
| Endpoints Weights |  |
| Blending Settings | Defines how each enpoint' property (`Start` and `End` Vtx) is blended into the edge.<br>*See {% include lk id='Blending' %}*. |
| Heuristics | If enabled, will reveal a new Heuristic input.<br>**This is used to ouput & debug the heuristic score of each edge.**<br>*Note that heuristics that require external parameters won't work correctly and output a constant value.*  |

<br>
> Note on blending: this is a 2-point blend, hence `Lerp` yields more accurate results than `Weight`.
{: .infos }


---
# Solidfication
<br>

Solidifying an edge means computing its bounds so they visually connect the start & end point.  
This makes edge point incredibly more useful than just "data holders"!
{: .fs-5 .fw-400 }

{% include img_link a='details/clusters-edge-properties/solidifcation.png' %}

Solidification is fairly straightforward on paper, but in order to be flexible it also exposes an overwhelming amount of parameters.  
<br>
> Unreal has a visual bug in the detail panel: the first time you select a solidification axis, remaining axis' properties ***will not show up**.
> *You have to deselect the node then select it again for the details panel to properly refresh.*
{: .warning }

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Write Edge Position          | |
| Edge Position Lerp          | |
| Solidification Axis          | |
| Solidification Lerp Operand          | |
| Solidification Lerp Constant          | |
| Radiuses          | |

> Note that if solidification is enabled, the specified `Edge Position Lerp` will override the `Endpoints Weights` specified for Outputs, to enforce consistency.
{: .warning }

### Radiuses

When a solidification axis is selected, lets you set the edge bounds' remaining two axis as "radiuses".  
Each component shares the same following properties:  

| Enabled    |  |
| Radius Type |  |
| Radius Constant |  |
| Radius Attribute |  |