---
layout: page
grand_parent: Pathfinding
parent: 🝰 Heuristics
title: 🝰 Heuristic Attribute
subtitle: Attribute-driven heuristics
#summary: A summary of the informations & parameters available on every PCGEx node.
splash: icons/icon_component.svg
preview_img: docs/splash-heuristic-modifiers.png
nav_order: -1
#has_children: true
---

{% include header_card %}

> DOC TDB -- Heuristics underwent a thorough refactor that isn't documented yet. See the example project!
{: .warning }

Heuristics Attribute allows fine-grained and precise control over pathfinding constraints by leveraging user-defined attributes.
{: .fs-5 .fw-400 } 

>When dealing with modifiers, keep in mind that **lower values are considered better** than higher ones by the {% include lk id='Search' %} algorithms.
{: .error }

#### Checklist
{: .no_toc }
- TOC
{:toc}

---

{% include imgc a='pathfinding/heuristic-modifier-list.png' %} 

## Properties

| Property       | Description          |
|:-------------|:------------------|
| Reference Weight           | The reference weight is used internally by heuristics to "adapt" its scale against modifiers. *In other words, if the heuristic module itself was outputing a value like a modifier, this would its `Weight` property.*  |
| **Modifier List**           | A list of individual modifiers. Their values are accumulated per `Source`, and the final sum is used by the {% include lk id='Search' %} algorithm. |

---
# Individual Modifier

{% include imgc a='pathfinding/heuristic-modifier-solo.png' %} 

## Properties

| Property       | Description          |
|:-------------|:------------------|
| Enabled           | If disabled, this modifier will be ignored by the pathfinding process. *This is basically a QoL toggle to facilitate experimentation and better understand the impact of a specific modifier when they start pilling up.*  |
| Source           | Source define if the attribute value of the modifier is fetched from the `Vtx` point data, or from the `Edge` point data.<br>*See {% include lk id='Pathfinding' %}.* |
| **Weight**          | The weight of a modifier represent its final maximum value. |
| Selector          | Attribute to read modifier value from. Reads a `double`.<br>*See {% include lk id='Attribute Selectors' %}.* |
| Local Weight          | If enabled, the weight used for the modifier is fetched from a local attribute (same `Source`` as base value), allowing for per-point weight. |
| **Score Curve**         | The score curve is a key control element of the final modifier value. It allows your to fully leverage the range of the input attribute value in any way you see fit.<br>*More infos below.* |

### Weight

>Under the hood, the value of a modifier is remapped from its min/max to 0-1. That remapped value is used to sample the (Score Curve)[#score-curve] and *then* that value is multiplied by the weight. 
>**Depending on the selected Search, this value is post-processed to ensure it is absolute (non-negative)**

### Score Curve 📌
The score curve is a key control element of the final modifier value. It allows your to fully leverage the range of the input attribute value in any way you see fit.

>The curve is expected to stay normalized in the 0-1 range on the `y` axis, and is sampled in the 0-1 range on the `x` axis. **Just keep in mind the resulting sampled value will be made absolute (non-negative), and is used as a multiplier.**
{: .error }

---
## Examples

Say we set-up a point attribute modifier using `MyWeightAttribute`, which has raw values ranging from `-50` to `50`, with a **weight** of `100`.
1. Raw values will be remapped to 0-1
2. The curve will be sampled, then multiplied by the **weight**.

### A - Linear
{% include imgc a='pathfinding/score-curve-example-a.png' %}

This is the default, linear curve. It doesn't modulate weight, so the final weight will closely match the attribute range.

| Raw attribute value       | → Remapped to | → Sampled at | → Final weight |
|:-------------|:------------------|:------------------|:------------------|
| -50           | 0.0  | 0|**0**|
| -25           | 0.25 | 0.25|**25**|
| 0          | 0.5 | 0.5 |**50**|
| 25          | 0.75 | 0.75 |**75**|
| 50          | 1.0 | 1.0 |**100**|

### B - Expo
{% include imgc a='pathfinding/score-curve-example-b.png' %}

Exponential curve is very useful to emphasize higher values and "ignore" lower ones.  
*This can be especially useful when the attribute is for example the edge length, the resulting weight would strongly discourage long edges*

| Raw attribute value       | → Remapped to | → Sampled at | → Final weight |
|:-------------|:------------------|:------------------|:------------------|
| -50           | 0.0  |0.0|**0**|
| -25           | 0.25 |0.01|**1**|
| 0          | 0.5 |0.05|**5**|
| 25          | 0.75 |0.25|**25**|
| 50          | 1.0 |1.0|**100**|

### C - Inverse Expo
{% include imgc a='pathfinding/score-curve-example-c.png' %}
Inverted curve (exponential or not) curve is very useful to, well, *inverse* the weight.
*This is especially handy if you want to discourage smaller values and ignore higher ones.*

| Raw attribute value       | → Remapped to | → Sampled at | → Final weight |
|:-------------|:------------------|:------------------|:------------------|
| -50           | 0.0  |1|**100**|
| -25           | 0.25 |0.25|**25**|
| 0          | 0.5 |0.05|**5**|
| 25          | 0.75 |0.01|**1**|
| 50          | 1.0 |0|**0**|

### D - Yoyo
{% include imgc a='pathfinding/score-curve-example-d.png' %}
This is really just here to make a comparison with other more classic approaches. This resulting weight would ignore extreme values inside the range and strongly discourage anything closer to the center.

| Raw attribute value       | → Remapped to | → Sampled at | → Final weight |
|:-------------|:------------------|:------------------|:------------------|
| -50           | 0.0  |0|**0**|
| -25           | 0.25 |0.5|**50**|
| 0          | 0.5 |1|**100**|
| 25          | 0.75 |0.5|**50**|
| 50          | 1.0 |0|**0**|