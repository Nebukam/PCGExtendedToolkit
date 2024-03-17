---
layout: page
parent: Smooth
grand_parent: Paths
title: Radius
subtitle: Radius & position-based sampling
color: white
#summary: Radius & position-based sampling
splash: icons/icon_path-smooth.svg
preview_img: docs/splash-smooth-radius.png
toc_img: placeholder.jpg
tagged: 
    - pathsmoothing
nav_order: 1
---

{% include header_card_node %}


> Note that because of how the maths works for this module, it can be used with any input data: **whether the points are ordered as `paths` or not doesn't matter.**
{: .infos-hl }

{% include img a='details/modules/smoothing-radius.png' %} 
{% include img a='docs/smooth/radius.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Blend Radius           | Radius within which neighbor points will be sampled. |

