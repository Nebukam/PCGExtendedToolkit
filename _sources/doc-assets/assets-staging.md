---
layout: page
#grand_parent: All Nodes
parent: Staging
title: Asset Staging
subtitle: Prepare points before spawning.
summary: The **Asset Staging** node lets your prepare points for spawning assets.
color: white
splash: icons/icon_paths-orient.svg
preview_img: placeholder.jpg
toc_img: placeholder.jpg
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

{% include header_card_toc %}

The **Asset Staging** exists for many reasons, but primarily to "pre-spawn" assets from an {% include lk id='Asset Collection' %}, and modify points according to various rules, in relation to their associated asset.
{: .fs-5 .fw-400 } 

This is especially useful if you want to have pruning control on overlaps, or require very tight placement rules no matter how the assets have been authored (pivot point location etc)

---
# Properties

| Property       | Description          |
|:-------------|:------------------|
|: **Source** :||
| Source           | Which type of source to use for the asset collection.<br>Can be either `Asset` for an {% include lk id='Asset Collection' %} picker, or `Attribute Set`, which will reveal a new input pin that accepts an attribute set. |
| Asset Collection           | If `Asset` is selected as a source, this is the {% include lk id='Asset Collection' %} that will be used for staging points. |

### Attribute Set Source

When using the `Attribute Set` source, the node will create a temp, internal {% include lk id='Asset Collection' %} from the source attribute set. 

|: **Attribute Set Details** :||
| Asset Path Source Attribute           | The name of the attribute within the Attribute Set that contains the entry' **Asset Path**.<br>*`FSoftObjectPath` is preferred, but `FString` and `FName` are supported.* |
| Weight Source Attribute           | The name of the attribute within the Attribute Set that contains the entry' **Weight**.<br>*`int32` is preferred, but `float`, `double` and `int64` are supported.* |
| Category Source Attribute           | The name of the attribute within the Attribute Set that contains the entry' **Category**.<br>*`FName` is preferred, but `FString` is supported.* |

> While it's a needed option, keep in mind that using an attribute set prevents access to any asset cached data.
> **As such, all assets from the Attribute Set will be first loaded (asynchronously) in memory in order to compute their bounds**; before the node execution can properly start.

---
## Scale to Fit

Scale the spawned asset bounds in order to fit within the host point' bounds.  


| Scale to Fit Mode           | Which type of scale-to-fit mode is to be applied. `None` disables this section, `Uniform` applies the same rule to each individual component, while `Individual` lets you pick per-component rules.  |
| Scale to Fit *(value)*           | If `Asset` is selected as a source, this is the {% include lk id='Asset Collection' %} that will be used for staging points. |  


You can use the following rules:

|: Scale to Fit      ||
|:-------------|:------------------|
|: **None** :||
| IMG           | Disable the scaling rule. |
|: **Fill** :||
| IMG           | Scale the asset so it fills the point' bounds. |
|: **Min** :||
| IMG           | Scale the asset so it fits snuggly within the minimum point' bounds. |
|: **Max** :||
| IMG           | Scale the asset so it fits snuggly within the maximum point' bounds. |
|: **Average** :||
| IMG           | Scale the asset so it fits the average of the point' bounds.|
{: .enum }

---
## Justification

Offset the spawned asset bounds relative to the host point' bounds.  
Justification is done & tweaked per-component.  

|: **Per component** :||
| From          | The location within the **Asset** bounds that will be justified *To* the point' bounds.   |
| To           | The location withn the **Point** bounds to which the **Asset** bounds will be justified. |  
|: **Consolidated custom inputs** :||
| Custom from Vector Attribute         | An `FVector` whose individual component will be used to drive `From` properties set to `Custom`. |
| Custom to Vector Attribute         | An `FVector` whose individual component will be used to drive `To` properties set to `Custom`. |

### From
You can use the following rules for `From`:

|: Justify From     ||
|:-------------|:------------------|
|: **Min** :||
| IMG           | Uses the asset bounds' min as reference point. |
|: **Center** :||
| IMG           | Uses the asset bounds' local center as reference point. |
|: **Max** :||
| IMG           | Uses the asset bounds' max as reference point.|
|: **Pivot** :||
| IMG           | Uses the asset pivot as reference point, ignoring bounds. |
|: **Custom** :||
| IMG           | Uses a lerped reference point between the asset bounds' min & max.<br>*Value is expected to be in the range 0-1 but isn't clamped.* |
{: .enum }

### To
You can use the following rules for `To`:

|: Justify To     ||
|:-------------|:------------------|
|: **Same** :||
| IMG           | Auto-selects the same justification as `From`, but computed against the point' bounds. |
|: **Min** :||
| IMG           | Uses the point bounds' min as reference point. |
|: **Center** :||
| IMG           | Uses the point bounds' local center as reference point. |
|: **Max** :||
| IMG           | Uses the point bounds' max as reference point. |
|: **Pivot** :||
| IMG           | Uses the point bounds' pivot, ignoring bounds. |
|: **Custom** :||
| IMG           | Uses a lerped reference point between the asset bounds' min & max.<br>*Value is expected to be in the range 0-1 but isn't clamped.* |
{: .enum }

--
## Variations

When to apply the asset' variations, if any, as defined in the Source.

> At the time of writing, this is not supported for `Attribute Set` source.

--
## Distribution

--
## Output

