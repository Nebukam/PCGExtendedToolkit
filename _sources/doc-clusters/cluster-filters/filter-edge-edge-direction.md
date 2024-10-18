---
layout: page
family: ClusterFilter
grand_parent: Clusters
parent: Cluster Filters
title: 🝖 Edge Direction (Edge)
name_in_editor: "Cluster Filter : Edge Direction (Edge)"
subtitle: Check if the edge direction is within a given range.
summary: The **Edge Direction** filter compares the direction of of the edge using a dot product.
color: param
splash: icons/icon_misc-sort-points.svg
tagged: 
    - node
    - clusterfilter
    - edgefilter
    - clusters
    - filter
nav_order: 20
outputs:
    -   name : Edge Filter
        desc : A single cluster filter definition
        pin : params
---

{% include header_card_node %}

WIP
{: .fs-5 .fw-400 } 

{% include img a='details/cluster-filters/filter-edge-edge-direction-lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|: **Settings (Operand A)** ||
|: Direction Settings | Used to determine which edge endpoint is either the start or end.<br>*See below.* |
| Comparison Quality          | Defines the method used for comparison.<br>- `Dot` is precise *(easily returns false)*<br>- `Hash` method is approximative *(easily returns true)* |

|: **Operand B** ||
| Compare Against | Type of operand B. Can be a per-edge point `Attribute`, or an easily overridable `Constant`. |
| Direction<br>`FVector` | Attribute to read the direction vector to compare the edge direction against. |
| Direction Constant | Constant direction vector to compare the edge direction against. |
| Transform Direction | If enabled, the direction to compare will be adjusted by the current edge point' transform.<br>*If disabled, the direction is in world space.* |

---
## Direction Settings
<br>
{% include embed id='settings-edge-direction' %}
{% include img_link a='explainers/edge-direction-method.png' %}

---
## Comparison Quality

{% include img a='details/settings/edge-direction.png' %}
<br>
{% include img a='details/settings/comparison-quality.png' %}

### Dot Comparison Details
*Only if Dot method is selected*
<br>
{% include embed id='settings-dot-comparison' %}

---
### Hash Comparison Details
*Only if Hash method is selected*
<br>
{% include embed id='settings-hash-comparison' %}