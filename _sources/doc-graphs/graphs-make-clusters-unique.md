---
layout: page
#grand_parent: All Nodes
parent: Graphs
title: Make Clusters Unique
subtitle: Create a forward copy of input clusters with unique tags.
color: white
summary: The **Copy Cluster** ...
splash: icons/icon_graphs-sanitize.svg
preview_img: placeholder.jpg
toc_img: placeholder.jpg
tagged:
    - node
    - graphs
see_also:
    - Working with Graphs
nav_order: 11
inputs:
    -   name : Vtx
        desc : Endpoints of the input Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the input Vtxs
        pin : points
outputs:
    -   name : Vtx
        desc : Endpoints of the output Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the output Vtxs
        pin : points
---

{% include header_card_node %}

*This node has no specific properties.*

> This node creates a duplicate of the input data with new unique cluster tags.
{: .infos-hl }

This node primarily exists to allow certain advanced operations such as copying an existing graph configuration, modify it and then fuse it with the original one.  


---
# Inputs & Outputs
## Vtx & Edges
See {% include lk id='Working with Graphs' %}