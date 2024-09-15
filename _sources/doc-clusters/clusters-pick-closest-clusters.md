---
layout: page
#grand_parent: All Nodes
parent: Clusters
title: Pick Closest Cluster
name_in_editor: "Cluster : Pick Closest"
summary: The **Pick Closest Cluster** node filters clusters based on their proximity to target points, allowing selection, omission, or tagging of the closest clusters.
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

The **Pick Closest Cluster** nodes lets you select clusters within the available one based on proximity to a target point. Each target point will select a single cluster based on proxmimity.
{: .fs-5 .fw-400 } 

{% include img a='placeholder-wide.jpg' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| Search Mode          | Whether to validate closest distance for the closest `Vtx` or the closest `Edge`. |
| Pick Mode          | Defines how many clusters will get picked.<br>-`Only Best` selects only the best pick, even if multiple targets have the same pick. The first best (bestest best) pick will be accounted for for forwarding.<br>-`Next Best` guarantees each target will pick a cluster, unless there are less clusters than there are targets. |
| Action          | Defines how the selection will be processed.<br>-`Keep` keeps the selected clusters, and omits unselected ones from the output.<br>-`Omit` remove the selected clusters from the output.<br>-`Tag` tags the data based on whether they're selected or not. |
| Keep Tag          | Tag to add to the data if it's selected. |
| Omit Tag          | Tag to add to the data if it's not selected. |

---
## Target Attributes to Tags
<br>

{% include embed id='settings-forward' %}


---
## Target Forwarding
<br>

{% include embed id='settings-forward' %}
