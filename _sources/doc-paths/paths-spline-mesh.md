---
layout: page
family: Path
#grand_parent: All Nodes
parent: Paths
title: Path Spline Mesh
name_in_editor: "Path : Spline Mesh"
subtitle: Create SplineMesh components from paths
summary: The **Path Spline Mesh** node generates spline meshes along a path with detailed per-segment settings, allowing for mesh and data distribution from asset collections or attribute sets, and includes advanced options for tangents, scaling, justification, and distribution to control mesh behavior and appearance.
color: white
splash: icons/icon_paths-orient.svg
see-also:
    - Path Spline Mesh (Simple)
tagged: 
    - node
    - paths
nav_order: 2
inputs:
    -   name : Paths
        desc : Paths to be used for SplineMesh
        pin : points
---

{% include header_card_node %}

The **Path Spline Mesh** node creates a spline mesh from an existing path, with extensive per-segment settings. It works very similarily to the {% include lk id='Asset Staging' %} node and uses the same set of options.
{: .fs-5 .fw-400 } 

{% include img a='details/paths-spline-mesh/lead.png' %}

# Properties
<br>

{% include embed id='settings-closed-loop' %}

---
## Asset Collection
<br>
Controls the collection of meshes & data that will be distributed to the path on its individual segments.

| Property       | Description          |
|:-------------|:------------------|
| Source           | Which type of source to use for the asset collection.<br>Can be either `Asset` for an {% include lk id='Asset Collection' %} picker, or `Attribute Set`, which will reveal a new input pin that accepts an attribute set. |
| Asset Collection           | If `Asset` is selected as a source, this is the {% include lk id='Asset Collection' %} that will be used for staging points.<br>*See [Available Asset Collections](#available-asset-collections).* |

When using the `Attribute Set` source, the node will create a temp, internal {% include lk id='Asset Collection' %} from the source attribute set. 

|: **Attribute Set Details** :||
| Asset Path Source Attribute           | The name of the attribute within the Attribute Set that contains the entry' **Asset Path**.<br>*`FSoftObjectPath` is preferred, but `FString` and `FName` are supported.* |
| Weight Source Attribute           | The name of the attribute within the Attribute Set that contains the entry' **Weight**.<br>*`int32` is preferred, but `float`, `double` and `int64` are supported.* |
| Category Source Attribute           | The name of the attribute within the Attribute Set that contains the entry' **Category**.<br>*`FName` is preferred, but `FString` is supported.* |

> While it's a needed option, keep in mind that using an attribute set prevents access to any asset cached data.
> **As such, all assets from the Attribute Set will be first loaded (asynchronously) in memory in order to compute their bounds**; before the node execution can properly start.

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

> The axis picked for `Spline Mesh Axis` will be ignored for Scale to Fit & Justification.
{: .error }

---
## Scale to Fit

{% include img a='details/paths-spline-mesh/scale-to-fit.png' %}

Scale the spawned asset bounds in order to fit within the host point' bounds.  

> Note that the maths are computed against the point' bounds, **not** the segment AABB.
{: .warning }

| Scale to Fit Mode           | Which type of scale-to-fit mode is to be applied. `None` disables this section, `Uniform` applies the same rule to each individual component, while `Individual` lets you pick per-component rules.  |
| Scale to Fit *(value)*           | If `Asset` is selected as a source, this is the {% include lk id='Asset Collection' %} that will be used for staging points. |  

You can use the following rules:

|: Scale to Fit      ||
|:-------------|:------------------|
| {% include img a='details/assets-staging/enum-stf-none.png' %}           | <span class="ebit">None</span><br>Disable the scaling rule. |
| {% include img a='details/assets-staging/enum-stf-fill.png' %}           | <span class="ebit">Fill</span><br>Scale the asset so it fills the point' bounds. |
| {% include img a='details/assets-staging/enum-stf-min.png' %}           | <span class="ebit">Min</span><br>Scale the asset so it fits snuggly within the minimum point' bounds. |
| {% include img a='details/assets-staging/enum-stf-max.png' %}           | <span class="ebit">Max</span><br>Scale the asset so it fits snuggly within the maximum point' bounds. |
| {% include img a='details/assets-staging/enum-stf-avg.png' %}           | <span class="ebit">Average</span><br>Scale the asset so it fits the average of the point' bounds.|
{: .enum }

---
## Justification

{% include img a='details/paths-spline-mesh/justification.png' %}

Offset the spawned asset bounds relative to the host point' bounds.  
Justification is done & tweaked per-component.  

> Note that the maths are computed against the point' bounds, **not** the segment AABB.
{: .warning }

|: **Per component** :||
| From          | The location within the **Asset** bounds that will be justified *To* the point' bounds. <br>*i.e, from which location in the asset do i start moving.*  |
| To           | The location withn the **Point** bounds to which the **Asset** bounds will be justified. <br>*i.e, to which location in the point do i want to go.* |  
|: **Consolidated custom inputs** :||
| Custom from Vector Attribute         | An `FVector` whose individual component will be used to drive `From` properties set to `Custom`.<br>*Prefer this consolidated approach if you're using custom values on more than one component.* |
| Custom to Vector Attribute         | An `FVector` whose individual component will be used to drive `To` properties set to `Custom`. *Prefer this consolidated approach if you're using custom values on more than one component.* |
| Justify to One         | If enabled, justification ignore the original path' point bounds, and uses 1,1,1 bounds instead.  |

### From
You can use the following rules for `From`:

|: Justify From     ||
|:-------------|:------------------|
| {% include img a='details/assets-staging/enum-justify-from-center.png' %}           | <span class="ebit">Center</span><br>Uses the asset bounds' local center as reference point. |
| {% include img a='details/assets-staging/enum-justify-from-min.png' %}           | <span class="ebit">Min</span><br>Uses the asset bounds' min as reference point. |
| {% include img a='details/assets-staging/enum-justify-from-max.png' %}           | <span class="ebit">Max</span><br>Uses the asset bounds' max as reference point.|
| {% include img a='details/assets-staging/enum-justify-from-pivot.png' %}           | <span class="ebit">Pivot</span><br>Uses the asset pivot as reference point, ignoring bounds. |
| {% include img a='details/assets-staging/enum-justify-from-custom.png' %}           | <span class="ebit">Custom</span><br>Uses a lerped reference point between the asset bounds' min & max.<br>*Value is expected to be in the range 0-1 but isn't clamped.* |
{: .enum }

### To
You can use the following rules for `To`:

|: Justify To     ||
|:-------------|:------------------|
| {% include img a='details/assets-staging/enum-justify-to-same.png' %}           | <span class="ebit">Same</span><br>Auto-selects the same justification as `From`, but computed against the point' bounds. |
| {% include img a='details/assets-staging/enum-justify-to-center.png' %}           | <span class="ebit">Center</span><br>Uses the point bounds' local center as reference point. |
| {% include img a='details/assets-staging/enum-justify-to-min.png' %}           | <span class="ebit">Min</span><br>Uses the point bounds' min as reference point. |
| {% include img a='details/assets-staging/enum-justify-to-max.png' %}           | <span class="ebit">Max</span><br>Uses the point bounds' max as reference point. |
| {% include img a='details/assets-staging/enum-justify-to-pivot.png' %}           | <span class="ebit">Pivot</span><br>Uses the point bounds' pivot, ignoring bounds. |
| {% include img a='details/assets-staging/enum-justify-to-custom.png' %}           | <span class="ebit">Custom</span><br>Uses a lerped reference point between the asset bounds' min & max.<br>*Value is expected to be in the range 0-1 but isn't clamped.* |
{: .enum }

---
## Distribution Settings

Distribution drives how assets are selected within the collection & assigned to points.

|: Seed components      | *Seed components lets you choose which seed source you want to combined to drive randomness.*   |
| <span class="ebit">None</span>           | Will only use the point' seed. |
| <span class="ebit">Local</span>           | The local user-set seed *see property below* |
| <span class="ebit">Settings</span>           | This node' Settings seed. |
| <span class="ebit">Component</span>           | The parent PCG component' seed. |
{: .enum }

### Distribution

|: Distribution     ||
| <span class="ebit">Index</span>           | Index-based selection within the collection.<br>*Enable a lot of additional options.*|
| <span class="ebit">Random</span>           | Plain old random selection. |
| <span class="ebit">Weighted Random</span>          | Weighted random selection, using entries' `Weight` property. |
{: .enum }

### Index Settings

Index settings offer granular controls over what the `Index` actually is, and how it 
is used.

| Index Setting       | Description          |
|:-------------|:------------------|
| Pick Mode           | Choose in which ordered context the input index should be used.  |
| Index Safety           | Defines how the index value should be sanitized/post-processed. |
| Index Source           | The source of the index value. |
| Remap Index to Collection Size           | If enabled, the input index is first remapped to the size of the collection.<br>**This enable the use of basically any input value and distribute the entierety of the collection over its range.**<br>*Note that this can have a noticeable performance impact since all input indices must be loaded in memory first to find the min/max range.*|
| Truncate Remap           | Lets you choose how the remapped value (floating point) should be truncated to an integer. |

### Index Safety
{% include embed id='settings-index-safety' %}

---
## Target Actor

Lets you pick which actor to attach the spline components to.  
Uses the PCG Component' owner if the provided value is invalid.

---
## Additional Outputs

Lets you output the Weight of the selection to each node, using different post-processing methods.  
**This can be very handy to identify "rare" spawns and preserve them during self-pruning operations.**

### Tagging Components

Lets you pick which tags to add/forward to the output spline mesh components.  

{% include embed id='settings-tagging-assets' %}
