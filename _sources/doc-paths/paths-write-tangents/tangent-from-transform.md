---
layout: page
grand_parent: Paths
parent: Write Tangents
title: ∢ From Transform
subtitle: Transform-based tangents
color: blue
#summary: Index-based moving-average sampling
splash: icons/icon_path-tangents.svg
tagged: 
    - pathstangents
nav_order: 1
---

{% include header_card_node %}

The **From Transform** writes the selected transform axis (in world space) of the point as a tangent.
{: .fs-5 .fw-400 } 

{% include img a='placeholder-wide.jpg' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|:**Settings**||
| Axis           | Lets you pick the transform axis  |

|**Axis**||
| Forward           | Transform forward axis.<br>*Positive X* |
| Backward           | Transform backward axis.<br>*Negative X* |
| Right           | Transform right axis.<br>*Positive Y* |
| Left           | Transform left axis.<br>*Negative Y* |
| Up           | Transform up axis.<br>*Positive Z* |
| Down           | Transform down axis.<br>*Negative Z* |

> Note that as the time of writing, this tangent require scaling as it's using a normalized vector.
> Additional scaling options will be exposed in order to make it easier to scale when used specifically for start/end points.
{: .infos }