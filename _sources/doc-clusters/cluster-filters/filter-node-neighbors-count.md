---
layout: page
family: ClusterFilter
grand_parent: Clusters
parent: Cluster Filters
title: 🝖 Neighbors Count (Node)
name_in_editor: "Cluster Filter : Neighbors Count (Node)"
subtitle: Check a node' neighbors count
summary: The **Neighbors Count** filter compares the number of neighbors a node has against a specified value, offering a faster alternative to writing and comparing neighbor counts as attributes.
color: param
splash: icons/icon_misc-sort-points.svg
tagged: 
    - node
    - clusterfilter
    - nodefilter
    - clusters
    - filter
nav_order: 12
outputs:
    -   name : Node Filter
        extra_icon: OUT_FilterNode
        desc : A single cluster filter definition
        pin : params
---

{% include header_card_node %}

The Neighbors Count filter does a simple numeric comparison of it's number of neighbors against another value. *When you can, this is much faster and preferrable to writing the neighbor count to an attribute and then doing a regular numeric compare with it.*
{: .fs-5 .fw-400 } 

{% include img a='details/cluster-filters/filter-node-neighbors-count-lead.png' %}

# Properties
<br>

# Properties

| Property       | Description          |
|:-------------|:------------------|
|:**Settings** :|
| Priority           | Defines the order in which this flag operation will be processed.<br>*See {% include lk id='Flag Nodes' %}* |
| Comparison          | TBD |
| Compare Against           | TBD |
| Count (Constant)           | TBD |
| Operand A (First)           | TBD |
| Tolerance           | Equality tolerance for near-value comparisons. |