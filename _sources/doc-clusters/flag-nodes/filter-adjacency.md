---
layout: page
grand_parent: Clusters
parent: Flag Nodes
title: 🝖 Adjacency
name_in_editor: "Cluster Filter : Adjacency"
subtitle: Check if adjacent node meet specific conditions
summary: The **Adjacency** filter performs numeric comparisons on each connection of a Vtx, allowing precise control over success criteria based on the number of passed comparisons, offering the flexibility to test either discrete or relative connections.
color: blue
splash: icons/icon_edges-extras.svg
tagged: 
    - node
    - clusterfilter
    - clusters
    - filter
nav_order: 100
outputs:
    -   name : Filter
        desc : A single cluster filter definition
        pin : params
---

{% include header_card_node %}

The adjacency filter does a simple numeric comparison on each individual connections of a `Vtx`, and offer fine grained control over what qualifies as a "success", based on how many connections passed the comparison or not. *What makes it equally useful and tricky to set-up is its ability to test against either a discrete number of connection, or relative ones.*
{: .fs-5 .fw-400 } 

{% include img a='placeholder-wide.jpg' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|: **Settings** :|
| Priority           | Defines the order in which this flag operation will be processed.<br>*See {% include lk id='Flag Nodes' %}* |
| Adjacency Settings          | Defines how to deal with the number of adjacent nodes *as a criteria to the filter success*.<br>*Covered in more depth later in this page* |

| Compare Against           | TBD |
| Operand A Constant           | TBD |
| Operand A Attribute           | TBD |
| Comparison           | How to compare Operand A against Operand B<br>*See {% include lk id='Flag Nodes' %}* |
| Operand B Source           | TBD |
| Operand B (Neighbor)           | TBD |
| Tolerance           | Comparison tolerance, for non-strict comparison modes. |


{% include embed id='settings-adjacency' %}
