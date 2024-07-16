---
layout: page
#grand_parent: All Nodes
parent: Clusters
title: Partition Vertices
subtitle: Create per-cluster Vtx datasets
summary: The **Partition Vertices** splits input vtx into separate output groups, so that each Edge dataset is associated to a unique Vtx dataset (as opposed to a shared Vtx dataset for multiple edge groups)
color: white
splash: icons/icon_graphs-sanitize.svg
preview_img: docs/splash-partition-vertices.png
toc_img: placeholder.jpg
tagged:
    - node
    - clusters
see_also:
    - Working with Clusters
nav_order: 50
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

> Contrary to other edge & cluster processors, this node does **not** produce a sanitized result.  
> *If the input is unsanitized, you may have unexpected results.*  
{: .warning }

This node primarily exists to allow certain advanced operations such as easily finding individual convex hull of isolated clusters.  
*This is not a default behavior as doing so slightly increases processing times.*

---
# Inputs & Outputs
## Vtx & Edges
See {% include lk id='Working with Clusters' %}