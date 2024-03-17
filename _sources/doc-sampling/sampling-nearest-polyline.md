---
layout: page
parent: Sampling
#grand_parent: All Nodes
title: Sample Nearest Polyline
subtitle: Sample informations from the nearest polyline
color: white
summary: The **Sample Nearest Polyline** node explore polylines within a range using various methods. Define sampling range, weight targets, and obtain useful attributes.
splash: icons/icon_sampling-line.svg
preview_img: docs/splash-sample-nearest-polyline.png
toc_img: placeholder.jpg
tagged: 
    - node
    - sampling
nav_order: 3
---

{% include header_card_node %}

{% include img a='details/details-sample-nearest-point.png' %} 

> Each output property is written individually for each point.  
> *Each polyline will yield a single sample point: the closest point on the closest segment, within the specified range.*
{: .comment }

| Property       | Description          |
|:-------------|:------------------|
|**Sampling**||
| Sample Method          | Selects the sampling method. See [Sampling Methods](#sampling-methods). |
| Range Min          | Minimum sampling range. |
| Range Max          | Maximum sampling range.<br>**Use `0` to sample all segments.** |

> Segments that are not within range are ignored.
> If no polyline segment is found within the specified range, the sampling for that point will be marked as **Usuccessful**.
{: .infos }

|**Weighting**||
| Weight Method          | Selects the method used to compute the weight of each target.<br>*See [Weighting](#weighting)*. |
| Weight Over Distance          | Curve used to sample the final weight of each target. |

|**Outputs**||
| **Success** Attribute Name     | Writes a boolean attribute to each point specifying whether the sampling has been successful (`true`) or not (`false`). |
| **Location** Attribute Name     | Writes the location sampled on the polyline, as an `FVector`. |
| **Look at** Attribute Name     | Writes the direction from the point to the location sampled on the polyline, as an `FVector`. |
| **Normal** Attribute Name     | Writes the normal of the point at the location sampled on the polyline, as an `FVector`. |
| Normal Source | Which direction to use as an Up vector for the Normal cross-product maths. |
| **Distance** Attribute Name     | Writes the distance between the point and the location sampled on the polyline, as a `double`. |
| **Signed Distance** Attribute Name     | Writes the signed distance between the point and the location sampled on the polyline, as a `double`. |
| Signed Distance Axis | Which axis to use to determine whether the distance is positive or negative (toward/away).<br>*Currently based on point Transform, this will likely change in the future to an attribute selector.* |
| **Angle** Attribute Name     | Writes the angle between the point and the transform sampled on the polyline, as a `double`. |
| Angle Axis | Which axis to use to determine the angle sign/range (toward/away) |
| Angle Range | The output range for the `Angle` value. |
| **Time** Attribute Name     | Writes the time (*in `Spline` terms*) of the location sampled on the polyline, as a `double`. |

> Based on the selected `Sample method`, the output values are a **weighted average** of all the sampled positions. 
> *See [Weighting](#weighting)*.
{: .infos-hl }

## Sampling Methods

| Method       | Description          |
|:-------------|:------------------|
| Within Range          | Samples all polylines within the specified range. |
| Closest Target          | Sample the single closest polyline within the specified range. |
| Farthest Target          | Sample the single farthest polyline within the specified range. |
| Target Extents          | Reverse the sampling mechanisms so points will sample the targets which `Extents` contains them.<br>**At the time of writing, will only check targets which position in world space is within range.**<br>*It is recommend to use a max range of `0` with this method.* |

{% include embed id='settings-weighting' %}

---
## Weighting

{% include img a='docs/relax/range.png' %} 

> Note that the `Effective Range` method tends to spread/scale the input set of values -- but allows one to leverage the full range of the curve no matter the min/max input values.  
> **Hence, using `Full Range` with only high (or low) input value will only sample a very narrow portion of the curve.**
{: .infos-hl }

---
# Inputs
## In
Points that will sample the input targets.  
Each point position in world space will be used as a center for a spherical query of the surrounding polylines' segments. 

## In Targets
Any number of polylines that will be sampled by each point of each dataset pluggued in the `In` pin.

> Sampling target location for each target polyline is the **closest point on the closest segment**.
{: .error-hl }

---
# Outputs
## Out
Same as input, with additional metadata.