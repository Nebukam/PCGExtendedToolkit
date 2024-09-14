---
layout: page
#grand_parent: All Nodes
parent: Misc
title: Lloyd Relax
subtitle: Two nodes to conveniently relax point data.
#summary: TBD
color: white
splash: icons/icon_misc-write-index.svg
has_children: true
use_child_thumbnails: true
tagged: 
    - clusters
    - node
nav_order: 200
---

{% include header_card_toc %}

The **Lloyd Relax* nodes are providing an implementation of the [Lloyd' Algorithm](https://en.wikipedia.org/wiki/Lloyd%27s_algorithm), in both 2D and 3D
{: .fs-5 .fw-400 } 

> Since every relaxation step involves building the Voronoi diagram of the set of points, needless to say this node is **very expensive**.
{: .warning }

{% include card_childs tagged='node' %}