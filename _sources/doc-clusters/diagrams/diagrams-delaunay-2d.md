---
layout: page
grand_parent: Clusters
parent: Diagrams
title: Delaunay 2D
name_in_editor: "Cluster : Delaunay 2D"
subtitle: Outputs a 2D Delaunay triangulation.
summary: The **Delaunay 2D** node outputs a 2D Delaunay triangulation with options like Urquhart graph, hull identification, and projection settings.
color: white
splash: icons/icon_graphs-delaunay.svg
tagged: 
    - node
    - clusters
    - graphs
nav_order: 1
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
    -   name : Sites
        desc : Delaunay Sites
        pin : points
---


{% include header_card_node %}

This node creates a 2D Delaunay graph from the input points. If you'd like to know more about Delaunay intrinsic properties, check out the [Wikipedia article](https://en.wikipedia.org/wiki/Delaunay_triangulation)!  
It has very *very* interesting properties, and this node also offers the ability to output the [Urquhart](https://en.wikipedia.org/wiki/Urquhart_graph) alternative; which is even more fascinating.
{: .fs-5 .fw-400 } 

{% include img a='details/diagrams/diagrams-delaunay-2d-lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|: **Settings** :|
| Urquhart           | If enabled, the node will output the Urquhart version of the Delaunay graph.<br>*This enables additional options for site output.* |
| <span class="eout">Hull Attribute Name</span><br>`Boolean`          | If enabled, will flag output `Vtx` points that lie on the convex hull of the graph. |
| Mark Edge on Touch          | If enabled, edges that have at least a point on the Hull as marked as being on the hull; *as opposed to only be marked as hull edges if both endpoints are on the hull.* |

---
## Sites
<br>

{% include img_link a='details/diagrams/diagrams-delaunay-2d-sites.png' %}

| Output Sites           | If enabled, the node will output the Delaunay Sites.<br>*Each site is the centroid of a Delaunay triangle.* |
| <span class="eout">Site Hull Attribute Name</span><br>`Boolean`          | If enabled, will flag output site points that have at least an edge that lie on the convex hull of the graph. |
| Urquhart Sites Merge         | Defines if and how Delaunay Sites should be merged when they are part of a single "Urquhart" cell. |

### Urquhart Sites Merge

| Mode       | |
|:-------------|:------------------|
| {% include img a='details/diagrams/enum-sitemerge-none.png' %} | **None**<br>Outputs canon Delaunay sites. |
| {% include img a='details/diagrams/enum-sitemerge-sites.png' %} | **Merge Sites**<br>The output site is the average of all the canon Delaunay sites that are abstracted by the Urquhart pass. |
| {% include img a='details/diagrams/enum-sitemerge-edges.png' %} | **Merge Edges**<br>The output site is the average of all the edges removed by the Urquhart pass. |

> Which merge mode to use, if any, depends on your final intent. **If your goal is find the contours of an Urquhart "cell", then Merge Edges offer usually more interesting sites positions.**
{: .infos-hl }


---
## Projection Settings
<br>
{% include embed id='settings-projection' %}


---
## Cluster Output Settings
*See [Working with Clusters - Cluster Output Settings](/PCGExtendedToolkit/doc-general/working-with-clusters.html#cluster-output-settings).*


---
## Tips

The delaunay algorithm creates a convex hull of very, *very* long edges, so you'll often need to remove them to "clean" the graph. There are multiple approaches to to that, the two most straighfoward approaches being:

### Hull pruning
- Simply enable the Hull mark, filter these `Vtx` out and sanitize the cluster afterward.

{% include img_link a='details/diagrams/diagrams-delaunay-2d-hull-pruning.png' %}

### Prune by Length
- Use {% include lk id='Prune edges by Length' %} to remove outliers. This is a more conversative approach compared to the one above, as it will keep the smaller hull edges.

{% include img_link a='details/diagrams/diagrams-delaunay-2d-hull-pruning2.png' %}

