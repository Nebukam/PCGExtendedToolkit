---
layout: page
parent: ∷ General
title: Blending
subtitle: PCGEx' blendings overview
summary: This page centralizes documentation for Blending, allowing us to keep relevant content accessible without duplicating extensive details across individual pages; as many nodes have blending capabilities.
splash: icons/icon_view-grid.svg
nav_order: 11
tagged:
    - basics
---

{% include header_card_toc %}

There's a lot of node in PCGEx that offer blending settings -- and while *what* is blended is highly contextual, they all share the same settings. 
{: .fs-5 .fw-400 } 

Generally speaking, blending happens between at least two points, but it can sometimes be more, much more.  
When a node creates new points, the final point' properties and attributes is usually the result of all other blended points -- however, certain nodes blend point properties *into an existing point* which changes slightly the behavior of the selected blendmode.  

---
# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| Blending Filter           | How to filter which attributes are going to be blended.<br>*This only applies to attribute, not points properties*. |
| Filtered Attributes           | Based on the selected filter above, this is the list of attributes that will have to match it.<br>If the filter is inclusive (`Include`), only the listed attributes will be blended.<br>If the filter is exclusive (`Exclude`), listed attribute won't be blended, but every other will be. |
| Default Blending           | This is the default blending applied to both point' Attributes and Properties.<br>*Per-attribute & per-property blending can be different.* |
| Properties Overrides           | Lets you select per-property override modes, if you want them to be different from the selected default. |
| Attribute Overrides           | Lets you select per-attribute override modes, if you want them to be different from the selected default. |

---
## Blend modes

|: Blend Mode    ||
|:-------------|:------------------|
| {% include img a='placeholder.jpg' %}           | <span class="ebit">None</span><br>No blending is applied, keep the original value. |
| {% include img a='placeholder.jpg' %}           | <span class="ebit">Average</span><br>Average all sampled values. |
| {% include img a='placeholder.jpg' %}           | <span class="ebit">Weight</span><br>Weights based on distance to blend targets. If the results are unexpected, try 'Lerp' instead. |
| {% include img a='placeholder.jpg' %}           | <span class="ebit">Min</span><br>Component-wise MIN operation.<br>*Keeps the smallest value of all inputs, per-component.* |
| {% include img a='placeholder.jpg' %}           | <span class="ebit">Max</span><br>Component-wise MAX operation.<br>*Keeps the largest value of all inputs, per-component.* |
| {% include img a='placeholder.jpg' %}           | <span class="ebit">Copy</span><br>Copy the latest incoming value, in no particular order. |
| {% include img a='placeholder.jpg' %}           | <span class="ebit">Sum</span><br>Component-wise sum of all the inputs. |
| {% include img a='placeholder.jpg' %}           | <span class="ebit">Weighted Sum</span><br>Component-wise weighted sum of all the inputs.<br>*What qualify as weight depends on context*. |
| {% include img a='placeholder.jpg' %}           | <span class="ebit">Lerp</span><br>Uses weight as lerp. If the results are unexpected, try 'Weight' instead.<br>*Lerp only works properly for nodes that do a simple two-point blending.*. |
| {% include img a='placeholder.jpg' %}           | <span class="ebit">Substract</span><br>Opposite of `Sum`, but substract all inputs. |
| {% include img a='placeholder.jpg' %}           | <span class="ebit">Unsigned Min</span><br>Component-wise MIN operation on the **absolute** values, but preserve the value sign.<br>*Keeps the smallest value of all inputs, per-component.* |
| {% include img a='placeholder.jpg' %}           | <span class="ebit">Unsigned Max</span><br>Component-wise MAX operation on the **absolute** values, but preserve the value sign.<br>*Keeps the smallest value of all inputs, per-component.* |
| {% include img a='placeholder.jpg' %}           | <span class="ebit">Absolute Min</span><br>Component-wise MIN operation on the **absolute** values.<br>*Keeps the smallest value of all inputs, per-component.* |
| {% include img a='placeholder.jpg' %}           | <span class="ebit">Absolute Max</span><br>Component-wise MAX operation on the **absolute** values.<br>*Keeps the smallest value of all inputs, per-component.* |
| {% include img a='placeholder.jpg' %}           | <span class="ebit">Copy (Other)</span><br>Same as `Copy`, but copies the first incoming value.<br>*Mostly only useful during attribute rolling and certain specific nodes.* |
{: .enum }
