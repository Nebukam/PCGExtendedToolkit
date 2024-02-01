---
layout: page
parent: Graphs
grand_parent: Nodes
title: Partition Vertices
subtitle: Create per-cluster Vtx datasets
color: white
summary: The **Partition Vertices** splits input vtx into separate output groups, so that each Edge dataset is associated to a unique Vtx dataset (as opposed to a shared Vtx dataset for multiple edge groups)
splash: icons/icon_graphs-sanitize.svg
preview_img: docs/splash-partition-vertices.png
toc_img: placeholder.jpg
tagged:
    - graphs
see_also:
    - Working with Graphs
nav_order: 7
---

{% include header_card_node %}

*This node has no specific properties.*

> Contrary to other edge & graph processors, this node does **not** produce a sanitized result.  
> *If the input is unsanitized, you may have unexpected results.*  
{: .warning }

This node primarily exists to allow certain advanced operations such as easily finding individual convex hull of isolated clusters.  
This is not a default behavior as doing so increases processing load

---
# Inputs & Outputs
See {% include lk id='Working with Graphs' %}