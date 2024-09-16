---
layout: page
grand_parent: Clusters
parent: "Properties : Vtx"
title: 🝊 Special Edges
name_in_editor: "Vtx : Special Edges"
subtitle: Gather infos from special-case edges
summary: The **Special Edges** property outputs data about the two most extreme edges connected to a vertex — the shortest and longest — allowing users to capture attributes like direction, length, and indices for both, with an additional option to average all connected edges.
color: blue
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

The **Special Edges** property outputs the properties of the two "special" connected edges -- special in the *outlier* sense : the **Shortest** connected edge, as well as the **Longest** connected edge.
{: .fs-5 .fw-400 } 

{% include img a='details/properties-vtx/vtx-extra-special-edges-lead.png' %}

# Properties
<br>

---
## Shortest Edge

| Output       | Description          |
|:-------------|:------------------|
| Direction Attribute<br>`FVector`           | If enabled, writes the Shortest `Edge`' direction to the specified attribute. |
| *Invert Direction* | *If enabled, will invert (`*-1`) the direction output above.* |
| Length Attribute<br>`Double` | If enabled, writes the Shortest `Edge`'' length to the specified attribute. |
| Edge Index Attribute<br>`Int32` | If enabled, writes the Shortest `Edge`'' point index to the specified attribute.<br>*This is useful for targeted/cherry-picking manipulations.* |
| Vtx Index Attribute<br>`Int32` | If enabled, writes the `Vtx`' point index the Shortest `Edge`' connects to, to the specified attribute.<br>*This is useful for targeted/cherry-picking manipulations.* |
| Neighbor Count Attribute<br>`Int32` | If enabled, writes the `Vtx`' point neighbor count the Shortest `Edge`' connects to, to the specified attribute.<br>*Amongst other things, this can come in handy to drive the {% include lk id='🝰 Heuristic Attribute' %}.* |

---
## Longest Edge

| Output       | Description          |
|:-------------|:------------------|
| Direction Attribute<br>`FVector`           | If enabled, writes the Longest `Edge`' direction to the specified attribute. |
| *Invert Direction* | *If enabled, will invert (`*-1`) the direction output above.* |
| Length Attribute<br>`Double` | If enabled, writes the Longest `Edge`'' length to the specified attribute. |
| Edge Index Attribute<br>`Int32` | If enabled, writes the Longest `Edge`'' point index to the specified attribute.<br>*This is useful for targeted/cherry-picking manipulations.* |
| Vtx Index Attribute<br>`Int32` | If enabled, writes the `Vtx`' point index the Longest `Edge`' connects to, to the specified attribute.<br>*This is useful for targeted/cherry-picking manipulations.* |
| Neighbor Count Attribute<br>`Int32` | If enabled, writes the `Vtx`' point neighbor count the Longest `Edge`' connects to, to the specified attribute.<br>*Amongst other things, this can come in handy to drive the {% include lk id='🝰 Heuristic Attribute' %}.* |

---
## Average Edge
<br>
A sub-selection of all connected' edges  average.

| Output       | Description          |
|:-------------|:------------------|
| Direction Attribute<br>`FVector`           | If enabled, writes the averaged direction of all connected edges. |
| *Invert Direction* | *If enabled, will invert (`*-1`) the direction output above.* |
| Length Attribute<br>`Double` | If enabled, writes the averaged length of all connected edges. |