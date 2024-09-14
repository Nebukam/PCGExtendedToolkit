---
layout: page
#grand_parent: All Nodes
parent: Staging
title: Asset Staging
name_in_editor: Asset Staging
subtitle: Prepare points before spawning.
summary: The **Asset Staging** node lets your prepare points for spawning assets.
color: white
splash: icons/icon_paths-orient.svg
see_also: 
    - Asset Collection
tagged: 
    - node
    - staging
nav_order: 1
inputs:
    -   name : In
        desc : Points to be prepared
        pin : points
    -   name : Attribute Set
        desc : Optional attribute set to be used as a collection, if the node is set-up that way.
        pin : params
outputs:
    -   name : Out
        desc : Modified points, with added attributes
        pin : points
---

{% include header_card_node %}

The **Asset Staging** exists for many reasons, but primarily to "pre-spawn" assets from an {% include lk id='Asset Collection' %}, and modify points according to various rules, in relation to their associated asset.
{: .fs-5 .fw-400 } 

This is especially useful if you want to have pruning control on overlaps, or require very tight placement rules no matter how the assets have been authored (pivot point location etc)

{% include img a='placeholder-wide.jpg' %}

---
# Properties

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
## Scale to Fit

{% include img a='placeholder-wide.jpg' %}

Scale the spawned asset bounds in order to fit within the host point' bounds.  


| Scale to Fit Mode           | Which type of scale-to-fit mode is to be applied. `None` disables this section, `Uniform` applies the same rule to each individual component, while `Individual` lets you pick per-component rules.  |
| Scale to Fit *(value)*           | If `Asset` is selected as a source, this is the {% include lk id='Asset Collection' %} that will be used for staging points. |  


You can use the following rules:

|: Scale to Fit      ||
|:-------------|:------------------|
| {% include img a='placeholder.jpg' %}           | **None**<br>Disable the scaling rule. |
| {% include img a='placeholder.jpg' %}           | **Fill**<br>Scale the asset so it fills the point' bounds. |
| {% include img a='placeholder.jpg' %}           | **Min**<br>Scale the asset so it fits snuggly within the minimum point' bounds. |
| {% include img a='placeholder.jpg' %}           | **Max**<br>Scale the asset so it fits snuggly within the maximum point' bounds. |
| {% include img a='placeholder.jpg' %}           | **Average**<br>Scale the asset so it fits the average of the point' bounds.|
{: .enum }

---
## Justification

{% include img a='placeholder-wide.jpg' %}

Offset the spawned asset bounds relative to the host point' bounds.  
Justification is done & tweaked per-component.  

|: **Per component** :||
| From          | The location within the **Asset** bounds that will be justified *To* the point' bounds. <br>*i.e, from which location in the asset do i start moving.*  |
| To           | The location withn the **Point** bounds to which the **Asset** bounds will be justified. <br>*i.e, to which location in the point do i want to go.* |  
|: **Consolidated custom inputs** :||
| Custom from Vector Attribute         | An `FVector` whose individual component will be used to drive `From` properties set to `Custom`.<br>*Prefer this consolidated approach if you're using custom values on more than one component.* |
| Custom to Vector Attribute         | An `FVector` whose individual component will be used to drive `To` properties set to `Custom`. *Prefer this consolidated approach if you're using custom values on more than one component.* |

### From
You can use the following rules for `From`:

|: Justify From     ||
|:-------------|:------------------|
| {% include img a='placeholder.jpg' %}           | **Min**<br>Uses the asset bounds' min as reference point. |
| {% include img a='placeholder.jpg' %}           | **Center**<br>Uses the asset bounds' local center as reference point. |
| {% include img a='placeholder.jpg' %}           | **Max**<br>Uses the asset bounds' max as reference point.|
| {% include img a='placeholder.jpg' %}           | **Pivot**<br>Uses the asset pivot as reference point, ignoring bounds. |
| {% include img a='placeholder.jpg' %}           | **Custom**<br>Uses a lerped reference point between the asset bounds' min & max.<br>*Value is expected to be in the range 0-1 but isn't clamped.* |
{: .enum }

### To
You can use the following rules for `To`:

|: Justify To     ||
|:-------------|:------------------|
| {% include img a='placeholder.jpg' %}           | **Same**<br>Auto-selects the same justification as `From`, but computed against the point' bounds. |
| {% include img a='placeholder.jpg' %}           | **Min**<br>Uses the point bounds' min as reference point. |
| {% include img a='placeholder.jpg' %}           | **Center**<br>Uses the point bounds' local center as reference point. |
| {% include img a='placeholder.jpg' %}           | **Max**<br>Uses the point bounds' max as reference point. |
| {% include img a='placeholder.jpg' %}           | **Pivot**<br>Uses the point bounds' pivot, ignoring bounds. |
| {% include img a='placeholder.jpg' %}           | **Custom**<br>Uses a lerped reference point between the asset bounds' min & max.<br>*Value is expected to be in the range 0-1 but isn't clamped.* |
{: .enum }

---
## Variations

When to apply the asset' variations, if any, as defined in the Source.

> At the time of writing, this is not supported for `Attribute Set` source.

---
## Distribution Settings

Distribution drives how assets are selected within the collection & assigned to points.

|: Seed components      ||
| Seed components lets you choose which seed source you want to combined to drive randomness.   ||
| None           | Will only use the point' seed. |
| Local           | The local user-set seed *see property below* |
| Settings           | This node' Settings seed. |
| Component           | The parent PCG component' seed. |
{: .enum }

### Distribution

|: Distribution     ||
| Index           | Index-based selection within the collection.<br>*Enable a lot of additional options.*|
| Random           | Plain old random selection. |
| Weighted Random           | Weighted random selection, using entries' `Weight` property. |
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
## Output

Lets you output the Weight of the selection to each node, using different post-processing methods.  
**This can be very handy to identify "rare" spawns and preserve them during self-pruning operations.**

---
# Modules

## Available {% include lk id='Asset Collection' %}s modules
<br>
{% include card_any tagged="assetcollection" %}