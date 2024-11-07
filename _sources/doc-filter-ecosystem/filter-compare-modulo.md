---
layout: page
family: Filter
#grand_parent: Misc
parent: Filter Ecosystem
title: 🝖 Modulo Comparison
name_in_editor: "Filter : Modulo Compare"
subtitle: Compares the modulo of two attributes against a third operand, with configurable comparisons and tolerance.
color: param
summary: "-"
splash: icons/icon_misc-sort-points.svg
tagged: 
    - node
    - filter
    - pointfilter
    - misc
nav_order: 20
outputs:
    -   name : Filter
        desc : A single filter definition
        pin : params
---

{% include header_card_node %}

The **Modulo Filter** compares the modulo of two values against a third.
{: .fs-5 .fw-400 } 


{% include img a='details/filter-ecosystem/filter-compare-modulo-lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| **Operand A**          ||
| Operand A          | The first attribute of the comparison. |

| **Operand B**          ||
| Operand B Source          |  Type of operand B. Can be a per-point `Attribute`, or an easily overridable `Constant`. |
| Compare B <br>*(Constant or Attribute)* | Operand B used for modulo. |

| **Comparison**          ||
| Comparison | How to compare the two `A % B` against `C`.<br>*See [Numeric comparisons](/PCGExtendedToolkit/doc-general/general-comparisons.html#numeric-comparisons).* |

| **Operand C**          ||
| Compare Against | Type of operand C. Can be a per-point `Attribute`, or an easily overridable `Constant`. |
| Operand C | Attribute that contains the per-point operand B value. |
| Operand C <br>*(Constant or Attribute)* | Operand C. *This value will be tested against the modulo' result.* |
| Tolerance | Equality tolerance for near-value comparisons. |

---
## Comparison modes
<br>
{% include embed id='settings-compare-numeric' %}