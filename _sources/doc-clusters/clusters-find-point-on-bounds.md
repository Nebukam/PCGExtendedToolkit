---
layout: page
family: ClusterGen
#grand_parent: All Nodes
parent: Clusters
title: Find point on Bounds
name_in_editor: "Cluster : Find point on Bounds"
subtitle: Find a point in each cluster that is the closest to a bound-relative position.
summary: The **Find point on Bounds** node find the point inside either edge or vtx points that is closest to that cluster' bounds. This is especially useful to use as a seed to find outer contours of individual clusters.
color: white
splash: icons/icon_edges-bridge.svg
tagged: 
    - node
    - clusters
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
    -   name : Out
        desc : Points found. One dataset per input cluster.
        pin : points
---

{% include header_card_node %}

The **Find point on Bounds** node for cluster extract the single closest `Vtx` point to a target position that is relative to the cluster' AABB.  
**In other words, it's a very reliable way to find a point that can be used as a seed for the {% include lk id='Find Contours' %} node and grab the perimeter of a cluster.**
{: .fs-5 .fw-400 } 

{% include img a='details/clusters-find-point-on-bounds/lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| Search Mode          | Whether to validate closest distance for the closest `Vtx` or the closest `Edge`. |
| Output Mode          | Whether to output a single dataset per cluster, or a single consolidated list. |
| UVW          | The bound-relative position to start searching from. `1,1,1` will be top left corner of the bounding box; while `-1,-1,-1` will be located in the opposite corner. |
| Offset          | Offset the output point by a distance, **away** from the center of the bounding box. |
| Carry Over Settings          | *See below.* |
| Quiet Attribute Mismatch Warning          | When using `Merged` output, some attribute may share the same name but not the same type, which will throw a warning. You can disable the warning by enabling this option. |

---
## Carry Over Settings
<br>
{% include embed id='settings-carry-over' %}

---
## Cluster Output Settings
*See [Working with Clusters - Cluster Output Settings](/PCGExtendedToolkit/doc-general/working-with-clusters.html#cluster-output-settings).*