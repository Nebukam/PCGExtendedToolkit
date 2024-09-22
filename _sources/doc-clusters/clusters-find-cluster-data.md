---
layout: page
family: Cluster
#grand_parent: All Nodes
parent: Clusters
title: Find Clusters
name_in_editor: "Find Clusters"
subtitle: Find matching cluster data.
summary: The **Find Clusters** node locates matching `Vtx` and `Edges` pairs from disordered data collections, helping streamline operations when working with individual clusters.
color: white
splash: icons/icon_misc-draw-attributes.svg
tagged:
    - node
    - clusters
see_also:
    - Working with Clusters
nav_order: 40
inputs:
    -   name : Vtx
        desc : Endpoints of the input Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the input Vtxs
        pin : points
    -   name : Vtx or Edges
        desc : Key data to be matched against. Availability depends on the selected Search Mode.
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

The **Find Clusters** node is a handy helper to find matching `Vtx` and `Edges` data from more-or-less scrambled collections of data.
{: .fs-5 .fw-400 } 

> Note that this node is not an alternative to {% include lk id='Sanitize Clusters' %}, it only works at the collection level and doesn't inspect per-point data.
{: .warning }

{% include img a='details/clusters-find-cluster-data/lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| Search Mode          | Defines how to search and what the node will output.<br>*Some search modes will unlock additional inputs.* |
| Skip Trivial Warnings          | If enabled, will skip trivial warnings about unmatched pairs and missing siblings. |
| Skip Important Warnings          | If enabled, will skip important warnings that you may get from incomplete data. |

The ability to skip warning is there mostly because I found this node to be often purposefully used with "bad" data, and you don't want to be spammed with infos you already know. **Just don't forget to uncheck these if for some reason the node is not generating the output you'd expect;** it will definitely provide some details on why.

## Search modes
### All

This search mode finds all valid `Vtx` & `Edges` pair and output the workable ones.  
If a `Vtx` is found but no matching `Edges` are, or vice versa (`Edges` found but no assocated `Vtx`), **They will be ommited from the ouput.**

### Vtx from Edges

This search modes finds the single valid `Vtx` associated with the `Edges` found in the corresponding node input.  
**If none is found, the entire output will be empty.**

### Edges from Vtx

This search modes finds all valid `Edges` associated with the `Vtx` found in the corresponding node input.  
**If none is found, the entire output will be empty.**

---
# Usage in loops

Looping over individual clusters can look like a needlessly complex task: how do you get the right `Edges` with the right `Vtx` or vice versa? Well, this node is there to help!
{: .fs-5 .fw-400 } 

In order to loop over individual clusters, you need to loop over the `Edges` dataset, not the `Vtx`; **because `Vtx` dataset may be bound to more than one `Edges` set.** *Check out {% include lk id='Working with Clusters' %} for more infos on that.*  
Forward `Vtx` groups to another loop input and use the **Vtx from Edges** input, like so:

{% include img a='placeholder-wide.jpg' %}

