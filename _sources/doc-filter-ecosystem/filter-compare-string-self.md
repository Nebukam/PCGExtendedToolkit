---
layout: page
family: Filter
#grand_parent: Misc
parent: Filter Ecosystem
title: 🝖 Self Compare (String)
name_in_editor: "Filter : Self Compare (String)"
subtitle: Compares the string value at one index against the same attribute at another index.
color: white
summary: "-"
splash: icons/icon_misc-sort-points.svg
tagged: 
    - node
    - filter
    - misc
nav_order: 32
outputs:
    -   name : Filter
        desc : A single filter definition
        pin : params
---

{% include header_card_node %}

The **Self Compare (String)** compares the point' value of a single attribute against that same attribute at another index.
{: .fs-5 .fw-400 } 

{% include img a='details/filter-ecosystem/filter-compare-string-self-lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| **Operand A**          ||
| Operand A          | The attribute to read compared values from. |

| **Comparison**          ||
| Comparison | How to compare A against B.<br>*See [Numeric comparisons](/PCGExtendedToolkit/doc-general/comparisons.html#string-comparisons).* |
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
{% include embed id='settings-compare-string' %}