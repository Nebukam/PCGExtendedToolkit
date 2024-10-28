---
layout: page
family: Edge
#grand_parent: All Nodes
parent: Clusters
title: Refine
name_in_editor: "Cluster : Refine"
subtitle: Algorithmic edge refinement
summary: The **Refine** node algorithmically prunes edges in a graph to enforce specific properties, allowing selection of refinement type and optional sanitization to restore edges based on predefined conditions.
color: white
splash: icons/icon_edges-refine.svg
preview_img: previews/index-refine.png
has_children: true
#use_child_thumbnails: true
tagged:
    - node
    - clusters
see_also: 
    - Working with Clusters
    - Refining
    - Filter Ecosystem
nav_order: 10
inputs:
    -   name : Vtx
        desc : Endpoints of the input Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the input Vtxs
        pin : points
    -   name : Heuristics
        desc : Heuristic nodes, if required by the selected refinement.
        pin : params
    -   name : Edge Filters
        desc : Point filter input used by some refinements
        pin : params
    -   name : Sanitization Filters
        desc : Point filter input used to preserve edges as a sanitization step
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

The **Cluster Refine** node lets you refine connections inside individual clusters, a.k.a Edge pruning.  
{: .fs-5 .fw-400 } 

> Refining only **removes** edges and does not create new ones.

{% include img a='details/edges-refine/edges-refine-lead.png' %}

> The filters determine which edge can be pruned. **Any filter that doesn't pass will ensure the edge is preserved.**
{: .infos-hl }

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Refinement           | This property lets you select which kind of refinement you want to apply to the input clusters.<br>**Specifics of the instanced module will be available under its inner Settings section, if any.**  |
| Output Only Edges As Points | If enabled, this node will output edges as raw points, without the usually associated cluster.<br>*This can be significantly faster and less greedy depending on your need for the output edges.* |

---
## Sanitization
The sanitization property lets you enforce some general conditions within the graph. Note that is applied after the refinement.  
*Note that this is not mutually exclusive with filters: sanitization happens has a post-process after the "raw" refinement is completed.*

| Sanitization       | Description          |
|:-------------|:------------------|
| None           | No sanitization.  |
| Shortest           | If a node has no edge left, restore the shortest one.|
| Longest           | If a node has no edge left, restore the longest one.|
| Filters           | Use per-point filters to ensure edge preservation.|

> Note that the **sanitization options offer no guarantee that the initial interconnectivity will be preserved!** *-- It's more of a "whatever happens, keep those edges" pass.*
{: .warning }

---
## Available Refining modules
<br>
{% include card_any tagged="edgerefining" %}

---
## Cluster Output Settings
*See [Working with Clusters - Cluster Output Settings](/PCGExtendedToolkit/doc-general/working-with-clusters.html#cluster-output-settings).*



---
# Filters
<br>
This node supports a wide variety of filters, but it's important to keep in mind that **only the edge points are processed by the filters**.
{: .fs-5 .fw-400 } 

Both regular point filter & edge filters are supported, node filter are not *(because they operate on endpoints specifically, which this node doesn't work on)*.

> Regular & Sanitization filtering operate on input data, not the modified output. That means if you use filters that rely on intrinsinc properties such as neighbor count, it will be the neighbor count of the input, not the output!
{: .infos }

---
## Available Filters
<br>
{% include card_any tagged="pointfilter" %}

---
## Edge-specific filters
<br>
{% include card_any tagged='edgefilter' %}