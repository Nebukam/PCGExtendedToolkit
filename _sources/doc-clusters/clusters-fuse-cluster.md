---
layout: page
family: Cluster
#grand_parent: All Nodes
parent: Clusters
title: Fuse Clusters
name_in_editor: "Cluster : Fuse"
subtitle: Fuse clusters together by finding Point/Edge and Edge/Edge intersections.
summary: The Fuse Clusters node merges clusters by combining collocated vertices and edges, while detecting point/edge and edge/edge intersections, creating larger and more complex clusters through a multi-stage blending process.
color: white
splash: icons/icon_edges-intersections.svg
tagged:
    - node
    - clusters
see_also:
    - Working with Clusters
nav_order: 30
has_children: false
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

The **Fuse Cluster** merges & fuse all input cluster together: collocated `Vtx` and their connected `Edges` are merged to create bigger islands. Additionally, the node can find point/edge and edge/edge intersections, creating even more complex clusters and supporting some advanced scenarios.
{: .fs-5 .fw-400 } 

{% include img a='details/clusters-fuse-cluster/lead.png' %}

> Be careful with this node, it can quickly become very expensive!
{: .warning }

---
# Properties
<br>

The Fuse node has a very specific order of operation:
1. First collocated `Vtx` are merged together (*Point/Point*), and any duplicate `Edge` is removed.
    - Points that are merged go through a first blending pass.
1. Next, if enabled, any `Vtx` that lie on a foreign `Edge` will split that edge into two.
    - ~~A second blending pass is applied~~
1. Last, `Edges` are tested against each other, possibly creating new `Vtx` & `Edges` in the process.  
    - A third and last blending pass is applied, where each edges' endpoints are blended into their resulting intersection point. 

> While counter intuitive, it is much cheaper to leave `self-intersection` enabled.
{: .infos }

---
## Point/Point Intersections
<br>

{% include img a='details/clusters-fuse-cluster/point-point.png' %}


{% include embed id='settings-intersect-point-point' %}


{% include img a='details/clusters-fuse-cluster/point-point-voxel-vs-octree.png' %}

> `Voxel` is faster but will snap vtx on a grid; `Octree` is slower but is much more conservative of the original aspect.
{: .infos }

---
## Point/Edge Intersections
<br>

Point/Edge intersections search for `Vtx` that are lying on `Edges` they're **not connected to**, and if such situation is found, the matching edge is split into new edges as to *insert* the `Vtx` there.  
The original `Edge` is removed and the `Vtx` then becomes connected to the old `Edge`' endpoints through 2 new `Edges`.
{: .fs-5 .fw-400 } 

| Find Point Edge Intersections | If enabled, fusing will search for point/edge intersections.  |

{% include img a='details/clusters-fuse-cluster/point-edge.png' %}

{% include embed id='settings-intersect-point-edge' %}


---
## Edge/Edge Intersections
<br>

Edge/Edge intersections search for `Edges` that intersect with others `Edges`. When found, **a new `Vtx` is created at the intersection**, creating 4 new edges in place of the previous 2.
{: .fs-5 .fw-400 } 

| Find Edge Edge Intersections | If enabled, fusing will search for edge/edge intersections.  |

{% include img a='details/clusters-fuse-cluster/edge-edge.png' %}

{% include embed id='settings-intersect-edge-edge' %}


---
## Blending
<br>
See {% include lk id='Blending' %} for properties details.

- `Default Points Blending` is used for Point/Point & Point/Edge
- `Default Edges Blending` is used for Point/Edge and Edge/Edge
- You may use the associated checkbox to override blending settings to get more control over the different processing stages.

---
## Meta Filters
<br>
Meta filters lets you choose separately which attributes & tags should carry over from their original data to the new output.

{% include embed id='settings-carry-over' %}

---
## Cluster Output Settings
*See [Working with Clusters - Cluster Output Settings](/PCGExtendedToolkit/doc-general/working-with-clusters.html#cluster-output-settings).*