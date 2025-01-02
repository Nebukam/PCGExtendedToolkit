---
layout: page
family: Path
#grand_parent: All Nodes
parent: Paths
title: Paths to Clusters
name_in_editor: "Path : To Clusters"
subtitle: Convert paths to clusters.
color: white
summary: The **Paths to Clusters** node transforms input paths into interconnected edge clusters by fusing points, while maintaining edge relationships, offering options similar to the Fuse Clusters node but using paths as input.
splash: icons/icon_path-to-edges.svg
tagged: 
    - node
    - paths
see_also:
    - Working with Clusters
nav_order: 6
inputs:
    -   name : Paths
        desc : Paths which points will be checked for collinearity
        pin : points
outputs:
    -   name : Vtx
        extra_icon: IN_Vtx
        desc : Endpoints of the output Edges
        pin : points
    -   name : Edges
        extra_icon: OUT_Edges
        desc : Edges associated with the output Vtxs
        pin : points
---


{% include header_card_node %}

The **Paths to cluster** node converts input paths into interconnected clusters, turning points to `Vtx` and path segments to `Edges`. It offers the same advanced options {% include lk id='Fuse Clusters' %} does, but takes paths as input!
{: .fs-5 .fw-400 } 

{% include img a='details/paths-to-clusters/lead.png' %}

# Properties
<br>

{% include embed id='settings-closed-loop' %}
|Fuse Paths| If enabled, emulates the behavior of {% include lk id='Fuse Clusters' %}. Otherwise, convert each path into its own "linear" cluster. |

The Path to Clusters node has a very specific order of operation:
1. First collocated points are merged together (*Point/Point*), turned into `Vtx`, and `Edges` are created from what was previously path segments.
    - Points that are merged go through a first blending pass.
1. Next, if enabled, any `Vtx` that lie on a foreign `Edge` will split that edge into two.
    - ~~A second blending pass is applied~~
1. Last, `Edges` are tested against each other, possibly creating new `Vtx` & `Edges` in the process.  
    - A third and last blending pass is applied, where each edges' endpoints are blended into their resulting intersection point. 

> While counter intuitive, it is much cheaper to leave `self-intersection` enabled.
{: .infos }

---
## Point/Point Settings
<br>

{% include img a='details/clusters-fuse-cluster/point-point.png' %}


{% include embed id='settings-intersect-point-point' %}


{% include img a='details/clusters-fuse-cluster/point-point-voxel-vs-octree.png' %}

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