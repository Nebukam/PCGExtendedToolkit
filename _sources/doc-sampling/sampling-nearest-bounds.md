---
layout: page
family: Sampler
#grand_parent: All Nodes
parent: Sampling
title: Sample Nearest Bounds
name_in_editor: "Sample : Nearest Bounds"
subtitle: Sample within bounds
color: white
summary: The **Sample Nearest Bounds** node samples points within given bounds, blending spatial attributes from the nearest targets based on selected methods, and outputs the processed points with additional properties and tags.
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
        extra_icon: OUT_Filter
        desc : Points filters used to determine which points will be processed. Filtered out points will be treated as failed sampling.
        pin : params
    -   name : Bounds
        desc : Point bounds to read data from
        pin : points
outputs:
    -   name : Out
        desc : In with extra attributes and properties
        pin : point
---

{% include header_card_node %}

The **Sample Nearest Point** grabs and blends attributes & properties from a target dataset as well as other spatial relationship outputs.  
**Input points sample the targets which bounds contains them, not the other way around.** See {% include lk id='Sample Inside Bounds' %} if that's what you were looking for.
{: .fs-5 .fw-400 } 

{% include img a='details/sampling-nearest-bounds/lead.png' %}

# Sampling
<br>

| Property       | Description          |
|:-------------|:------------------|
|:**Settings**||
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
# Blending
<br>
{% include embed id='settings-blending-sampling' %}

---
# Outputs
Outputs are values extracted from the bounds that encapsulate a given point, and written to attributes on the output points.
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
| <span class="eout">Component-wise Distance</span><br>`FVector`     | TBD |
| └─ Absolute | TBD |
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

---
## Advanced
<br>

| Property       | Description          |
|:-------------|:------------------|
| Process Filtered Out As Fails    | If enabled, mark filtered out points as "failed". Otherwise, just skip the processing altogether.<br>**Only uncheck this if you want to ensure existing attribute values are preserved.**<br>Default is set to true, as it should be on a first-pass sampling. |