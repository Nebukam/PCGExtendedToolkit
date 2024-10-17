---
layout: page
family: ClusterFilter
grand_parent: Clusters
parent: Cluster Filters
title: 🝖 Endpoints Compare (Numeric)
name_in_editor: "Cluster Filter : Endpoints Compare (Numeric)"
subtitle: Compare the value of an attribute on each of the edge endpoint.
summary: The **Endpoint Comparison** filter performs a numeric comparison of the values of an attribute on each endpoint, against each other.
color: param
splash: icons/icon_misc-sort-points.svg
tagged: 
    - node
    - clusterfilter
    - edgefilter
    - clusters
    - filter
nav_order: 21
outputs:
    -   name : Edge Filter
        desc : A single cluster filter definition
        pin : params
---

{% include header_card_node %}

WIP
{: .fs-5 .fw-400 } 

{% include img a='details/cluster-filters/filter-edge-endpoints-compare-numeric-lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|: **Settings** ||
| Attribute | Attribute to read on edges' endpoints |
| Comparison           | How to compare each endpoint value against the other.<br>*See [Numeric comparisons](/PCGExtendedToolkit/doc-general/comparisons.html#numeric-comparisons).* |
| Invert | If enabled, invert the result of the filter. |