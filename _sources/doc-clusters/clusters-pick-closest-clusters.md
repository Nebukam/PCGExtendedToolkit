---
layout: page
#grand_parent: All Nodes
parent: Clusters
title: Pick Closest Cluster
name_in_editor: "Cluster : Pick Closest"
summary: The **Pick Closest Clusters** node filters clusters based on proximity to target points, allowing filtering or tagging of the closest ones.
color: white
splash: icons/icon_sampling-guided.svg
tagged:
    - node
    - clusters
see_also:
    - Working with Clusters
nav_order: 30
inputs:
    -   name : Vtx
        desc : Endpoints of the input Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the input Vtxs
        pin : points
    -   name : In Targets
        desc : Target points used to test for proximity
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

> At the time of writing, this node has extremely poor performance, it needs refactoring; **functionality won't change so it can be used safely in your graphs.**
{: .warning }

The **Pick Closest Cluster** nodes lets you select clusters within the available one based on proximity to a target point. Each target point will select a single cluster based on proxmimity.
{: .fs-5 .fw-400 } 

{% include img a='placeholder-wide.jpg' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| Search Mode          | Whether to validate closest distance for the closest `Vtx` or the closest `Edge`. |
| Filter Actions          | What to do with the selected cluster.<br>*Detailed below* |
| Use Octree Search          | Whether or not to search for closest node using an octree. Depending on your dataset, enabling this may be either much faster, or slightly slower.<br>*It's a bit of a weird tradeoff, because even if the search is much faster, building the octree may take so much time you'd be better off without it.* |

## Action on Selection

| Property       | Description          |
|:-------------|:------------------|
| Action          | Defines how the selection will be processed.<br>-`Keep` keeps the selected clusters, and omits unselected ones from the output.<br>-`Omit` remove the selected clusters from the output.<br>-`Tag` tags the data based on whether they're selected or not. |
| Keep Tag          | Tag to add to the data if it's selected. |
| Omit Tag          | Tag to add to the data if it's not selected. |