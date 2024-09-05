---
layout: page
parent: ∷ General
title: Working with Clusters
subtitle: Vtx? Edges? 
summary: A summary of what "graph" means and entails in the context of PCGEx.
splash: icons/icon_edges-refine.svg
preview_img: docs/splash-working-with-graph.png
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

>Because of that approach, nodes that work with graph require `Vtx` and `Edges` as separate inputs. Data Tags are used to mark which edges match which vertices, so you will see additional tags appear on your data, formatted as `PCGEx/Cluster::UID`.

>**Rule of thumb** : If you manually alter *(as in add or remove points)* the content of a `Vtx` or `Edges` created by a PCGEx Node, make sure to use the {% include lk id='Sanitize Clusters' %} node before using them as inputs for other PCGEx nodes.
{: .infos-hl }

#### Checklist
{: .no_toc }
- TOC
{:toc}

---
## Vtx

A `Vtx` has one important piece of data written on it, and their position in space is relied upon for edge metrics -- `Vtx` being edges' start and end points.
### Cached Index
The `PCGEx/CachedIndex` attribute hold the index of the `Vtx` when it was written into a graph. This is the index `PCGEx/EdgeStart` and `PCGEx/EdgeEnd` refers to on the `Edges` points.  
It is primarily useful to know whether the vtx structure has been altered or not, and ensure the graph is safe to use to avoid exceptions.

### Cached Edge Num
The `PCGEx/CachedEdgeNum` attribute hold the number of edges connected to that `Vtx` when it was written into a graph. It is solely used for validation purposes when rebuilding a cluster to process it.

>Note that this attribute can be reliably fetched to know how many unique edges are connected to a `Vtx` ;)

---
## Edges

>**Rule of thumb** : The only information that matter on `Edges` for clusters is their `start` and `end` attribute. Their position in space is ignored so feel free to use those points if they can be relevant to you. 
{: .infos-hl }

`Edges` have a single important piece of data written on them, and everything else is pretty much ignored by PCGEx -- meaning you can use the set of point *almost* as you see fit.
### Edge Endpoints
The `PCGEx/Endpoints` attribute hold the *cached index* of its start `Vtx` when it was written into a cluster.

---
## Clusters

In PCGEx terms, a cluster is "a set of `Vtx` and `Edges` that are interconnected in a finite way" -- in other words, **there is a guaranteed way inside a cluster that a path can be found between any `Vtx` to any other `Vtx` when passing through `Edges`.**

A graph usually has a single set of vertices, but will output as many edge datasets as there are *clusters*. These will be then rebuilt inside the nodes to be processed.

{% include imgc a='docs/vtx-edge-cluster-tag.png' %}  

>If you're unhappy with how clusters have been split in your graph, {% include lk id='Bridge Clusters' %} is here to save your day!
{: .infos }

---
## Graph Output Settings 📌
<br>
{% include imgc a='docs/graph-output-settings.png' %}  

This is a setting block you will see in a form or another on nodes that output sanitized clusters. They expose controls/filters over the `Vtx/Edges` output of the node to make sure the output is **sanitized, i.e, that it can be safely traversed by pathfinding search algorithms.**

See the {% include lk id='Sanitize Clusters' %} node for more infos, as it encapsulate the sanitization behavior embedded in many other nodes.

> If you want more fleshed out controls over `Edges` data & positioning, check out {% include lk id='Write Extras' %} Node!
{: .infos }

---
# Important Notes

>Altering `PCGEx/CachedIndex`, `PCGEx/CachedEdgeNum`, `PCGEx/EdgeStart` or `PCGEx/EdgeEnd` voids the guarantee that PCGEx nodes will work as expected.
{: .warning }

>The `UID` used for the tagging is **NOT** deterministic, and will change *at each execution* of the graph, and for *each node*. It is used under the hood only, and should not be edited, nor relied upon for any kind of tag-related operations. For the same reason, if you create custom tags starting with `PCGEx/Cluster::`, it will break existing vtx/edge associations.  
>**Because of the reliance on the tagging system, if you edit `Vtx` and `Edges` before using them with a PCGEx node, make sure tags are preserved.**
