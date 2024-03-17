---
layout: page
parent: Sampling
#grand_parent: All Nodes
title: Sample Nearest Surface
subtitle: Sample information from the nearest mesh collision
color: white
summary: The **Sample Nearest Points** node fetches all collisions within a given radius, and find the closest point on the closest collision. Works with simple colliders only.
splash: icons/icon_sampling-surface.svg
preview_img: docs/splash-sample-nearest-surface.png
warning: This node works with collisions and as such can be very expensive on large datasets.
toc_img: placeholder.jpg
tagged: 
    - node
    - sampling
nav_order: 1
---

{% include header_card_node %}

{% include img a='details/details-sample-nearest-surface.png' %} 

> Each output property is written individually for each point.
{: .comment }

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Max Distance     | Maximum surface search distance. |
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
|**Channel**||
| Collision Channel          | Project-specific. Selects a single collision channel to test against. |
|**Object Type**||
| Collision Object Type          | Same as Collision Channel, but work with a **flag**, allowing for a combination of types to test against. |
|**Profile**||
| Collision Profile Name          | Name of the collision profile to test against. |

> Important note: under the hood this find the closest point on the closest collider -- this feature **is only supported for simple collider and won't work on complex ones**.
{: .error }

---
# Inputs
## In
Points that will sample the environment for collision.  
Each point position in world space will be used as a center for a spherical query of the surrounding collisons. 

---
# Outputs
## Out
Same as input, with additional metadata.