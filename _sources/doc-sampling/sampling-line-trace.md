---
layout: page
#grand_parent: All Nodes
parent: Sampling
title: Line Trace
subtitle: Sample environment using line casting
color: white
summary: The **Line Trace** node performs a single line trace for each point, using a local attribute or property as direction & size.
splash: icons/icon_sampling-guided.svg
warning: This node works with collisions and as such can be very expensive on large datasets.
tagged: 
    - node
    - sampling
nav_order: 2
inputs:
    -   name : In
        desc : Points that will be used as origin for line tracing
        pin : points
outputs:
    -   name : Out
        desc : In with extra attributes and properties
        pin : points
---

{% include header_card_node %}

# Properties
<br>

> Each output property is written individually for each point.
{: .comment }

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Direction     | Select which property or attribute should be used as direction for the line trace. |
| Max Distance     | Maximum trace distance. |
| Local Max Distance     | If enabled, will use a local property or attribute as `Max Distance`. |

|**Outputs**||
| **Success** Attribute Name     | Writes a boolean attribute to each point specifying whether the sampling has been successful (`true`) or not (`false`).<br>*Sampling is considered unsuccessful if there was no point within the specified range.* |
| **Location** Attribute Name     | Writes the sampled location, as an `FVector`. |
| **Look at** Attribute Name     | Writes the direction from the point to the sampled location, as an `FVector`. |
| **Normal** Attribute Name     | Writes the normal of the surface at the sampled, as an `FVector`. |
| **Distance** Attribute Name     | Writes the distance between the point and the sampled location, as a `double`. |

---
## Collision Settings
<br>
### Collision Settings
{% include embed id='settings-collisions' %}