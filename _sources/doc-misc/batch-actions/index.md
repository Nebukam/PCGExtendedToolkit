---
layout: page
family: MiscAdd
#grand_parent: All Nodes
parent: Misc
title: Batch Actions
name_in_editor: "Batch Actions"
subtitle: A node that batche-process individual actions
summary: The **Batch Actions** node is executing all its input "actions" in a single node. It's especially handy if you want to externalize some processing, or have subgraph that offer the opportunity for additional processing.
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