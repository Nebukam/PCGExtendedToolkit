---
layout: page
grand_parent: Pathfinding
parent: 🝓 Goal Pickers
title: 🝓 Goal from Attribute
subtitle: Match seed with goals picked at an attribute-specified index.
summary: The **Goal from Attribute** picker ...
color: white
splash: icons/icon_placement-center.svg
tagged: 
    - module
    - goalpicker
nav_order: 1
---

{% include header_card_node %}

# Properties
<br>

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

### Index Safety
{% include embed id='settings-index-safety' %}
