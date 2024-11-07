---
layout: page
family: Filter
#grand_parent: Misc
parent: Filter Ecosystem
title: 🝖 Compare (Numeric)
name_in_editor: "Filter : Compare (Numeric)"
subtitle: The **Numeric Comparison Filter** compares the arithmetic value of two attributes
color: param
summary: "-"
splash: icons/icon_misc-sort-points.svg
tagged: 
    - node
    - filter
    - pointfilter
    - misc
nav_order: 10
outputs:
    -   name : Filter
        desc : A single filter definition
        pin : params
---

{% include header_card_node %}

The **Compare (Numeric)** compares two attributes against each other.
{: .fs-5 .fw-400 } 

> Note that each value is converted to a `double` under the hood, so you can't compare multi-component value with it.
{: .warning }

{% include img a='details/filter-ecosystem/filter-compare-numeric-lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| **Operand A**          ||
| Operand A          | The first attribute of the comparison. |

| **Comparison**          ||
| Comparison | How to compare A against B.<br>*See [Numeric comparisons](/PCGExtendedToolkit/doc-general/general-comparisons.html#numeric-comparisons).* |

| **Operand B**          ||
| Compare Against | Type of operand B. Can be a per-point `Attribute`, or an easily overridable `Constant`. |
| Operand B <br>*(Constant or Attribute)* | Operand B, or second value to compare to the first. |
| Tolerance | Equality tolerance for near-value comparisons. |

---
## Comparison modes
<br>
{% include embed id='settings-compare-numeric' %}