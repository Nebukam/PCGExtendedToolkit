---
layout: page
#grand_parent: All Nodes
parent: Clusters
title: Mesh to Clusters
name_in_editor: "Mesh to Clusters"
subtitle: Convert mesh/geometry topology to usable clusters.
summary: The **Mesh to Cluster** node transforms raw mesh geometry into clusters by extracting vertices and edges, which are then copied to input points. This is useful for leveraging mesh topology within a cluster-based workflow.
color: white
splash: icons/icon_graphs-delaunay.svg
tagged:
    - node
    - clusters
see_also:
    - Working with Clusters
nav_order: 2
inputs:
    -   name : In Targets
        desc : Target points to copy mesh cluster to.
        pin : point
outputs:
    -   name : Vtx
        desc : Endpoints of the output Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the output Vtxs
        pin : points
---

{% include header_card_node %}

The **Mesh to Cluster** node converts raw geometry from meshes (either specific reference or by sampling the scene) into Clusters, using geometry' vertices and triangles to build `Vtx` and `Edges` and copy them onto the input points, much like {% include lk id='Copy Cluster to Points' %} does.
{: .fs-5 .fw-400 } 

{% include img a='details/clusters-mesh-to-clusters/lead.png' %}

> Note that the current implementation is very prototype-y : when using an actor reference, only the first primitive will be processed.
{: .warning }

---
# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| Graph Output Type          | Defines how the raw geometry will be converted to `Vtx` & `Edges` |
| Static Mesh Source          | Select the source for the mesh(es) to be converted.<br>- `Constant` is a single StaticMesh picker that will be used for each of the input points.<br>- `Attribute` on the other end lets you pick an attribute on the input points that contains an object path to either a StaticMesh or an Actor reference. |
| Static Mesh Constant          | The static mesh to convert to a graph & copy to input points. |
| Static Mesh Attribute          | The name of the attribute that contains a path to a StaticMesh or Actor.<br>*`FSoftObjectPath` is preferred, but supports `Fname`, `FString`* |
| Attribute Handling         | Lets you pick how the soft path should be handled, either as StaticMesh or an ActorReference.<br>*Note that this is legacy is will be removed in an upcoming update to support both. Meanwhile, you still need to choose one.* |

| **Transform Details**  | |
| Inherit Scale          | If enabled, cluster points will be scaled by the target' scale. |
| Inherit Rotation          | If enabled, cluster points will be scaled by the target' rotation. |

| **Misc**  | |
| Ignore Mesh Warning          | If enabled, skips over invalid meshes path without warning. |
| Cluster Output Settings          | *See below.* |
| Attributes Forwarding          | *See below.* |


---
## Graph Output Type

|: Output Type    | Description |
|:-------------|:------------------|
| {% include img a='details/clusters-mesh-to-clusters/enum-raw.png' %}           | <span class="ebit">Raw</span><br>Uses the raw, unedited mesh topology. |
| {% include img a='details/clusters-mesh-to-clusters/enum-dual.png' %}           | <span class="ebit">Dual</span><br>Uses the dual graph of the mesh topology. |
| {% include img a='details/clusters-mesh-to-clusters/enum-hollow.png' %}           | <span class="ebit">Hollow</span><br>Uses a special combination of both raw and dual graph to create more edges. |
{: .enum }

---
## Cluster Output Settings
*See [Working with Clusters - Cluster Output Settings](/PCGExtendedToolkit/doc-general/working-with-clusters.html#cluster-output-settings).*

---
## Attribute Forwarding
<br>

> Note that at the time of writing, attribute forwarding is not implemented
{: .warning }

{% include embed id='settings-forward' %}