---
layout: page
parent: Goal Pickers
grand_parent: ∷ Pathfinding
title: Goal from Attribute
subtitle: Match seed with goals picked at an attribute-specified index.
color: white
#summary: summary_goes_here
splash: icons/icon_placement-center.svg
preview_img: docs/splash-picker-attribute.png
toc_img: placeholder.jpg
tagged: 
    - goalpicker
nav_order: 1
---

{% include header_card_node %}

{% include img a='details/modules/picker-attribute.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Goal Count           | Either a `Single Goal` or `Multiple Goals` |
| Attribute           | If `Single Goal` is specified, lets you specify a property or attribute which value will be used as an index within the input goals. |
| Attributes           | If `Multiple Goal` is specified, lets you specify a list of property or attribute which values will be used as an index within the input goals. |
| Index Safety           | Failsafe method if the picked `Goal` index is out of bounds.<br>*See [Index Safety](#index-safety)* |

>When using `Multiple Goal`, each seed will attempt to create one connection per entry in the array.  
>The attribute is fetched on the `Seed` input.
{: .infos } 

{% include embed id='settings-index-safety' %}
