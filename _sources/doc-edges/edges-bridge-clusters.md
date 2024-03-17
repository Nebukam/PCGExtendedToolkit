---
layout: page
parent: Edges
#grand_parent: All Nodes
title: Bridge Clusters
subtitle: Connects clusters together.
color: blue
summary: The **Bridge Clusters** node merge clusters using methods like Delaunay for organic results, Least Edges for minimal bridges, or Most Edges for comprehensive connections. Regardless of method, bridges always connect the two nearest cluster points.
splash: icons/icon_edges-bridge.svg
preview_img: docs/splash-edges-bridge.png
toc_img: placeholder.jpg
tagged: 
    - node
    - edges
nav_order: 1
---

{% include header_card_node %}

{% include img a='details/details-bridge-clusters.png' %} 

| Property       | Description          |
|:-------------|:------------------|
| Bridge Method           | The method that will be used to identify and create bridges between clusters.|

> Note that no matter what method is selected, **a bridge will always connect the two closest points between two clusters.**  The chosen method only drives which cluster is connected to which other cluster.
{: .comment }

---
## Bridge Methods

| Method       | Description          |
|:-------------|:------------------|
|**Delaunay**||
| {% include img a='docs/bridge/method-delaunay.png' %}           | When using this method, each cluster is abstracted into a single bounding box that encapsulates all its vertices. A 3D Delaunay is generated using each bounding box center as an input, and the resulting delaunay edges are used as bridges.|
|**Least Edges**||
| {% include img a='docs/bridge/method-least.png' %}           | When using this method, the algorithm will generate the least possible amount of bridge in order to connect all the clusters together.<br>*Careful because it can easily look like a minimum spanning tree, but it's not.*|
|**Most Edges**||
| {% include img a='docs/bridge/method-most.png' %}           | When using this method, the algorithm will create a bridge from each cluster to every other cluster.|

---
# Inputs & Outputs
## Vtx & Edges
See {% include lk id='Working with Graphs' %}