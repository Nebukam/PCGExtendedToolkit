---
layout: page
title: Sampling
subtitle: Data from nearest points, polylines, surfaces, ...
summary: Extract data from surroundings
splash: icons/icon_cat-sampling.svg
color: red
preview_img: previews/index-sampling.png
nav_order: 40
has_children: true
tagged:
    - category-index
---

{% include header_card %}

PCGEx comes with a collection of "sampling" nodes. They're basically all **designed to extract information & data from their surroundings**, one way or another -- from overlaps, to line traces, to bounds or proximity.  
The general philosophy behind sampling nodes is to expose data, and makes no assumption about their possible usage; *hence they all write new data but never modify existing ones; unless you enable blending operations on capable nodes*.
{: .fs-5 .fw-400 } 

> "Sampling" may comes out as a bit of a misnomer : contrary to vanilla PCG *Sampler* nodes, PCGEx' *Sampling* nodes **do not generate new points**, and instead relies on existing points as spatial references to drive queries.
{: .infos-hl }

---
## Sampling Nodes
<br>
{% include card_childs tagged='node' %}