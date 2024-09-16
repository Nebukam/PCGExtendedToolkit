---
layout: page
grand_parent: Clusters
parent: Packing Clusters
title: Pack Clusters
name_in_editor: "Cluster : Pack"
subtitle: Pack Clusters
summary: The **Pack Clusters** node combines individual vertex-edge pairs into a single data set for storage and reuse, allowing for selective attribute and tag preservation, but must be unpacked to be used as a cluster.
color: red
splash: icons/icon_edges-extras.svg
tagged: 
    - node
    - clusters
nav_order: 200
see_also:
    - Working with Clusters
inputs:
    -   name : Vtx
        desc : Endpoints of the input Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the input Vtxs
        pin : points
outputs:
    -   name : Packed Clusters
        desc : Individually packed vtx/edges pairs
        pin : points
---

{% include header_card_node %}

The **Pack Cluster** nodes outputs individual clusters into **a single points data that contains both `Vtx` and `Edges`**. As such, it is not directly usable as a cluster object, until it is unpacked using the {% include lk id='Unpack Clusters' %} node. 
{: .fs-5 .fw-400 } 

> This node is primarily designed so you can save clusters/diagrams to assets in order to re-use them without the associated cost of building them. **Make sure to flatten the data before you save it, otherwise attributes may not carry over properly.**
{: .infos-hl }
<br>

{% include img a='details/packing-clusters/clusters-pack-clusters-lead.png' %}

---
## Carry Over Settings
<br>
{% include embed id='settings-carry-over' %}
