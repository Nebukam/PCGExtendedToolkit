---
layout: page
family: Sampler
#grand_parent: All Nodes
parent: Sampling
title: Sample Nearest Points
name_in_editor: "Sample : Nearest Point"
subtitle: Sample points within a spherical range
color: white
summary: The **Sample Nearest Points** node extracts and blends data from nearby target points within a customizable range, allowing you to fine-tune sampling methods, apply filters, and compute weighted outputs, making it ideal for tasks like proximity-based data collection.
splash: icons/icon_sampling-point.svg
tagged: 
    - node
    - sampling
nav_order: 4
inputs:
    -   name : In
        desc : Points that will sample data from targets
        pin : points
    -   name : Point Filters
        desc : Points filters used to determine which points will be processed. Filtered out points will be treated as failed sampling.
        pin : params
    -   name : Targets
        desc : Target points to read data from
        pin : point
outputs:
    -   name : Out
        desc : In with extra attributes and properties
        pin : points
---

{% include header_card_node %}

The **Sample Nearest Point** grabs and blends attributes & properties from a target dataset, as well as other spatial relationship outputs.
{: .fs-5 .fw-400 } 

{% include img a='details/sampling-nearest-point/lead.png' %}

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

> Points that are not within range are ignored.
> If no point is found within the specified range, the sampling for that point will be marked as **Usuccessful**.
{: .infos }
<br>

|**Distance Details**||
| Source          | TBD |
| Target          | TBD |

|**Weighting**||
| Weight Method          | Selects the method used to compute the weight of each target.<br>*See [Weighting](#weighting)*. |
| Weight Over Distance          | Curve used to sample the final weight of each target. |

---
### Sampling Methods
<br>

| Method       | Description          |
|:-------------|:------------------|
| <span class="ebit">All (Within Range)</span>          | Samples all points within the specified range. |
| <span class="ebit">Closest Target</span>          | Sample the single closest target within the specified range. |
| <span class="ebit">Farthest Target</span>          | Sample the single farthest target within the specified range. |

---
### Weighting
<br>

{% include img a='details/sampling-nearest-point/weighting.png' %}

{% include embed id='settings-weighting' %}

---
# Blending
<br>
{% include embed id='settings-blending-sampling' %}

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
| <span class="eout">Num Samples</span><br>`int32`     | TBD |

> Based on the selected `Sample method`, the output values are a **weighted average** of all the sampled targets. 
> *See [Weighting](#weighting)*.
{: .infos-hl }

---
## Tagging
Some high level tags may be applied to the data based on overal sampling.
<br>

| Tag       | Description          |
|:-------------|:------------------|
| <span class="etag">Has Successes Tag</span>     | If enabled, add the specified tag to the output data **if at least a single target** has been sampled. |
| <span class="etag">Has No Successes Tag</span>     | If enabled, add the specified tag to the output data **if no target** has been sampled. |

> Note that fail/success tagging will be affected by points filter as well; since filtered out points are considered fails.
{: .warning }