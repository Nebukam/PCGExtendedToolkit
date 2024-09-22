---
layout: page
family: Sampler
#grand_parent: All Nodes
parent: Sampling
title: Sample Nearest Spline
name_in_editor: "Sample : Nearest Spline"
subtitle: Sample informations from the nearest spline
color: white
summary: The **Sample Nearest Spline** node retrieves and blends spatial data from the target splines within a defined range, enabling fine-tuned sampling methods and weighting for tasks such as spline-based proximity analysis, spatial alignment, and extracting relational data.
splash: icons/icon_sampling-line.svg
tagged: 
    - node
    - sampling
nav_order: 3
inputs:
    -   name : In
        desc : Points that will sample data from targets
        pin : points
    -   name : Point Filters
        desc : Points filters used to determine which points will be processed. Filtered out points will be treated as failed sampling.
        pin : params
    -   name : Targets
        desc : Target splines to read data from
        pin : splines
outputs:
    -   name : Out
        desc : In with extra attributes and properties
        pin : points
---

{% include header_card_node %}

The **Sample Nearest Splines** finds nearest(s) target splines and extract relational spatial data from them.
{: .fs-5 .fw-400 } 

{% include img a='details/sampling-nearest-spline/lead.png' %}

# Sampling
<br>

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Sample Method          | Selects the sampling method. See [Sampling Methods](#sampling-methods). |
| Range Min          | Minimum sampling range. |
| Range Max          | Maximum sampling range.<br>**Use `0` to sample all targets.** |
| Local Range Min          | If enabled, uses a per-point `double` attribute value as minimum sampling range. |
| Local Range Max          | If enabled, uses a per-point `double` attribute value as maximum sampling range. |
| Distance Settings          | TBD |

> Splines that are not within range are ignored.
> If no spline is found within the specified range, the sampling for that point will be marked as **Usuccessful**.
{: .infos }
<br>

|**Weighting**||
| Weight Method          | Selects the method used to compute the weight of each target.<br>*See [Weighting](#weighting)*. |
| Weight Over Distance          | Curve used to sample the final weight of each target. |

---
### Sampling Methods
<br>

| Method       | Description          |
|:-------------|:------------------|
| <span class="ebit">All (Within Range)</span>          | Samples all splines within the specified range. |
| <span class="ebit">Closest Target</span>          | Sample the single closest target spline within the specified range. |
| <span class="ebit">Farthest Target</span>          | Sample the single farthest target spline within the specified range. |

---
### Weighting
<br>

{% include img a='details/sampling-nearest-spline/weighting.png' %}

{% include embed id='settings-weighting' %}

---
# Outputs
Outputs are values extracted from the neighbor(s), and written to attributes on the output points.
{: .fs-5 .fw-400 }  

| Output       | Description          |
|:-------------|:------------------|
|**Generic**||
| <span class="eout">Success</span><br>`bool` | TBD |
{: .soutput }

|**Spatial Data**||
| <span class="eout">Transform</span><br>`FTransform`    | TBD |
| <span class="eout">Look At</span><br>`FVector`     | TBD |
| └─ Align | TBD |
| └─ Use Up from... | TBD |
| └─ Up Vector | TBD |
| <span class="eout">Distance</span><br>`double`     | TBD |
| <span class="eout">Signed Distance</span><br>`double`     | TBD |
| └─ Axis | TBD |
| <span class="eout">Angle</span><br>`double`     | TBD |
| └─ Axis | TBD |
| └─ Range | TBD |
| <span class="eout">Time</span><br>`double`     | TBD |
| <span class="eout">Num Inside</span><br>`int32`     | TBD |
| <span class="eout">Num Samples</span><br>`int32`     | TBD |
| <span class="eout">Closed Loop</span><br>`bool`     | TBD |

> Based on the selected `Sample method`, the output values are a **weighted average** of all the sampled targets. 
> *See [Weighting](#weighting)*.
{: .infos-hl }

---
## Tagging
Some high level tags may be applied to the data based on overal sampling.
<br>

| Tag       | Description          |
|:-------------|:------------------|
| <span class="etag">Has Successes Tag</span>     | If enabled, add the specified tag to the output data **if at least a single spline** has been sampled. |
| <span class="etag">Has No Successes Tag</span>     | If enabled, add the specified tag to the output data **if no spline** was found within range. |

> Note that fail/success tagging will be affected by points filter as well; since filtered out points are considered fails.
{: .warning }