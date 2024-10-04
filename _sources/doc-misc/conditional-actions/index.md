---
layout: page
family: MiscAdd
#grand_parent: All Nodes
parent: Misc
title: Conditional Actions
name_in_editor: "Conditional Actions"
subtitle: Unified approach to Match & Set.
summary: The **Conditional Actions** node is executing "actions" based on filters.
color: white
splash: icons/icon_misc-write-index.svg
preview_img: previews/index-conditional-actions.png
has_children: true
tagged: 
    - node
    - misc
nav_order: 7
inputs:
    -   name : In
        desc : Points to be matchmaked
        pin : points
    -   name : Actions
        desc : Actions to be processed
        pin : params
outputs:
    -   name : Out
        desc : In after actions.
        pin : points
---

{% include header_card_node %}

# Properties
<br>

> DOC TDB
{: .warning }

---
## Available Actions
<br>
{% include card_childs tagged='action' %}