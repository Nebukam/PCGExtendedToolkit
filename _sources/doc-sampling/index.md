---
layout: page
title: Sampling
subtitle: Data from nearest points, polylines, surfaces, ...
summary: Extract data from surroundings
splash: icons/icon_cat-sampling.svg
preview_img: previews/index-sampling.png
nav_order: 40
has_children: true
tagged:
    - category
---

{% include header_card %}

PCGEx comes with a collection of "sampling" node, which may comes out as a bit of a misnomer. They're basically all **designed to extract information & data from their surroundings**, one way or another -- from overlaps, to line traces, to bounds or proximity.
{: .fs-5 .fw-400 } 

---
## Sampling Nodes
<br>
{% include card_childs tagged='node' %}