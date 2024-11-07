---
layout: page
family: ClusterFilter
grand_parent: Clusters
parent: Cluster Filters
title: 🝖 Neighbors Count (Edge)
name_in_editor: "Cluster Filter : Neighbors Count (Edge)"
subtitle: Check a node' neighbors count
summary: The **Neighbors Count** filter compares the number of neighbors of each endpoints against a specified threhsold.
color: param
splash: icons/icon_misc-sort-points.svg
tagged: 
    - node
    - clusterfilter
    - edgefilter
    - clusters
    - filter
nav_order: 23
outputs:
    -   name : Edge Filter
        desc : A single cluster filter definition
        pin : params
---

{% include header_card_node %}

This filter takes the start & end points of an edge and compares the number of connections for each endpoint against a given threshold. 
{: .fs-5 .fw-400 } 

{% include img a='details/cluster-filters/filter-edge-neighbors-count-lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|: **Settings** ||
| Threshold Input | Type of threshold value. Can be a per-edge point `Attribute`, or an easily overridable `Constant`. |
| Threshold Constant           | Constant threshold value. |
| Threshold Attribute<br>`int32` | Attribute to read the threshold to compare the number of connections against. |
| Mode | Define how the number of endpoint' connections should be aggregated (or not) before the comparison is processed. |
| Comparison | How the number of neighbors should be compared against the threshold.<br>*See [Numeric comparisons](/PCGExtendedToolkit/doc-general/general-comparisons.html#numeric-comparisons).* |
| Tolerance | Equality tolerance for near-value comparisons. |
| Invert | If enabled, invert the result of the filter. |

### Mode

|: Handling     ||
|:-------------|:------------------|
| <span class="ebit">Sum</span>           | The sum of adjacencies will be compared against the specified threshold. |
| <span class="ebit">Any</span>           | At least one endpoint adjacency count must pass the comparison against the specified threshold. |
| <span class="ebit">Both</span>           | Both endpoint adjacency count must individually pass the comparison against the specified threshold. |
{: .enum }

