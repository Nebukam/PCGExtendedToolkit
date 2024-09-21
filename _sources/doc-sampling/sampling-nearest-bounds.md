---
layout: page
#grand_parent: All Nodes
parent: Sampling
title: Sample Nearest Bounds
name_in_editor: "Sample : Nearest Bounds"
subtitle: Sample within bounds
color: white
summary: The **Sample Nearest Bounds** node explore points within input bounds.
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
    -   name : Bounds
        desc : Point bounds to read data from
        pin : points
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
| Bounds Source          | TBD |
| Weight Remap          | TBD |

---
### Sampling Methods
<br>

| Method       | Description          |
|:-------------|:------------------|
| <span class="ebit">All</span>          | TBD |
| <span class="ebit">Closest Bounds</span>          | TBD |
| <span class="ebit">Farthest Bounds</span>          | TBD |
| <span class="ebit">Largest Bounds</span>          | TBD |
| <span class="ebit">Smallest Bounds</span>          | TBD |

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

> Based on the selected `Sample method`, the output values are a **weighted average** of all the sampled target. 
> *See [Weighting](#weighting)*.
{: .infos-hl }

---
## Tagging
Some high level tags may be applied to the data based on overal sampling.
<br>

| Tag       | Description          |
|:-------------|:------------------|
| <span class="etag">Has Successes Tag</span>     | If enabled, add the specified tag to the output data **if at least a single line trace** has been successful. |
| <span class="etag">Has No Successes Tag</span>     | If enabled, add the specified tag to the output data **if all line trace** failed. |

> Note that fail/success tagging will be affected by points filter as well; since filtered out points are considered fails.
{: .warning }