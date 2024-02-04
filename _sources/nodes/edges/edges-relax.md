---
layout: page
parent: Edges
grand_parent: All Nodes
title: Relax
subtitle: Relax points positions of a graph.
color: blue
summary: The **Relax Edges** nodes shifts points gradually in order to smooth position in relation to their neighbors.
splash: icons/icon_edges-relax.svg
preview_img: docs/splash-relaxing.png
toc_img: placeholder.jpg
tagged: 
    - edges
see_also: 
    - Relaxing
nav_order: 3
---

{% include header_card_node %}

{% include img a='details/details-relax.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Iterations | The number of time to additively apply the relaxing algorithm.<br>Each iteration uses the previous' iteration results. |
| Influence | Interpolate between the original position and the final, relaxed position.<br>- `1.0` means fully relaxed<br>- `0.0` means the original position is preserved.  |
| **Local Influence** | When enabled, this allows you to use a per-point influence value.<br>*This is especially useful for "pinning" specific points.*|
| Progressive Influence | When enabled, `Influence` is **applied between each iteration**, instead of once after all iterations have been processed.<br>*This is more expensive but yield more good looking results, especially with non-uniform local influence.*|
| Relaxing           | This property lets you select which kind of relaxing you want to apply to the input clusters.<br>**Specifics of the instanced module will be available under its inner Settings section.**<br>*See {% include lk id='Relaxing' %}.*  |

>Note that while the default `Influence` is clamped, the local influence **is purposefully not clamped**, allowing for undershooting or overshooting the influence' interpolation between the relaxed and original position.
{: .comment }

{% include img_link a='docs/relax/comparison.png' %} 

---
# Inputs & Outputs
See {% include lk id='Working with Graphs' %}

---
# Modules

> Note that the differences are very subtle between the available algorithms. I personally recommend going with {% include lk id='Laplacian' %}.

## Available {% include lk id='Relaxing' %} modules
<br>
{% include card_any tagged="relax" %}

---
# Inputs & Outputs
## Vtx & Edges
See {% include lk id='Working with Graphs' %}

---
# Examples
