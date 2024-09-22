---
layout: page
family: SamplerNeighbor
grand_parent: Clusters
parent: "Properties : Vtx"
title: 🝊 Special Neighbors
name_in_editor: "Vtx : Special Neighbors"
subtitle: Gather infos from special-case neighbors
summary: The **Special Neighbors** property outputs information about two key neighbors of a vertex — the one with the most connections (Largest Neighbor) and the one with the fewest connections (Smallest Neighbor). You can extract various attributes, such as edge direction, length, and vertex indices, for both of these special neighbors.
color: param
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

The **Special Neighbor** property outputs the properties of the two "special" connected neighbors -- special in the *outlier* sense : the **Largest** connected vtx, as well as the **Smallest** connected vtx -- in terms of number of connectons.
{: .fs-5 .fw-400 } 

{% include img a='details/properties-vtx/vtx-extra-special-neighbors-lead.png' %}

# Properties
<br>

---
## Largest Neighbor

> The Largest Neighbor is the connected `Vtx` with the largest number of connections.

| Output       | Description          |
|:-------------|:------------------|
| <span class="eout">Direction Attribute</span><br>`FVector`           | If enabled, writes the `Edge`' direction to the specified attribute. |
| *Invert Direction* | *If enabled, will invert (`*-1`) the direction output above.* |
| <span class="eout">Length Attribute</span><br>`Double` | If enabled, writes the `Edge`'' length to the specified attribute. |
| <span class="eout">Edge Index Attribute</span><br>`Int32` | If enabled, writes the `Edge`'' point index to the specified attribute.<br>*This is useful for targeted/cherry-picking manipulations.* |
| <span class="eout">Vtx Index Attribute</span><br>`Int32` | If enabled, writes the `Vtx`' point index the `Edge`' connects to, to the specified attribute.<br>*This is useful for targeted/cherry-picking manipulations.* |
| <span class="eout">Neighbor Count Attribute</span><br>`Int32` | If enabled, writes the `Vtx`' point neighbor count the `Edge`' connects to, to the specified attribute.<br>*Amongst other things, this can come in handy to drive the {% include lk id='🝰 Heuristic Attribute' %}.* |

---
## Smallest Neighbor

> The Smallest Neighbor is the connected `Vtx` with the smallest number of connections.

| Output       | Description          |
|:-------------|:------------------|
| <span class="eout">Direction Attribute</span><br>`FVector`           | If enabled, writes the `Edge`' direction to the specified attribute. |
| *Invert Direction* | *If enabled, will invert (`*-1`) the direction output above.* |
| <span class="eout">Length Attribute</span><br>`Double` | If enabled, writes the `Edge`'' length to the specified attribute. |
| <span class="eout">Edge Index Attribute</span><br>`Int32` | If enabled, writes the `Edge`'' point index to the specified attribute.<br>*This is useful for targeted/cherry-picking manipulations.* |
| <span class="eout">Vtx Index Attribute</span><br>`Int32` | If enabled, writes the `Vtx`' point index the `Edge`' connects to, to the specified attribute.<br>*This is useful for targeted/cherry-picking manipulations.* |
| <span class="eout">Neighbor Count Attribute</span><br>`Int32` | If enabled, writes the `Vtx`' point neighbor count the `Edge`' connects to, to the specified attribute.<br>*Amongst other things, this can come in handy to drive the {% include lk id='🝰 Heuristic Attribute' %}.* |