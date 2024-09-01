---
layout: page
#grand_parent: All Nodes
parent: Staging
title: Asset Staging
subtitle: Prepare points before spawning.
summary: The **Asset Staging** node...
color: white
splash: icons/icon_paths-orient.svg
preview_img: placeholder.jpg
toc_img: placeholder.jpg
tagged: 
    - node
    - staging
nav_order: 1
inputs:
    -   name : In
        desc : Points to be prepared
        pin : points
    -   name : Attribute Set
        desc : Optional attribute set to be used as a collection, if the node is set-up that way.
        pin : params
outputs:
    -   name : Out
        desc : Modified points, with added attributes
        pin : points
---

{% include header_card_node %}

> WIP
{: .warning-hl }
