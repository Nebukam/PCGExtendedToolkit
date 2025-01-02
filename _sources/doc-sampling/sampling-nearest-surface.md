---
layout: page
family: Sampler
#grand_parent: All Nodes
parent: Sampling
title: Sample Nearest Surface
name_in_editor: "Sample : Nearest Surface"
subtitle: Sample information from the nearest mesh collision
color: white
summary: The **Sample Nearest Surface** node performs spherical queries to find the closest point on nearby simple colliders within a set radius, sampling spatial data and actor references from overlapping surfaces and applying tags based on query results.
splash: icons/icon_sampling-surface.svg
#warning: This node works with collisions and as such can be very expensive on large datasets.
tagged: 
    - node
    - sampling
nav_order: 1
inputs:
    -   name : In
        desc : Points that will be used as origin for finding & sampling the nearest surface
        pin : points
    -   name : Point Filters
        extra_icon: OUT_Filter
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

The **Sample Nearest Surface** node allows you to perform a single per-point spherical query to find the closest point on the closest overlapping collider.
{: .fs-5 .fw-400 } 

> Please note that this node is **not** tracking actors. If you change the sampled environment you will need to manually regenerate the graph unless something else already triggers a refresh in editor.
{: .comment }

> Under the hood this find the closest point on the closest *collider* -- this feature **is only supported for simple collider and won't work on complex ones**.
{: .error }

{% include img a='details/sampling-nearest-surface/lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|:**Settings**||
| Surface Source     | Select where you want to read surfaces from: either `All` (revealing additional actors picking parameters), or `Actor References`, to sample only specific actors. |
| Actor Reference     | Attribute to read actor reference from the Actor References input data. |
| Max Distance     | Maximum radius of the overlapping sphere |
| Local Max Distance     | If enabled, fetches the overlap sphere radius on a per-point basis using the specified attribute. |

---
# Outputs
Outputs are values extracted from the overlap query, and written to attributes on the output points.
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
| <span class="etag">Has Successes Tag</span>     | If enabled, add the specified tag to the output data **if at least a single query** has been successful. |
| <span class="etag">Has No Successes Tag</span>     | If enabled, add the specified tag to the output data **if all queries** failed. |

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