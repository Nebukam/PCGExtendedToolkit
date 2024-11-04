---
layout: page
family: MiscAdd
#grand_parent: All Nodes
parent: Staging
title: Asset Staging
name_in_editor: Asset Staging
subtitle: Prepare points before spawning.
summary: The **Asset Staging** node lets your prepare points for spawning assets.
color: white
splash: icons/icon_component.svg
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

{% include img a='details/assets-staging/lead.png' %}

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
## Scale to Fit & Justification

*See [Fitting](/PCGExtendedToolkit/doc-general/general-fitting.html).*

---
## Variations

When to apply the asset' variations, if any, as defined in the Source.

> At the time of writing, this is not supported for `Attribute Set` source.

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
## Additional Outputs

Lets you output the Weight of the selection to each node, using different post-processing methods.  
**This can be very handy to identify "rare" spawns and preserve them during self-pruning operations.**

### Tagging/Flagging Points

{% include embed id='settings-tagging-assets' %}

---
# Modules

## Available {% include lk id='Asset Collection' %}s modules
<br>
{% include card_any tagged="assetcollection" %}