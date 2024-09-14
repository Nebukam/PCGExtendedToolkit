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

# Properties
<br>

---
## Cluster Output Settings
*See [Working with Clusters](/PCGExtendedToolkit/doc-general/working-with-clusters.html).*
<br>
{% include embed id='settings-cluster-output' %}