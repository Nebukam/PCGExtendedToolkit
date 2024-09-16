---
layout: page
#grand_parent: All Nodes
parent: Clusters
title: Connect Clusters
name_in_editor: "Cluster : Connect"
subtitle: Connects clusters together.
summary: The **Connect Clusters** node creates bridge edges between clusters using methods like Delaunay, Least Edges, or Most Edges, always connecting the closest points between clusters to form a larger, unified cluster.
color: white
splash: icons/icon_edges-bridge.svg
tagged: 
    - node
    - edges
nav_order: 100
see_also: 
    - Working with Clusters
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

The **Connect Clusters** creates "bridge" edges between all input clusters and outputs a single bigger one as a result.
{: .fs-5 .fw-400 } 

{% include img a='details/clusters-connect/lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| Bridge Method           | The method that will be used to identify and create bridges between clusters.|

> Note that no matter what method is selected, **a bridge will always connect the two closest points between two clusters.**  The chosen method only drives which cluster is connected to which other cluster.
{: .comment }

### Bridge Methods

| Method       | Description          |
|:-------------|:------------------|
| {% include img a='docs/bridge/method-delaunay.png' %}           | **Delaunay**<br>When using this method, each cluster is abstracted into a single bounding box that encapsulates all its vertices. A 3D Delaunay is generated using each bounding box center as an input, and the resulting delaunay edges are used as bridges.|
| {% include img a='docs/bridge/method-least.png' %}           | **Least Edges**<br>When using this method, the algorithm will generate the least possible amount of bridge in order to connect all the clusters together.<br>*Careful because it can easily look like a minimum spanning tree, but it's not.*|
| {% include img a='docs/bridge/method-most.png' %}           | **Most Edges**<br>When using this method, the algorithm will create a bridge from each cluster to every other cluster.|


---
## Projection Settings
<br>
{% include embed id='settings-projection' %}


---
## Carry Over Settings
<br>
{% include embed id='settings-carry-over' %}


---
## Cluster Output Settings
*See [Working with Clusters](/PCGExtendedToolkit/doc-general/working-with-clusters.html).*
<br>
{% include embed id='settings-cluster-output' %}