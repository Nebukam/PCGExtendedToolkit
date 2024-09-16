---
layout: page
grand_parent: Clusters
parent: Flag Nodes
title: 🝖 Edge Direction
name_in_editor: "Cluster Filter : Edge Direction"
subtitle: Check if adjacent node meet specific conditions
summary: The **Edge Direction** filter compares the direction of connections using a dot product, providing precise control over how many directions meet the set conditions, allowing tests against both discrete and relative number of connections.
color: blue
splash: icons/icon_edges-extras.svg
tagged: 
    - node
    - clusterfilter
    - clusters
    - filter
nav_order: 101
outputs:
    -   name : Filter
        desc : A single cluster filter definition
        pin : params
---

{% include header_card_node %}

The Edge Direction filter does a simple dot product comparison on each individual connections' direction, and offer fine grained control over what qualifies as a "success", based on how many directions passed the comparison or not. *What makes it equally useful and tricky to set-up is its ability to test against either a discrete number of connection, or relative ones.*
{: .fs-5 .fw-400 } 

{% include img a='placeholder-wide.jpg' %}

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
### Dot Comparison Details
{% include embed id='settings-dot-comparison' %}

