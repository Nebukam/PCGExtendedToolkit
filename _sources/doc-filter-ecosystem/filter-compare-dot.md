---
layout: page
family: Filter
#grand_parent: Misc
parent: Filter Ecosystem
title: 🝖 Dot Product
name_in_editor: "Filter : Dot"
subtitle: Compares the dot product of two direction vectors against a third value.
color: param
summary: "-"
splash: icons/icon_misc-sort-points.svg
tagged: 
    - node
    - filter
    - pointfilter
    - misc
nav_order: 15
outputs:
    -   name : Filter
        extra_icon: OUT_Filter
        desc : A single filter definition
        pin : params
---

{% include header_card_node %}

The **Dot Filter** compares the dot product value of two directions.
{: .fs-5 .fw-400 } 


{% include img a='details/filter-ecosystem/filter-compare-dot-lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| **Operand A**          ||
| Operand A          | The first attribute of the comparison.<br>*Read as `FVector` direction.* |
| Transform Operand A          | If enabled, the Operand A direction will be transformed by the tested' point transform. |

| **Operand B**          ||
| Compare Against | Type of operand B. Can be a per-point `Attribute`, or an easily overridable `Constant`. |
| Operand B<br>*(Constant or Attribute)* | Operand B value, used to compute the dot product with.<br>*Read as `FVector` direction.* |
| Transform Operand A          | If enabled, the Operand A direction will be transformed by the tested' point transform. |

---
## Dot Comparison Details
<br>
{% include embed id='settings-dot-comparison' %}

---
## Comparison modes
<br>
{% include embed id='settings-compare-numeric' %}

