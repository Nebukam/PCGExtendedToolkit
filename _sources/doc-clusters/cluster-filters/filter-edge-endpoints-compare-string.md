---
layout: page
family: ClusterFilter
grand_parent: Clusters
parent: Cluster Filters
title: 🝖 Endpoints Compare (String)
name_in_editor: "Cluster Filter : Endpoints Compare (String)"
subtitle: Compare the value of an attribute on each of the edge endpoint.
summary: The **Endpoint Comparison** filter performs string comparison of the values of an attribute on each endpoint, against each other.
color: param
splash: icons/icon_misc-sort-points.svg
tagged: 
    - node
    - clusterfilter
    - edgefilter
    - clusters
    - filter
nav_order: 22
outputs:
    -   name : Edge Filter
        desc : A single cluster filter definition
        pin : params
---

{% include header_card_node %}

WIP
{: .fs-5 .fw-400 } 

{% include img a='details/cluster-filters/filter-edge-endpoints-compare-string-lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|: **Settings** ||
| Attribute | Attribute to read on edges' endpoints |
| Comparison           | How to compare each endpoint value against the other.<br>*See [String comparisons](/PCGExtendedToolkit/doc-general/comparisons.html#string-comparisons).* |
| Invert | If enabled, invert the result of the filter. |