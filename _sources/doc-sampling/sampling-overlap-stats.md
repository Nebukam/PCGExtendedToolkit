---
layout: page
family: Sampler
#grand_parent: All Nodes
parent: Sampling
title: Sample Overlap Stats
name_in_editor: "Sample : Overlap Stats"
subtitle: Compute overlap statistics between point collections
color: white
summary: The **Sample Overlap Stats** node computes overlap statistics between point collections, providing per-point and per-collection overlap data such as count, sub-count, and relative measures, with optional tagging for overlap detection.
splash: icons/icon_sampling-point.svg
tagged: 
    - node
    - sampling
nav_order: 4
inputs:
    -   name : In
        desc : Point collections to be checked against each other
        pin : points
    -   name : Point Filters
        extra_icon: OUT_Filter
        desc : Points filters used to determine which points will be processed. Filtered out points will be treated as not overlapping.
        pin : params

outputs:
    -   name : Out
        desc : In with extra attributes and properties
        pin : points
---

{% include header_card_node %}

The **Sample Overlap Stats** is designed to extract per-point overlap data **between entire collections**. *It is a bit of a special node, in the sense that it works cross-inputs.*   
It is primarily designed for `PCGDataAsset` workflows, **to check overlaps against external points**.
{: .fs-5 .fw-400 } 

{% include img a='details/sampling-overlap-stats/lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|:**Settings**||
| Bounds Source          | Which point bounds to use. |
| Min Threshold          | Minimum overlap threhsold required for an overlap to be registered. |
| Threshold Measure          | Let you pick how the overlap threshold is measured. Either as a `Relative` measure (*in which case the Min Threhsold is treated as an overlap percentage*), or a fixed, `Discrete` penetration measurement, *in which case the threshold is treated as world units.* |

---
# Outputs
Outputs are a mix of per-collection & per-point overlap data.
{: .fs-5 .fw-400 }  

| Output       | Description          |
|:-------------|:------------------|
| <span class="eout">Overlap Count</span><br>`int32` | This represents the number of other input collection that have a registered overlap with this point. |
| <span class="eout">Overlap Sub Count</span><br>`int32` | This represents the number of other individual points that have a registered overlap with this point, all inputs combined.<br>*Depending on your data, this number can be very high.* |
| <span class="eout">Relative Overlap Count</span><br>`double` | This is the same as `Overlap Count`, but normalized against the maximum value found during initial processing. |
| <span class="eout">Relative Overlap  SubCount</span><br>`double` | This is the same as `Overlap Sub Count`, but normalized against the maximum value found during initial processing. |
{: .soutput }

---
## Tagging
Some high level tags may be applied to the data based on overlap sampling. This comes very handy to optimize further operations only on overlapping elements, especially if you intend to pair this node with {% include lk id='Discard Points by Overlap' %}.
<br>

| Tag       | Description          |
|:-------------|:------------------|
| <span class="etag">Has Any Overlap</span>     | If enabled, add the specified tag to the output data **if at least a single overlap** has been found. |
| <span class="etag">Has No OVerlap</span>     | If enabled, add the specified tag to the output data **if no overlap** has been found. |

> Note that overlap/no-overlap tagging will be affected by points filter as well; since filtered out points are considered fails.
{: .warning }