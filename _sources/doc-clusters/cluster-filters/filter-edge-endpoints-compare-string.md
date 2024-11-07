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
see_also:
    - Refine
    - 🝖 Endpoints Compare (Numeric)
outputs:
    -   name : Edge Filter
        desc : A single cluster filter definition
        pin : params
---

{% include header_card_node %}

This filter takes the start & end points of an edge and compare the value of a single attribute on these points. It's most useful when used with the {% include lk id='Refine' %} node to either keep or remove edge that connect to different areas.
{: .fs-5 .fw-400 } 

{% include img a='details/cluster-filters/filter-edge-endpoints-compare-string-lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|:**Settings** ||
| Attribute | Attribute to read on edges' endpoints |
| Comparison           | How to compare each endpoint value against the other.<br>*See [String comparisons](/PCGExtendedToolkit/doc-general/general-comparisons.html#string-comparisons).* |
| Tolerance | Equality tolerance for near-value comparisons. |
| Invert | If enabled, invert the result of the filter. |