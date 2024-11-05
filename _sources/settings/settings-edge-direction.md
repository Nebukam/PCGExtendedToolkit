---
title: settings-edge-direction
has_children: false
nav_exclude: true
---

| Property       | Description          |
|:-------------|:------------------|
| Direction Method     |  Defines which endpoints "order" will be used to define the direction reference for the ouputs. |
| Direction Choice | Further refines the direction method, based on the above selection.<br>-`Smallest to Greatest` will order direction reference metrics in ascending order.<br>-`Greatest to Smallest` will order direction reference metrics in descending order.<br>*Really it's how the endpoint reference value is sorted, but I couldn't call that Direction' direction.*|
| Dir Source Attribute     |  The attribute that will be used by the selected method. |

> If the selected method is `Edge Dot Attribute`, the attribute will be read from `Edges` as an `FVector` direction.

### Method

The `Direction method`, combined with the `Direction Choice` determine which endpoint should be considered the `Start` & `End` of the edge. **The "direction" of the edge used for computing outputs & properties is the safe normal going from the start to the end of the edge.**

| Mode       | Description          |
|:-------------|:------------------|
|<span class="ebit">Endpoints order</span>    | Will use the endpoints' as ordered during cluster construction. |
|<span class="ebit">Endpoints indices</span> | Will use the endpoints' point index. |
|<span class="ebit">Endpoints Sort</span> | Enables the use of any number of [Sorting Rules](/PCGExtendedToolkit/doc-misc/sort-points/sort-rule.html) to determine direction.<br>*This method, while more expensive, is the most efficient.* |
| <span class="ebit">Edge Dot Attribute</span> | Will use an attribute (converted to an `FVector`) from the edges' and do a Dot Product with the edge' direction.<br>*This method, combined with `Direction Choice` offers full control over direction.* |
{: .enum }
