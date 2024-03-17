---
layout: page
parent: Orient
grand_parent: Paths
title: LookAt
subtitle: Look at next
color: white
#summary: summary_goes_here
splash: icons/icon_paths-orient.svg
preview_img: docs/splash-orient-lookat.png
toc_img: placeholder.jpg
tagged: 
    - pathsorient
nav_order: 1
---

{% include header_card_node %}

{% include img a='details/modules/orienting-lookat.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| LookAt           | Select what the point should look at.<br>*See [LookAt Target](#lookat-target).*|
| Look at Selector           | The attribute to fetch as a LookAt target if `LookAt == Attribute`.|
| Attribute as Offset           | If enabled, the attribute specified above will be used as a translation offset from the point location, as opposed to a world space position. |

|**Orientation**||
| Orient Axis           | The transform' axis that will be *oriented*. |
| Up Axis           | The Up axis used for the orientation maths. |

## LookAt Target
There are three possibilities:
- Look At Next : orientation will be computed so the point is oriented toward its next neighbor.
- Look At Previous : orientation will be computed so the point is oriented toward its previous neighbor.
- Attribute : orientation will be computed so the point looks at the world space position stored in a local attribute.