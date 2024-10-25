---
layout: page
family: MiscAdd
#grand_parent: All Nodes
parent: Misc
title: Attribute Stats
name_in_editor: "Attribute Stats"
subtitle: Compute attribute statistics & unique values
summary: The **Attribute Stats** node ...
color: white
splash: icons/icon_misc-points-to-bounds.svg
tagged:
    - node
    - misc
nav_order: 5
inputs:
    -   name : In
        desc : Points to process for attributes
        pin : points
outputs:
    -   name : Out
        desc : Processed point with an added identifier tag to easily map to stats
        pin : points
    -   name : Attribute Stats
        desc : Per-attribute stats. One row per input point dataset
        pin : params
    -   name : Unique Values
        desc : Per-attribute, per-dataset unique values.
        pin : params
---

{% include header_card_node %}

WIP
{: .fs-5 .fw-400 } 

{% include img a='details/sampling-pack-actor-data/lead.png' %}

# Properties
<br>

WIP / TBD