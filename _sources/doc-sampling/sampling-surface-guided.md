---
layout: page
#grand_parent: All Nodes
parent: Sampling
title: Guided Trace
subtitle: Sample environment using line casting
color: white
summary: The **Guided Trace** node performs a single line trace for each point, using a local attribute or property as direction & size.
splash: icons/icon_sampling-guided.svg
preview_img: docs/splash-sample-guided.png
toc_img: placeholder.jpg
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

{% include img a='details/details-sample-guided-trace.png' %} 

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

|**Collision Settings**||
| Max Distance          | The maximum distance to search for a collision, from the point location in world space.<br>*In other words, the radius of the query sphere.* |
| Collision Type          | The type of collison to sample.<br>*See [Collision Types](#collision-types)*. |
| Collision Details          | Additional properties are available based on the selected `Collision Type`. |
| Ignore Self          | Ignore the Actor owning the outer PCG graph. |
| Ignored Actor Selector          | In-depth actor filtering.<br>*Uses native PCG' actor filter. See Unreal documentation.*|

## Collision Types
### Channel

|**Extra properties**||
|:-------------|:------------------|
|**Channel**||
| Collision Channel          | Project-specific. Selects a single collision channel to test against. |
|**Object Type**||
| Collision Object Type          | Same as Collision Channel, but work with a **flag**, allowing for a combination of types to test against. |
|**Profile**||
| Collision Profile Name          | Name of the collision profile to test against. |

---
# Inputs
## In
Points that will sample the environment for collision.  
Each point position in world space will be used as a center for a spherical query of the surrounding collisons. 

---
# Outputs
## Out
Same as input, with additional metadata.