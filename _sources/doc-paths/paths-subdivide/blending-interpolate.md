---
layout: page
grand_parent: Paths
parent: Subdivide
title: ⋰ Interpolate
subtitle: Interpolates values between first and last point.
color: blue
summary: Processed subpoints properties & attributes will lerp between the first and last subpoints.
splash: icons/icon_path-subdivide.svg
tagged: 
    - blending
nav_order: 1
---

{% include header_card_node %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Blending Over           | Defines the method used to compute the alpha value for the interpolation. |
| Blending Settings           | See{% include lk id='Blending' a='#common-properties' %} *(Common properties)* |

## Blend Over Methods

- [Distance](#distance)
- [Count](#count)
- [Fixed](#fixed)

### Distance
{% include img a='details/modules/blendover-distance.png' %}  

`Distance` will first calculate the total distance covered by the subpoints (*from current to next*), and compute their individual lerp as `Local Distance / Total Distance`.  
**This usually gives more better-looking results if the distance between points is irregular;** but the difference in values between a point and the next can either be brutal or negligible.

*If points are equally spaced, it will look very similar to `Point Count`.*

### Count
{% include img a='details/modules/blendover-index.png' %}  

`Point Count` uses the current point index, and compute their individual lerp as `Current Index / Total Point Count`.
**Using this method is equivalent of using a fixed blending step between each point.**

*If points are irregularly spaced, it may look a bit random.*

### Fixed
Uses a unique fixed lerp value for each point.
