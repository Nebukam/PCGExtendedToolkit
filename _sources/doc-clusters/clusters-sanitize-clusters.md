---
layout: page
#grand_parent: All Nodes
parent: Clusters
title: Sanitize Clusters
name_in_editor: "Cluster : Sanitize"
subtitle: Ensure clusters are clean and complete
summary: The **Sanitize Clusters** ensures clusters are pathfinding-friendly. Fix broken connections, create new clusters as needed. Customize settings for isolated points, edge positions, and cluster sizes.
color: red
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
outputs:
    -   name : Vtx
        desc : Endpoints of the output Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the output Vtxs
        pin : points
---

{% include header_card_node %}

The **Sanitize Cluster** is an imperative post-processing node when you "manually" remove `Vtx` or `Edge` from datasets. It ensure that the cluster data is properly usable inside PCGEx' ecosystem of cluster-related nodes.
{: .fs-5 .fw-400 } 

As mentionned in the {% include lk id='Working with Clusters' %} page, `Vtx` and `Edge` points are storing critical attributes & informations about the topology of a cluster. **Sanitize** removes `Edges` which `Vtx` have been removed, as well as prune `Vtx` that aren't participating to any `Edge`.  
Additionally, if a cluster that was previously a bit interconnected topology has been split into smaller groups, this will be reflected in the `Edges` output with additional point datasets.

> Note that all cluster nodes output sanitized clusters by design, so there is no need to use this node after "official" PCGEx operations.
> **Rule of thumb is, if a node has a 'Cluster Output Settings' in its detail panel, it's generating clean clusters.**
{: .infos-hl }

{% include img a='details/clusters-sanitize-clusters/lead.png' %}

---
## Cluster Output Settings
*See [Working with Clusters](/PCGExtendedToolkit/doc-general/working-with-clusters.html).*
<br>
{% include embed id='settings-cluster-output' %}
