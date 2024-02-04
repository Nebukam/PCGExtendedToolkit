---
layout: page
parent: Graphs
grand_parent: All Nodes
title: Sanitize Clusters
subtitle: Ensure clusters are clean and complete
color: white
summary: The **Sanitize Clusters** ensures clusters are pathfinding-friendly. Fix broken connections, create new clusters as needed. Customize settings for isolated points, edge positions, and cluster sizes.
splash: icons/icon_graphs-sanitize.svg
preview_img: docs/splash-sanitize.png
toc_img: placeholder.jpg
tagged:
    - graphs
see_also:
    - Working with Graphs
nav_order: 8
---

{% include header_card_node %}

{% include img a='details/details-sanitize-clusters.png' %} 

| Property       | Description          |
|:-------------|:------------------|
| Prune Isolated Points           | If enabled, input points that are not part of a valid cluster *(either no edges or pruned cluster)* will be omitted from the `Vtx` output.<br>If disabled, the input points are forwarded as-is in the `Vtx` output (with added attributes).  |
| Edge Position           | If enabled, this sets the position of the `Edge` points to a lerp between their `Start` and `End` points.<br>*By default, `Edges` point are placed at the center between their two `Vtx`.*|
| Min Cluster Size           | If enabled, any cluster with less **edges** than specified will be pruned from the output.  |
| Max Cluster Size           | If enabled, any cluster with more **edges** than specified will be pruned from the output.  |

---
# Inputs & Outputs
## Vtx & Edges
See {% include lk id='Working with Graphs' %}