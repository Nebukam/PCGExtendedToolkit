---
layout: page
family: ClusterFilter
grand_parent: Clusters
parent: Cluster Filters
title: 🝖 Edge Direction (Node)
name_in_editor: "Cluster Filter : Edge Direction (Node)"
subtitle: Check if adjacent node meet specific conditions
summary: The **Edge Direction** filter compares the direction of connections using a dot product, providing precise control over how many directions meet the set conditions, allowing tests against both discrete and relative number of connections.
color: param
splash: icons/icon_misc-sort-points.svg
tagged: 
    - node
    - clusterfilter
    - nodefilter
    - clusters
    - filter
nav_order: 11
outputs:
    -   name : Node Filter
        desc : A single cluster filter definition
        pin : params
---

{% include header_card_node %}

The Edge Direction filter does a simple dot product comparison on each individual connections' direction, and offer fine grained control over what qualifies as a "success", based on how many directions passed the comparison or not. *What makes it equally useful and tricky to set-up is its ability to test against either a discrete number of connection, or relative ones.*
{: .fs-5 .fw-400 } 

{% include img a='details/cluster-filters/filter-node-edge-direction-lead.png' %}

# Properties
<br>

# Properties

| Property       | Description          |
|:-------------|:------------------|
|: **Settings** :|
| Priority           | Defines the order in which this flag operation will be processed.<br>*See {% include lk id='Flag Nodes' %}* |
| Comparison Quality          | Defines the method used for comparison.<br>- `Dot` is precise *(easily returns false)*<br>- `Hash` method is approximative *(easily returns true)* |

{% include embed id='settings-adjacency' %}

| Direction Order           | TBD |
| Compare Against           | TBD |
| Direction Constant           | TBD |
| Direction Attribute           | TBD |
| Comparison           | How to compare Operand A against Operand B<br>*See {% include lk id='Flag Nodes' %}* |
| Transform Direction           | TBD |
| Operand B (Neighbor)           | TBD |

---
## Dot Comparison Details
<br>
{% include embed id='settings-dot-comparison' %}

