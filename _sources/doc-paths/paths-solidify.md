---
layout: page
family: Path
#grand_parent: All Nodes
parent: Paths
title: Solidify Path
name_in_editor: "Path : Solidify"
subtitle: Solidify paths into "edges"
summary: The **Solidify Path** node converts abstract path segments into solid edges, optionally reducing points and adding information, with flexible parameters for axis-based solidification and radius settings.
color: white
splash: icons/icon_paths-orient.svg
tagged: 
    - node
    - paths
nav_order: 2
inputs:
    -   name : Paths
        desc : Paths to solidify
        pin : points
outputs:
    -   name : Paths
        desc : Paths with optionally less points & more informations
        pin : points
---

{% include header_card_node %}

The **Path Solidify** node turns abstract path segments into "solid" edges. Each path point is turned into an edge-looking one that connects to the next. *Unless the path is a closed loop, the last point is deprecated, hence it is recommended to remove it.*
{: .fs-5 .fw-400 } 

{% include img a='details/paths-solidify/lead.png' %}

# Properties
<br>

{% include embed id='settings-closed-loop' %}

| Output       | Description          |
|:-------------|:------------------|
| Remove Last Point | If enabled, removes the last point on open paths, as they can't be properly solidified. |
| Scale Bounds | If enabled, bounds will be scaled to reflect the real world distance that separates the points.<br>*This is often necessary if the path' points have a scale that is not equal to one.* |


---
# Solidfication
<br>

Solidification is fairly straightforward on paper, but in order to be flexible it also exposes an overwhelming amount of parameters.  
<br>
> Unreal has a visual bug in the detail panel: the first time you select a solidification axis, remaining axis' properties ***will not show up**.
> *You have to deselect the node then select it again for the details panel to properly refresh.*
{: .warning }

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Solidification Axis          | |
| Soldification Lerp Operand          | |
| Soldification Lerp <br>*(Constant or Attribute)*          | |

### Radiuses

When a solidification axis is selected, lets you set the segment bounds' remaining two axis as "radiuses".  
Each component shares the same following properties:  

| Write Radius X/Y/Z    |  |
| Radius Type |  |
| Radius Constant |  |
| Radius Attribute |  |