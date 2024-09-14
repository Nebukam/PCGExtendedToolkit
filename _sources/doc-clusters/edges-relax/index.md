---
layout: page
#grand_parent: All Nodes
parent: Clusters
title: Relax
name_in_editor: "Cluster : Relax"
subtitle: Relax points positions of a graph.
summary: The **Relax** node smooths a graph's point positions by applying iterative relaxation algorithms, allowing control over the number of iterations, type of relaxation, and influence settings for fine-tuning results.
color: blue
splash: icons/icon_edges-relax.svg
preview_img: previews/index-edges-relax.png
has_children: true
tagged: 
    - node
    - edges
see_also: 
    - Working with Clusters
    - Relaxing
nav_order: 30
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

The relax point node smoothes cluster' topology by iteratively applying a given **relaxation** algorithm.
{: .fs-5 .fw-400 } 

{% include img_link a='docs/relax/comparison.png' %} 

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Iterations | The number of time to additively apply the relaxing algorithm.<br>Each iteration uses the previous' iteration results. |
| Relaxing           | This property lets you select which kind of relaxing you want to apply to the input clusters.<br>**Specifics of the instanced module will be available under its inner Settings section.**<br>*See {% include lk id='Relaxing' %}.*  |

|**Influence**||
| Influence | Interpolate between the original position and the final, relaxed position.<br>- `1.0` means fully relaxed<br>- `0.0` means the original position is preserved.  |
| Local Influence           | If enabled, will use a per-point attribute value as *Influence*. |
| Progressive Influence           | Switchs betweeen factoring the influence after each *per-iteration* (progressive) or once all iterations have been processed.<br>*This yields vastly different results, so don't hesite to try it.* |

---
## Influence Settings
<br>
{% include embed id='settings-influence' %}

---
## Relaxing modules
<br>
{% include card_any tagged="relax" %}

---
## Cluster Output Settings
*See [Working with Clusters](/PCGExtendedToolkit/doc-general/working-with-clusters.html).*
<br>
{% include embed id='settings-cluster-output' %}

