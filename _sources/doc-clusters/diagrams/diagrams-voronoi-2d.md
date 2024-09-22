---
layout: page
family: ClusterGen
grand_parent: Clusters
parent: Diagrams
title: Voronoi 2D
name_in_editor: "Cluster : Voronoi 2D"
subtitle: Outputs a 2D Voronoi graph.
summary: The **Voronoi 2D** node outputs a 2D Voronoi graph with options like balanced, canon, or centroid positioning. Adjust bounds, prune sites, and mark edges on the hull. 
color: white
splash: icons/icon_graphs-voronoi.svg
tagged:
    - node
    - clusters
    - graphs
nav_order: 2
see_also:
    - Working with Clusters
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

This node creates a 2D Voronoi diagram from the input points. If you'd like to know more about Voronoi intrinsic properties, check out the [Wikipedia article](https://en.wikipedia.org/wiki/Voronoi_diagram)!  
{: .fs-5 .fw-400 } 

{% include img a='details/diagrams/diagrams-voronoi-2d-lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|: **Settings** :|
| Method           | Defines how the position of the Voronoi site is computed. *See below for more infos.* |
| Expand Bounds           | Value added on each axis of the initial input points bounds, used for maths & processes involving bounds. |
| Prune Out of Bounds           | Depending on the selected method, the diagram will produce out-of-bounds points *(up to ±inf)*. Enabling this option lets you remove those points from the output. |
| <span class="eout">Hull Attribute Name</span><br>`Boolean`          | If enabled, will flag output `Vtx` points that lie on the convex hull of the underlying Delaunay diagram.<br>*Note that this is not the exact hull, but rather an approximation.* |
| Mark Edge on Touch          | If enabled, edges that have at least a point on the Hull as marked as being on the hull; *as opposed to only be marked as hull edges if both endpoints are on the hull.* |

> Note that enabling `Prune Out of Bounds` points has a theorical risk of creating more than one finite cluster as a result.
{: .warning }

---
## Voronoi site position
<br>

{% include img a='details/diagrams/diagrams-voronoi-3d-sites.png' %}

| Mode       | |
|:-------------|:------------------|
| {% include img a='placeholder.jpg' %} | **Centroid**<br>Uses the centroid of the Delaunay site.<br>*While more visually pleasing, some concave sites may appear depending on the initial topology.* |
| {% include img a='placeholder.jpg' %} | **Canon (Circumcenter)**<br>Uses the [circumcenter](https://en.wikipedia.org/wiki/Circumcircle) of the Delaunay triangle.<br>*This is the true voronoi algorithm, it guarantees only convex sites.* |
| {% include img a='placeholder.jpg' %} | **Centroid**<br>Uses the centroid of the Delaunay site for the point that are outside the bounds, otherwise use circumcenters.<br>*Best of both worlds, or worst of both worlds; depending on how you look at it.* |


---
## Projection Settings
<br>
{% include embed id='settings-projection' %}


---
## Cluster Output Settings
*See [Working with Clusters - Cluster Output Settings](/PCGExtendedToolkit/doc-general/working-with-clusters.html#cluster-output-settings).*