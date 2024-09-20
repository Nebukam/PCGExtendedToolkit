---
layout: page
#grand_parent: Misc
parent: Filter Ecosystem
title: 🝖 Modulo Comparison
name_in_editor: "Filter : Modulo Compare"
subtitle: Compares the modulo of two attributes against a third operand, with configurable comparisons and tolerance.
color: white
summary: TBD
splash: icons/icon_misc-sort-points.svg
tagged: 
    - node
    - filter
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
| Compare B (Attribute) | Attribute to use as Operand B. |
| Compare B (Constant) | Constant value to use as Operand B. |

| **Comparison**          ||
| Comparison | How to compare the two `A % B` against `C`.<br>*See [Numeric comparisons](/PCGExtendedToolkit/doc-general/comparisons.html#numeric-comparisons).* |
| Compare Against | Type of operand C. Can be a per-point `Attribute`, or an easily overridable `Constant`. |

| **Operand C**          ||
| Operand C | Attribute that contains the per-point operand B value. |
| Operand C (Attribute) | Attribute to use as Operand C. |
| Operand C (Constant) | Constant value to use as Operand C. |
| Tolerance | Equality tolerance using an approximative comparison. |

---
## Comparison modes
<br>
{% include embed id='settings-compare-numeric' %}