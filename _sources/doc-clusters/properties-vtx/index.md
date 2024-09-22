---
layout: page
family: SamplerNeighbor
grand_parent: All Nodes
parent: Clusters
title: "Properties : Vtx"
name_in_editor: "Cluster : Vtx Properties"
subtitle: Compute vtx extra data
color: white
summary: The **Vtx Properties** node allows you to compute and write additional data for vertices within a cluster. It utilizes external nodes to define individual property outputs. You can input multiple property nodes, ensuring unique names for each to avoid overwriting issues.
splash: icons/icon_edges-extras.svg
preview_img: previews/index-vtx-properties.png
has_children: true
see_also: 
    - Working with Clusters
tagged: 
    - node
    - edges
see_also: 
    - Interpolate
nav_order: 6
inputs:
    -   name : Vtx
        desc : Endpoints of the input Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the input Vtxs
        pin : points
    -   name : Properties 
        desc : Individual 'Vtx Property' nodes
        pin : params
outputs:
    -   name : Vtx
        desc : Endpoints of the output Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the output Vtxs
        pin : points
   
---

{% include header_card_node %}

The **Vtx Properties** node lets you extract & write cluster-related properties to individual Vtx points. Individual output definitions/settings use external nodes.
{: .fs-5 .fw-400 } 

{% include img a='details/properties-vtx/properties-vtx-lead.png' %}

# Property Nodes

Properties input are at the core of the **Vtx Properties** node.  
You can connect as many properties node as you'd like to the `Properties` input.
{: .fs-5 .fw-400 } 

> Make sure not to use duplicate output names otherwise they will overwrite each other in no guaranteed order.
{: .warning }
<br>

{% include card_childs tagged='vtx-property' %}

---
# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Edge Count           | If enabled, output the number of `edges` this `vtx` is connected to.  |
| Normal | If enabled, output the averaged normal of the `vtx` based on all connected `edges`.<br>*This output is hardly usable for highly 3-dimensional nodes.* |
