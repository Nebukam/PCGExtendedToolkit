---
layout: page
family: Cluster
#grand_parent: All Nodes
parent: Clusters
title: Cut Clusters
name_in_editor: "Cluster : Cut"
subtitle: Cut clusters using paths
summary: The **Cut Cluster** node ...
color: white
splash: icons/icon_graphs-sanitize.svg
tagged:
    - node
    - clusters
see_also:
    - Working with Clusters
nav_order: 20
inputs:
    -   name : Vtx
        desc : Endpoints of the input Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the input Vtxs
        pin : points
    -   name : In Targets
        desc : Target points to copy input clusters to.
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

The **Cut Cluster** node is a path/cluster interop node. It uses input paths to "select" clusters parts they intersect with, and either keep or remove these parts. It's especially useful to split clusters into multiple partitions using paths, but has many other uses.  
It can operate on clusters edges only, nodes only, or both.
{: .fs-5 .fw-400 } 

{% include img a='details/clusters-copy-to-points/lead.png' %}

# Properties
<br>

{% include embed id='settings-closed-loop' %}

| Property       | Description          |
|:-------------|:------------------|
| **Settings**  | |
| Invert          | Inverts the cutting behavior. Edges & nodes are kept in the vicinity of the path, instead of being removed. |
| Mode          | Which cluster components the node operates on. |
| Node Expansion          | Amount by which node bounds should be expanded for proximity testing.<br>*Uses node point' bounds as a baseline.* |
| Affected Nodes Affect Connected Edges          | If a node is kept or removed, so are its connected edges. 
| Affected Edges Affect Endpoints          | If an edge is kept or removed, so are its start and end points.<br>*This make the whichever mode is selected more "aggressive".* |
| Keep Edges that connect valid nodes          | When `Invert` is enabled, this keep edges that exist between two valid nodes.<br>*This make the overall pruning more conservative of edges.* |

### Mode

|: Handling     ||
|:-------------|:------------------|
| <span class="ebit">Nodes</span>           | Nodes are tested for **proximity** against the input paths. |
| <span class="ebit">Edges</span>           | Edges are tested for **intersection** with the input paths. |
| <span class="ebit">Edges & Nodes</span>           | Combines the two operations above. |
{: .enum }


---
## Cluster Output Settings
*See [Working with Clusters - Cluster Output Settings](/PCGExtendedToolkit/doc-general/working-with-clusters.html#cluster-output-settings).*

