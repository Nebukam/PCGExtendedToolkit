---
layout: page
family: Sampler
#grand_parent: All Nodes
parent: Sampling
title: Sample Line Trace
name_in_editor: "Sample : Line Trace"
subtitle: Sample environment using line casting
color: white
summary: The **Line Trace** node performs a single line trace for each point, using a local attribute or property as direction & size.
splash: icons/icon_sampling-guided.svg
#warning: This node works with collisions and as such can be very expensive on large datasets.
tagged: 
    - node
    - sampling
nav_order: 2
inputs:
    -   name : In
        desc : Points that will be used as origin for line tracing
        pin : points
    -   name : Point Filters
        desc : Points filters used to determine which points will be processed. Filtered out points will be treated as failed sampling.
        pin : params
    -   name : Actor References
        desc : Points containing actor references, if that mode is selected.
        pin : point
outputs:
    -   name : Out
        desc : In with extra attributes and properties
        pin : points
---

{% include header_card_node %}

The **Line Trace** node allows you to perform a single per-point line trace and output data from that trace.
{: .fs-5 .fw-400 } 

> Please note that this node is **not** tracking traced actors. If you change the sampled environment you will need to manually regenerate the graph unless something else already triggers a refresh in editor.
{: .comment }

{% include img a='details/sampling-line-trace/lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Surface Source     | Select where you want to read surfaces from: either `All` (revealing additional actors picking parameters), or `Actor References`, to sample only specific actors. |
| Actor Reference     | Attribute to read actor reference from the Actor References input data. |

|**Trace**||
| Direction     | `FVector` attribute to read per-point trace direction from. |
| Max Distance     | Maximum distance for the trace |
| Local Max Distance     | If enabled, fetches the max distance on a per-point basis using the specified attribute. |

---
# Outputs
Outputs are values extracted from the line trace, and written to attributes on the output points.
{: .fs-5 .fw-400 }  


| Output       | Description          |
|:-------------|:------------------|
|**Generic**||
| <span class="eout">Success</span><br>`bool` | TBD |
{: .soutput }

|**Spatial Data**||
| <span class="eout">Location</span><br>`FVector`     | TBD |
| <span class="eout">Look At</span><br>`FVector`     | TBD |
| <span class="eout">Normal</span><br>`FVector`     | TBD |
| <span class="eout">Distance</span><br>`double`     | TBD |
| <span class="eout">Is Inside</span><br>`bool`     | TBD |
{: .soutput }

|**Actor Data**||
| <span class="eout">Actor Reference</span><br>`FSoftObjectPath`     | TBD |
| <span class="eout">Phys Mat</span><br>`FSoftObjectPath`     | TBD |
{: .soutput }

---
# Extra properties

## Forwarding
*Data forwarding settings from the Actor References input points, if enabled.*
<br>
{% include embed id='settings-forward' %}

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

---
## Collision Settings
<br>
{% include embed id='settings-collisions' %}

---
## Advanced
<br>

| Property       | Description          |
|:-------------|:------------------|
| Process Filtered Out As Fails    | If enabled, mark filtered out points as "failed". Otherwise, just skip the processing altogether.<br>**Only uncheck this if you want to ensure existing attribute values are preserved.**<br>Default is set to true, as it should be on a first-pass sampling. |