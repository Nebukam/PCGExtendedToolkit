---
layout: page
family: Filter
#grand_parent: Misc
parent: Filter Ecosystem
title: 🝖 Compare (String)
name_in_editor: "Filter : Compare (String)"
subtitle: Compares two string-like attributes against each other.
color: param
summary: "-"
splash: icons/icon_misc-sort-points.svg
tagged: 
    - node
    - filter
    - pointfilter
    - misc
nav_order: 11
outputs:
    -   name : Filter
        desc : A single filter definition
        pin : params
---

{% include header_card_node %}

The **Compare (String)** compares two string-like attributes against each other.
{: .fs-5 .fw-400 } 

{% include img a='details/filter-ecosystem/filter-compare-string-lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| **Operand A**          ||
| Operand A          | The first attribute of the comparison. |

| **Comparison**          ||
| Comparison | How to compare A against B.<br>*See [String comparisons](/PCGExtendedToolkit/doc-general/comparisons.html#string-comparisons).* |

| **Operand B**          ||
| Compare Against | Type of operand B. Can be a per-point `Attribute`, or an easily overridable `Constant`. |
| Operand B <br>*(Constant or Attribute)* | Operand B' value. |
| Tolerance | Equality tolerance for near-value comparisons. |

---
## Comparison modes
<br>
{% include embed id='settings-compare-string' %}