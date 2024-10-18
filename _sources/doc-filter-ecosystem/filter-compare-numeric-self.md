---
layout: page
family: Filter
#grand_parent: Misc
parent: Filter Ecosystem
title: 🝖 Self Compare (Numeric)
name_in_editor: "Filter : Self Compare (Numeric)"
subtitle: Compares the numeric value at one index against the same attribute at another index.
color: param
summary: "-" 
splash: icons/icon_misc-sort-points.svg
tagged: 
    - node
    - filter
    - pointfilter
    - misc
nav_order: 31
outputs:
    -   name : Filter
        desc : A single filter definition
        pin : params
---

{% include header_card_node %}

The **Self Compare (Numeric)** compares the point' value of a single attribute against that same attribute at another index.
{: .fs-5 .fw-400 } 

> Note that each value is converted to a `double` under the hood, so you can't compare multi-component value with it.
{: .warning }

{% include img a='details/filter-ecosystem/filter-compare-numeric-self-lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| **Operand A**          ||
| Operand A          | The attribute to read compared values from. |

| **Comparison**          ||
| Comparison | How to compare A against B.<br>*See [Numeric comparisons](/PCGExtendedToolkit/doc-general/comparisons.html#numeric-comparisons).* |
| Tolerance | Equality tolerance using an approximative comparison. |

| **Index**          ||
| Index Mode          | Choose how to process the Index value as either a `Pick` (specific index), or an `Offset` applied to the currently tested point' index. |
| Compare Against | Type of values for the Index. Can be a per-point `Attribute`, or an easily overridable `Constant`. |
| Index <br>*(Constant or Attribute)* | Value to use as Index/Offset. |

{% include img a='details/filter-ecosystem/filter-compare-self-explainer.png' %}

### Index Safety
{% include embed id='settings-index-safety' %}

---
## Comparison modes
<br>
{% include embed id='settings-compare-numeric' %}