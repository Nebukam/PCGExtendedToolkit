---
layout: page
family: Cluster
#grand_parent: All Nodes
parent: Clusters
title: Prune edges by Length
#name_in_editor: "Cluster : Prune edges by Length"
subtitle: DEPRECATED
summary: DEPRECATED
color: white
splash: icons/icon_edges-prune-by-length.svg
tagged:
    - node
    - clusters
see_also:
    - Working with Clusters
nav_order: 1000
---

{% include header_card_node %}

This node has been deprecated because other, more robust systems allow to reproduce its exact behavior, with the added benefit of outputing tons of useful data in the same processing time.  
**Edge properties are often used somewhere at least once if there's a need to prune them by length; and Refining by Filter allow additional filtering criterias in a single run as well.**
{: .fs-5 .fw-400 } 

> **This node has been DEPRECATED. Instead, you can reproduce its behavior with the combination of nodes below.**  
> It will continue to exist where it used to, but won't be maintained and my cause crashes in the future.
{: .error }

<br>

- Use {% include lk id='Properties : Edge' %} to ouput **Edge Length** as an attribute.
- Use {% include lk id='🝔 Remove by Filter' %} on a {% include lk id='Refine' %} node, using
- {% include lk id='🝖 Mean Value' %} filter based on the **Edge Length** attribute written earlier.

{% include img a='docs/prune-edges-by-length-replacement.png' %}

<br>

You can find the above graph in the plugin' content folder : `/PCGExtendedToolkit/Subgraphs/PCGEx_PruneEdgeByLength_Replacement`

<br>

---
## Easy pruning
If you just want to exclude edge above or below a certain fixed length -- i.e `any edge longer than 500` or `any edge shorter than 42`, just do the following:

### Exclude any edge longer than X
<br>
{% include img a='docs/prune-edges/above-500.png' %} 
<br>

Measure: `Absolute`, Mean Method: `Fixed`, Mean Value: `500`, Exclude Above : `0`.
*This basically Exclude all edges which length is above `500 + 0`.*

### Exclude any edge shorter than X
<br>
{% include img a='docs/prune-edges/below-42.png' %}
<br>

Measure: `Absolute`, Mean Method: `Fixed`, Mean Value: `42`, Exclude Below : `0`.
*This basically Exclude all edges which length is below `42 - 0`.*
