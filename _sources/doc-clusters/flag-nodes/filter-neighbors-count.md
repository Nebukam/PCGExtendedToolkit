---
layout: page
grand_parent: Clusters
parent: Flag Nodes
title: 🝖 Neighbors Count
name_in_editor: "Cluster Filter : Neighbors Count"
subtitle: Check a node' neighbors count
summary: The **Neighbor Count** filter ...
color: white
splash: icons/icon_misc-write-index.svg
preview_img: placeholder.jpg
toc_img: placeholder.jpg
tagged: 
    - node
    - clusterfilter
    - clusters
nav_order: 7
outputs:
    -   name : Filter
        desc : A single cluster filter definition
        pin : params
---

{% include header_card_node %}

The Neighbors Count filter does a simple numeric comparison of it's number of neighbors against another value. *When you can, this is much faster and preferrable to writing the neighbor count to an attribute and then doing a regular numeric compare with it.*
{: .fs-5 .fw-400 } 

{% include img a='placeholder-wide.jpg' %}

# Properties
<br>

# Properties

| Property       | Description          |
|:-------------|:------------------|
|: **Settings** :|
| Priority           | Defines the order in which this flag operation will be processed.<br>*See {% include lk id='Flag Nodes' %}* |
| Comparison          |  |
| Compare Against           | TBD |
| Count (Constant)           | TBD |
| Operand A (First)           | TBD |
| Tolerance           | |