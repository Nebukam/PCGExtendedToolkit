---
layout: page
grand_parent: Clusters
parent: Graphs
title: Delaunay 3D
name_in_editor: "Cluster : Delaunay 3D"
subtitle: Outputs a 3D Delaunay tetrahedralization.
summary: The **Delaunay 3D** node outputs a 3D Delaunay tetrahedralization with options like Urquhart graph, hull identification, and projection settings.
color: blue
splash: icons/icon_graphs-delaunay.svg
preview_img: docs/splash-delaunay-3D.png
toc_img: placeholder.jpg
tagged: 
    - node
    - clusters
    - graphs
nav_order: 3
see_also:
    - Working with Clusters
example: EdgesAndGraphs/PCGEx_Graph_Delaunay-3D
inputs:
    -   name : In
        desc : Points clouds that will be triangulated
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

This node creates a 3D Delaunay tetrahedralization from the input points. If you'd like to know more about Delaunay intrinsic properties, check out the [Wikipedia article](https://en.wikipedia.org/wiki/Delaunay_triangulation)!  
It has very *very* interesting properties, and this node also offers the ability to output the [Urquhart](https://en.wikipedia.org/wiki/Urquhart_graph) alternative; which is even more fascinating.
{: .fs-5 .fw-400 } 

{% include img a='placeholder-wide.jpg' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|: **Settings** :|
| Urquhart           | If enabled, the node will output the Urquhart version of the Delaunay graph. |
| Hull Attribute Name<br>`Boolean`          | If enabled, will flag output `Vtx` points that lie on the convex hull of the graph. |
| Mark Edge on Touch          | If enabled, edges that have at least a point on the Hull as marked as being on the hull; *as opposed to only be marked as hull edges if both endpoints are on the hull.* |

---
## Sites
<br>

{% include img a='placeholder-wide.jpg' %}

| Output Sites           | If enabled, the node will output the Delaunay Sites.<br>*Each site is the centroid of a Delaunay triangle.* |
| Site Hull Attribute Name<br>`Boolean`          | If enabled, will flag output site points that have at least an edge that lie on the convex hull of the graph. |

> Contrary to the {% include lk id='Delaunay 2D' %} node, the 3D version does not offer site-merging. Not only is it not trivial, it's all also of very little use as its main appeal is to find contours, which works poorly (if at all) on complex 3D topologies.
{: .comment }

{% include img a='placeholder-wide.jpg' %}


{% include embed id='settings-projection' %}


{% include embed id='settings-cluster-output' %}



