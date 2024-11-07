---
layout: page
parent: ∷ General
title: Fitting
subtitle: PCGEx' fitting overview
summary: This page centralizes documentation for Fitting, allowing us to keep relevant content accessible without duplicating extensive details across individual pages; as a few nodes have fitting capabilities and it's a heavy piece of doc.
splash: icons/icon_view-grid.svg
nav_order: 12
tagged:
    - centralized
---

{% include header_card_toc %}

There's a few node in PCGEx that offer fitting settings -- and while *what* is fitted is highly contextual, they all share the same settings.  
Fitting is broken down in two parts: **Scale to Fit**, which is responsible for scaling something against a reference point' bounds & scale, and **Justification**, which is responsible for the placement of the fitted *something* against the same reference bounds.  
{: .fs-5 .fw-400 } 


---
# Scale to Fit

{% include img a='details/assets-staging/scale-to-fit.png' %}

Scale the spawned asset bounds in order to fit within the host point' bounds.  


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
# Justification

{% include img a='details/assets-staging/justification.png' %}

Offset the spawned asset bounds relative to the host point' bounds.  
<u>Justification is processed per-component, after the scale-to-fit pass.</u>

|: **Per component** :||
| From          | The location within the **Asset** bounds that will be justified *To* the point' bounds. <br>*i.e, from which location in the asset do i start moving.*  |
| To           | The location withn the **Point** bounds to which the **Asset** bounds will be justified. <br>*i.e, to which location in the point do i want to go.* |  
|: **Consolidated custom inputs** :||
| Custom from Vector Attribute         | An `FVector` whose individual component will be used to drive `From` properties set to `Custom`.<br>*Prefer this consolidated approach if you're using custom values on more than one component.* |
| Custom to Vector Attribute         | An `FVector` whose individual component will be used to drive `To` properties set to `Custom`. *Prefer this consolidated approach if you're using custom values on more than one component.* |

## From
You can use the following rules for `From`:

|: Justify From     ||
|:-------------|:------------------|
| {% include img a='details/assets-staging/enum-justify-from-center.png' %}           | <span class="ebit">Center</span><br>Uses the asset bounds' local center as reference point. |
| {% include img a='details/assets-staging/enum-justify-from-min.png' %}           | <span class="ebit">Min</span><br>Uses the asset bounds' min as reference point. |
| {% include img a='details/assets-staging/enum-justify-from-max.png' %}           | <span class="ebit">Max</span><br>Uses the asset bounds' max as reference point.|
| {% include img a='details/assets-staging/enum-justify-from-pivot.png' %}           | <span class="ebit">Pivot</span><br>Uses the asset pivot as reference point, ignoring bounds. |
| {% include img a='details/assets-staging/enum-justify-from-custom.png' %}           | <span class="ebit">Custom</span><br>Uses a lerped reference point between the asset bounds' min & max.<br>*Value is expected to be in the range 0-1 but isn't clamped.* |
{: .enum }

## To
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
