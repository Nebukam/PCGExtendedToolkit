---
layout: page
family: Path
#grand_parent: All Nodes
parent: Paths
title: Path Spline Mesh (Simple)
name_in_editor: "Path : Spline Mesh (Simple)"
subtitle: Create SplineMesh components from paths
summary: The **Path Spline Mesh (Simple)** node generates spline meshes along using per-point attributes.
color: white
splash: icons/icon_paths-orient.svg
see-also:
    - Path Spline Mesh
tagged: 
    - node
    - paths
nav_order: 3
inputs:
    -   name : Paths
        desc : Paths to be used for SplineMesh
        pin : points
---

{% include header_card_node %}

The **Path Spline Mesh** node creates a spline mesh from an existing path. It's a lightweight alternative to the {% include lk id='Path Spline Mesh' %} node, that rely on pre-existing per-point data to build the spline mesh.  
*As a rule of thumb, it's also more straightforward to use when your spline doesn't need complex fitting & distribution options.*
{: .fs-5 .fw-400 } 

{% include img a='details/paths-spline-mesh-simple/lead.png' %}

# Properties
<br>

{% include embed id='settings-closed-loop' %}

| Property       | Description          |
|:-------------|:------------------|
| Asset Path           | Name of the `FSoftObjectPath` attribute that contains the mesh associated with the point' segment. |

---
## Tangents
<br>

While disabled by default, Tangents play a crucial role in giving the spline mesh a correct deformation. **It's generally preferable to compute some using {% include lk id='Write Tangents' %} beforehand.**

| Property       | Description          |
|:-------------|:------------------|
| Apply Custom Tangents           | When enabled, applies `FVector` tangents attributes to path segments. |
| Arrive Tangent Attribute           | Per-point attribute to read the arrive tangent from. |
| Leave Tangent Attribute           | Per-point attribute to read the leave tangent from. |
| Spline Mesh Axis           | Chooses which mesh local axis will be aligned along the spline mesh' segment. |

> Note that the way arrive/leave tangent works is that each spline mesh segment uses the current point' `leave` value, and the *next* point `arrive` value.
{: .comment }  


---
## Output

Lets you output the Weight of the selection to each node, using different post-processing methods.  
**This can be very handy to identify "rare" spawns and preserve them during self-pruning operations.**