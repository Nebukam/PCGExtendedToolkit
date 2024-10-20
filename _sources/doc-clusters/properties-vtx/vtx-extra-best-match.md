---
layout: page
family: SamplerNeighbor
grand_parent: Clusters
parent: "Properties : Vtx"
title: 🝊 Edge Match
name_in_editor: "Vtx : Edge Match"
subtitle: Find the neighbors that best match a given direction
summary: The **Edge Match** property identifies and outputs the properties of the edge connected to a vertex that best aligns with a specified direction, offering options for customizing direction sources and writing edge attributes like length, direction, and indices.
color: param
summary: TBD
splash: icons/icon_misc-write-index.svg
see_also: 
    - Working with Clusters
    - "Properties : Vtx"
tagged: 
    - node
    - vtx-property
    - clusters
nav_order: 7
outputs:
    -   name : Property
        desc : A single property writer
        pin : params
---

{% include header_card_node %}

The **Edge Match** property outputs the properties of the single connected edges that best matches a given direction.
{: .fs-5 .fw-400 } 

{% include img a='details/properties-vtx/vtx-extra-best-match-lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Origin           | Defines how the base direction is computed, either from the Node to the neighbor (*going outward*), or from the Neighbor to the node (*going inward*).  |
| Direction Source | Select where to read the direction to compare to from; either `Constant` or `Attribute`. |
| Direction <br>*(Constant or Attribute)* | Test Direction. |
| Transform Direction | If enabled, will transform the input direction (either `Constant` or `Attribute`) using the point' transform. |

---
## Dot Comparison Details
<br>
{% include embed id='settings-dot-comparison' %}

---
## Outputs

| Output       | Description          |
|:-------------|:------------------|
| <span class="eout">Direction Attribute</span><br>`FVector`           | If enabled, writes the `Edge`' direction to the specified attribute. |
| *Invert Direction* | *If enabled, will invert (`*-1`) the direction output above.* |
| <span class="eout">Length Attribute</span><br>`Double` | If enabled, writes the `Edge`' length to the specified attribute. |
| <span class="eout">Edge Index Attribute</span><br>`Int32` | If enabled, writes the `Edge`' point index to the specified attribute.<br>*This is useful for targeted/cherry-picking manipulations.* |
| <span class="eout">Vtx Index Attribute</span><br>`Int32` | If enabled, writes the `Vtx`' point index the selected `Edge` connects to, to the specified attribute.<br>*This is useful for targeted/cherry-picking manipulations.* |
| <span class="eout">Neighbor Count Attribute</span><br>`Int32` | If enabled, writes the `Vtx`' point neighbor count the selected `Edge` connects to, to the specified attribute.<br>*Amongst other things, this can come in handy to drive the {% include lk id='🝰 Heuristic Attribute' %}.* |