---
layout: page
parent: Misc
grand_parent: All Nodes
title: Attribute Remap
subtitle: Highly customizable attribute remapping.
color: white
summary: The **Attribute Remap** allows you to easily do component-wise attribute remapping and clamping.
splash: icons/icon_misc-write-index.svg
preview_img: docs/splash-remap.png
toc_img: placeholder.jpg
tagged: 
    - misc
nav_order: 7
---

{% include header_card_node %}

> Note that this node **only support attribute and not properties**, *extra selectors* will be ignored.
{: .warning }

> Additional note: this node works in a component-wise fashion: the remapping will be done individually for each component of the input data.  By default the same remapping rule is applied to each component, but you can freely override the behavior per-component.
{: .comment }

{% include img a='details/details-attribute-remap.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Source Attribute Name           | The source attribute to read data from.  |
| Target Attribute Name           | The target attribute to write data to.<br>*Can be the same as `Source`.*|

|**Remap (Default)**||
|**Input Clamp Settings**||
| Clamp Min           | If enabled, input data smaller than the specified value will be clamped. |
| Clamp Max           | If enabled, input data greater than the specified value will be clamped.  |

|**Remap Settings**||
| In Min           | If enabled, lets you input the `In Min` value manually for the remapping.<br>**If left disabled, this value will be the minimum found in the input dataset.** |
| In Max           | If enabled, lets you input the `In Max` value manually for the remapping.<br>**If left disabled, this value will be the maximum found in the input dataset.** |
| Scale           | A scale factor applied to the output, remapped value.<br>*See [Remap Curve & Scale](#remap-curve--scale).* |
| Range Method           | Basically lets you choose whether the smallest value should be `0` (Full Range) or the effective `Min` value (Effective range).<br>*See [Range Method](#range-method).* |
| Remap Curve           | The curve that will be sampled for remapping.<br>*See [Remap Curve & Scale](#remap-curve--scale).* |
|**Output Clamp Settings**| *Same as Input, but applied to the final value before writing it.*|

|**Remap overrides**||
| **Remap (2nd Component)**           | Override default settings for the 2nd component (`Y`, `Yaw`, `G`, etc) |
| **Remap (3rd Component)**           | Override default settings for the 3rd component (`Z`, `Pitch`, `B`, etc) |
| **Remap (4th Component)**           | Override default settings for the 4th component (`W`, `A`)  |


---
## Remap Curve & Scale

The way this node works is by measuring the minimum & maximum attribute value, and remap input values to a `[0..1]` range that is then used to sample the specified `Remap Curve` on the x-axis.  
The `y` curve value is then multplied by the specified `Scale`.  

See [Range Method](#range-method) as it drives how values are sampled close to zero! 

---
## Range Method

{% include img a='docs/relax/range.png' %} 

> Note that the `Effective Range` method tends to spread/scale the input set of values -- but allows one to leverage the full range of the curve no matter the min/max input values.  
> **Hence, using `Full Range` with only high (or low) input value will only sample a very narrow portion of the curve.**
{: .infos-hl }

---
# Inputs
## In
Any number of point datasets.

---
# Outputs
## Out
Same as Inputs with the added metadata.  
*Reminder that empty inputs will be ignored & pruned*.