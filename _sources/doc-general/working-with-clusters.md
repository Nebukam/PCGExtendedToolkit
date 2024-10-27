---
layout: page
parent: ∷ General
title: Working with Clusters
subtitle: Vtx? Edges? 
summary: A summary of what "graph" means and entails in the context of PCGEx.
splash: icons/icon_edges-refine.svg
nav_order: 3
tagged:
    - basics
#has_children: true
---

{% include header_card %}

One of the main focus of PCGEx is working with Clusters : vtx & edge-based "graph" structures. They're akin to a mesh without faces: a bunch of vertices connected by a bunch of edges.
{: .fs-5 .fw-400 } 

{% include imgc a='docs/vtx-edge-cluster.png' %}  

PCGEx leverage PCG' point data as data holders in order to enable easy tweaking and manipulation of edge/vertice data using existing vanilla nodes. In short, **a graph is represented by at least two separate set of points: the first, `Vtx` represent individual vertices; others, `Edges`, represent interconnected clusters.**  
`Edges` points stores the indices of their `start` and `end` point in the matching `Vtx` group.
{: .fs-5 .fw-400 } 

>Because of that approach, nodes that work with graph require `Vtx` and `Edges` as separate inputs. Data Tags are used to mark which edges match which vertices, so you will see additional tags appear on your data, formatted as `PCGEx/XXX`.

>**Rule of thumb** : If you manually alter *(as in add or remove points)* the content of a `Vtx` or `Edges` created by a PCGEx Node, make sure to use the {% include lk id='Sanitize Clusters' %} node before using them as inputs for other PCGEx nodes.
{: .infos-hl }

#### Checklist
{: .no_toc }
- TOC
{:toc}

---
## Packed data
The `PCGEx/VtxEndpoint` attribute first 32 bits are that `Vtx` unique ID from the cluster perspective, while the last 32 bits contains the number of `Edges` this `Vtx` is connected to. It makes up the identity of the `Vtx`. Alternatively, `Edges` have a `PCGEx/EdgeEndpoints` attribute whose first 32 bits matches their start' `Vtx` unique ID, and the last 32 bits are the edge' end `Vtx` unique ID.  

Put together, this data is used to rebuild clusters in memory. If you manually alter (*read : using native nodes*) the number of points in either `Vtx` or `Edges` dataset, make sure to use {% include lk id='Sanitize Clusters' %} to restore the coherence of these attributes.

>**Rule of thumb** : The only information that matter on `Edges` for clusters is their `start` and `end` attribute. Their position in space is ignored so feel free to use those points if they can be relevant to you. 
{: .infos-hl }


---
## Clusters

In PCGEx terms, a cluster is "a set of `Vtx` and `Edges` that are interconnected in a finite way" -- in other words, **there is a guaranteed way inside a cluster that a path can be found between any `Vtx` to any other `Vtx` when passing through `Edges`.**

A graph usually has a single set of vertices, but will output as many edge datasets as there are *clusters*. These will be then rebuilt inside the nodes to be processed.

{% include imgc a='docs/vtx-edge-cluster-tag.png' %}  

>If you find yourself with unexpected disconnections that ruin your pathfinding, check out {% include lk id='Connect Clusters' %} or {% include lk id='Fuse Clusters' %}.
{: .infos }

---
## Cluster Output Settings
<br>
This group of settings is present on all PCGEx nodes that output cluster data. They're used to prune undesired ouputs and include a few very specific optimization options.

{% include embed id='settings-cluster-output' %}

---
# Important Notes

>Altering `PCGEx/VtxEndpoints`, `PCGEx/ClusterId` voids the guarantee that PCGEx nodes will work as expected.
{: .warning }

>The `UID` used for the tagging is **NOT** deterministic, and will change *at each execution* of the graph, and for *each node*. It is used under the hood only, and should not be edited, nor relied upon for any kind of tag-related operations. For the same reason, if you create custom tags starting with `PCGEx/`, it will break existing vtx/edge associations.  
>**Because of the reliance on the tagging system, if you edit `Vtx` and `Edges` before using them with a PCGEx node, make sure tags are preserved.**  

*I'm working on more robust ways to associate the data other than tags, but sadly tagging is the most bulletproof and reliable method I found. Underlying data types are always at the risk of not being preserved if they're not properly duplicated, which can happen in earlier/experimental version of PCG that this plugin is supporting.*
