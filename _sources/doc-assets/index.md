---
layout: page
title: Staging
subtitle: Data Asset & Staging nodes
summary: Prepare points for spawning meshes & actors
splash: icons/icon_component.svg
preview_img: previews/index-assets.png
color: red
nav_order: 30
has_children: true
tagged:
    - category-index
---

{% include header_card %}

This section contains data asset & staging utilities. Staging nodes rely on {% include lk id='Asset Collection' %} to **associate points & edit them based on asset properties, such as bounding box.**
{: .fs-5 .fw-400 } 

---
## Staging Nodes
<br>
{% include card_childs tagged='staging' %}

---
## {% include lk id='Asset Collection' %}s
<br>
{% include card_childs reference="Asset Collection" tagged='assetcollection' %}