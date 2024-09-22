---
layout: page
family: Path
#grand_parent: All Nodes
parent: Paths
title: Write Paths Properties
name_in_editor: "Path : Properties"
subtitle: Compute extra path informations
color: white
summary: The **Write Path Properties** node enhances path data by computing both point-level details (e.g., distances, normals, and directions) and path-level attributes like averaged direction, convexity, centroids, and additional spatial insights.
splash: icons/icon_edges-extras.svg
tagged: 
    - node
    - paths
nav_order: 4
inputs:
    -   name : Paths
        desc : Paths to compute information from
        pin : points
outputs:
    -   name : Paths
        desc : Paths with extra information
        pin : points
---

{% include header_card_node %}

The **Write Path Properties** node outputs a variety of useful points data, as well as path-level data; like direction between path neighboring points, averaged path direction, whether a path is concave or convex; that kind of stuff.
{: .fs-5 .fw-400 } 

{% include img a='details/paths-write-properties/lead.png' %}

# Properties
<br>

{% include embed id='settings-closed-loop' %}

# Outputs
## Output - Path
<br>

| Output       | Description          |
|:-------------|:------------------|
| <span class="eout">Path Length</span><br>`double` | TBD |
| <span class="eout">Path Direction</span><br>`FVector` | TBD |
| <span class="eout">Path Centroid</span><br>`FVector` | TBD |
{: .soutput }

---
## Output - Points
<br>

| Output       | Description          |
|:-------------|:------------------|
| Up Vector Type | Type of Up Vector source. Either a `Constant` or a per-point `Attribute`. |
| Up Vector <br>*(Constant or Attribute)* | Up vector used for computing spatial data. |
| Average Normals | Whether normals should be averaged or not. |
| <span class="eout">Dot</span><br>`double` | TBD |
| <span class="eout">Angle</span><br>`double` | TBD |
| └─ Range | TBD |
| <span class="eout">Distance To Next</span><br>`FVector` | TBD |
| <span class="eout">Distance To Prev</span><br>`FVector` | TBD |
| <span class="eout">Distance To Start</span><br>`FVector` | TBD |
| <span class="eout">Distance To End</span><br>`FVector` | TBD |
| <span class="eout">Point Time</span><br>`FVector` | TBD |
| <span class="eout">Point Normal</span><br>`FVector` | TBD |
| <span class="eout">Point Binormal</span><br>`FVector` | TBD |
| <span class="eout">Direction To Next</span><br>`FVector` | TBD |
| <span class="eout">Direction To Prev</span><br>`FVector` | TBD |
{: .soutput }

---
## Tagging
<br>

| Tag       | Description          |
|:-------------|:------------------|
| <span class="etag">Concave Tag</span>     | If enabled, add the specified tag to the output data **if the path is concave**. |
| <span class="etag">Convex Tag</span>     | If enabled, add the specified tag to the output data **if the path is convex**. |