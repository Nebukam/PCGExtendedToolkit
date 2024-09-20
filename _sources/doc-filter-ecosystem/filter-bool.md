---
layout: page
#grand_parent: Misc
parent: Filter Ecosystem
title: 🝖 Bool
name_in_editor: "Filter : Bool Compare"
subtitle: Performs a simple boolean comparison, converting numeric values to true (> 0) or false (<= 0).
color: white
summary: TBD
splash: icons/icon_misc-sort-points.svg
tagged: 
    - node
    - filter
    - misc
nav_order: 12
outputs:
    -   name : Filter
        desc : A single filter definition
        pin : params
---

{% include header_card_node %}

The **Bool Filter** lets you do a simple boolean comparison.
{: .fs-5 .fw-400 } 

Numeric values will be converted following these rules:
- `true` if the value is > 0
- `false` if the value is <= 0

{% include img a='details/filter-ecosystem/filter-bool-lead.png' %}

---
# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| **Operand A**          ||
| Operand A          | The first attribute of the comparison |
| Comparison | How to compare the two operand A & B. Either `Equal` (A == B) or `Not Equal` (A != B) |
| Compare Against | Type of operand B. Can be a per-point `Attribute`, or an easily overridable `Constant`. |

| **Operand B**          ||
| Operand B | Attribute that contains the per-point operand B value. |
| Operand B Constant | Constant operand B value. |
