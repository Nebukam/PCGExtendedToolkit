---
layout: page
family: ClusterGen
#grand_parent: All Nodes
parent: Clusters
title: Connect Points
name_in_editor: Connect Points
subtitle: Creates connections between points using various probes.
summary: The **Connect Points** node creates connections between points in clusters based on user-defined probes, allowing control over how points generate and receive connections, with options for preventing overlap and projecting points for more accurate results.
color: white
splash: icons/icon_path-to-edges.svg
preview_img: previews/index-connect-points.png
has_children: true
#use_child_thumbnails: true
tagged: 
    - clusters
    - node
nav_order: 0
see_also:
    - Working with Clusters
    - Filter Ecosystem
inputs:
    -   name : Points
        desc : Points that will be connected togethers
        pin : points
    -   name : Probes
        desc : Probes used to build connections
        pin : params
    -   name : Generators
        desc : Filters used to determine which points are allowed to generate connections
        pin : params
    -   name : Connectables
        desc : Filters used to determine which points are allowed to receive connections
        pin : params
outputs:
    -   name : Vtx
        desc : Endpoints of the output Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the output Vtxs
        pin : points
---

{% include header_card_node %}

The connect point node allows you to create connected clusters using different **probes**. Each point will go through each probe' rules to find neighboring points to connect to.
{: .fs-5 .fw-400 } 

{% include img a='details/connect-points/lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| Coincidence Prevention Tolerance           | When enabled, the node will prevent multiple connections from happening in the same direction, within that tolerance.<br>*This avoids the creation of overlapping edges when testing in near-collinear situations.* |
| Project Points           | If enabled, points will be projected on a plane under-the-hood before looking for connections.<br>*This is especially desirable when working with landscapes* |
| Projection normal           | The normal of the plane to project points to, if enabled.<br>*Unless you're doing very custom stuff, the default value is usually fine as it project points as if "seen from top view"* |

---
## Generators & Connectables
These two inputs accept any of filters available as part of the {% include lk id='Filter Ecosystem' %}.  
- **Generators** are points that will use the probes to find neighbors they can connect to.
- **Connectables** are points that will be seen by the probes.
All points

---
## Available Probes

Probes are at the core of the **Connect Points** node.  
You can connect as many probes as you'd like to the `Probes` input.
{: .fs-5 .fw-400 } 

> No matter how many probes you use, this node will NEVER generate duplicate edges; so feel free to experiment.
{: .infos-hl }
<br>

{% include card_childs tagged='probe' %}

---

## Available Filters
*You can use the regular filters for Generators & Connectables :*  

{% include card_any tagged="filter" %}

---
## Cluster Output Settings
*See [Working with Clusters - Cluster Output Settings](/PCGExtendedToolkit/doc-general/working-with-clusters.html#cluster-output-settings).*