---
layout: page
#grand_parent: All Nodes
parent: Clusters
title: Packing Clusters
subtitle: Two nodes to conveniently pack/unpack cluster data.
summary: TBD
color: white
splash: icons/icon_misc-write-index.svg
preview_img: placeholder.jpg
has_children: true
use_child_thumbnails: true
tagged: 
    - clusters
    - node
nav_order: 200
---

{% include header_card_toc %}

Packing and umpacking cluster are a pair of nodes designed to "pack" a single cluster' `Vtx` and `Edges` into a single point group. **This is especially useful if you want to safe clusters as PCG Point Data Assets.**
{: .fs-5 .fw-400 } 

>If you're here because you're looking for a way to loop over individual cluster, while packing/unpacking will *work*, it's not efficient. Prefer to loop over `Edges` and use {% include lk id='Find Clusters' %} inside the loop' body.
{: .infos-hl }

---
## Packing & Unpacking Clusters
<br>
{% include card_childs tagged='clusters' %}