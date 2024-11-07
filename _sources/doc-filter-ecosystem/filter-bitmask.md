---
layout: page
family: Filter
#grand_parent: Misc
parent: Filter Ecosystem
title: 🝖 Bitmask
name_in_editor: "Filter : Bitmask"
subtitle: Checks specific flags in an int64 bitmask attribute with configurable mask types, comparisons, and an option to invert results.
color: param
summary: "-"
splash: icons/icon_misc-sort-points.svg
tagged: 
    - node
    - filter
    - pointfilter
    - misc
nav_order: 22
outputs:
    -   name : Filter
        desc : A single filter definition
        pin : params
---

{% include header_card_node %}

The **Bitmask Filter** lets you check whether certain flags are set or not in an `int64` bitmask attribute.
{: .fs-5 .fw-400 } 

{% include img a='details/filter-ecosystem/filter-bitmask-lead.png' %}

---
# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| Flags Attributes          | The is the name of the attribtue which value will be tested.<br>*It is expected to be an `int64`.* |
| Mask Type | Type of source mask. Can be a per-point `Attribute`, or an easily overridable `Constant`. |
| Comparison | *See [Bitmask comparisons](/PCGExtendedToolkit/doc-general/general-comparisons.html#bitmask-comparisons).* |
| Bitmask <br>*(Constant or Attribute)* | Mask value.<br>*Strictly expects an `int64`.* |
| Invert Result | If enabled, invert the result of the filter. |

---
## Comparison modes
<br>
{% include embed id='settings-compare-bitmask' %}