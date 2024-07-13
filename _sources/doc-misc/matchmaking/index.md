---
layout: page
#grand_parent: All Nodes
parent: Misc
title: Matchmaking
subtitle: Unified approach to Match & Set.
summary: The **Matchmaking** node is writing things based on filters.
color: white
splash: icons/icon_misc-write-index.svg
preview_img: placeholder.jpg
toc_img: placeholder.jpg
has_children: true
tagged: 
    - node
    - misc
nav_order: 7
inputs:
    -   name : In
        desc : Points to be matchmaked
        pin : points
    -   name : Matchmakers
        desc : Matchmakes to be processed
        pin : params
outputs:
    -   name : Out
        desc : In after matchmaking.
        pin : points
---

{% include header_card_node %}

> DOC TDB
{: .warning }