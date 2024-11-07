---
layout: page
family: ClusterFilter
grand_parent: Clusters
parent: Cluster Filters
title: 🝖 Adjacency (Node)
name_in_editor: "Cluster Filter : Adjacency (Node)"
subtitle: Check if adjacent node meet specific conditions
summary: The **Adjacency** filter performs numeric comparisons on each connection of a Vtx, allowing precise control over success criteria based on the number of passed comparisons, offering the flexibility to test either discrete or relative connections.
color: param
splash: icons/icon_misc-sort-points.svg
tagged: 
    - node
    - clusterfilter
    - nodefilter
    - clusters
    - filter
nav_order: 10
outputs:
    -   name : Node Filter
        desc : A single cluster filter definition
        pin : params
---

{% include header_card_node %}

The adjacency filter does a simple numeric comparison on each individual connections of a `Vtx`, and offer fine grained control over what qualifies as a "success", based on how many connections passed the comparison or not. *What makes it equally useful and tricky to set-up is its ability to test against either a discrete number of connection, or relative ones.*
{: .fs-5 .fw-400 } 

{% include img a='details/cluster-filters/filter-node-adjacency-lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|:**Settings** :|
| Priority           | Defines the order in which this flag operation will be processed.<br>*See {% include lk id='Flag Nodes' %}* |
| Adjacency Settings          | Defines how to deal with the number of adjacent nodes *as a criteria to the filter success*.<br>*Covered in more depth later in this page* |

| Compare Against           | TBD |
| Operand A Constant           | TBD |
| Operand A Attribute           | TBD |
| Comparison           | How to compare Operand A against Operand B<br>*See [Numeric comparisons](/PCGExtendedToolkit/doc-general/general-comparisons.html#numeric-comparisons).* |
| Operand B Source           | TBD |
| Operand B (Neighbor)           | TBD |
| Tolerance           | Equality tolerance for near-value comparisons. |


{% include embed id='settings-adjacency' %}
