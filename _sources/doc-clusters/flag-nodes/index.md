---
layout: page
family: Edge
#grand_parent: All Nodes
parent: Clusters
title: Flag Nodes
name_in_editor: "Cluster : Flag Nodes"
subtitle: Find conditional-based states to nodes within a graph
summary: The **Flag Nodes** node identifies and marks complex, non-mutually exclusive states within a cluster by applying filters and conditions to bitmask attributes, allowing for fine-tuned control over node selection and flagging based on specific criteria.
color: red
splash: icons/icon_edges-extras.svg
preview_img: previews/index-flag-nodes.png
has_children: true
see_also:
    - Working with Clusters
    - Node Flag
    - Filter Ecosystem
tagged: 
    - node
    - clusters
nav_order: 13
inputs:
    -   name : Vtx
        desc : Endpoints of the input Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the input Vtxs
        pin : points
    -   name : Flags
        desc : Node flags and their associated filters
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

The **Flag Nodes** node helps you find & mark complex, non-mutually exclusive states on each `Vtx` of a cluster. It takes any number of {% include lk id='Node Flag' %} and processes their associated filters & conditions in order to update a bitmask attribute on individual points. **It is primarily used to identify nodes within a graph that meet very specific set of criterias that are not mutually exclusive**.  
Output attribute can then be filtered using {% include lk id='🝖 Bitmask' %}, and works with the {% include lk id='Bitmasks' %} toolset.
{: .fs-5 .fw-400 } 

Bitmasks & bit fields can be obscure to work with if it's not something you're used to. Sadly the [Wikipedia article](https://en.wikipedia.org/wiki/Mask_(computing)) on the topic isn't super helpful, and digging into the specifics wouldn't be very productive either. *If you have a user-friendly resources I could add to the doc, please poke me.*  

> Bitmasks fields in PCGEx are using a `int64`, which **loosely translate as storing 63 boolean values on a single attribute**.
{: .infos }


{% include img a='details/flag-nodes/flag-nodes-lead.png' %}

# Order of operation

Connected {% include lk id='Flag Node' %}s are first sorted (ascending) using their individual `Priority`, and then processed in that order; **using the result of the previous operation, if any.**  
What this means is higher priorities have the ability to radically change the entire bitmask, should you chose to.  
*It synergize very well with the {% include lk id='Conditional Actions' %} node that lets you match & set attributes using filters; such as the {% include lk id='🝖 Bitmask' %} one.*

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|:**Settings**||
| Flag Attribute<br>`int64`           | This is the name of the attribute the final bitmask will be written to.  |
| Initial Flag | This is the flag to start operating from.<br>*Setting this value manually isn't recommended, instead use a {% include lk id='Bitmask' %} and plug it into the override pin.*  |

---
## Individual Node Flag
<br>
{% include card_childs tagged="flagger" %}

---
## Available Cluster Filters
<br>
{% include card_any tagged="clusterfilter" %}

---
## Available Regular Filters
<br>
{% include card_any tagged="filter" %}