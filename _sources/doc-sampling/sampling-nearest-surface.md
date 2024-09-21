---
layout: page
#grand_parent: All Nodes
parent: Sampling
title: Sample Nearest Surface
name_in_editor: "Sample : Nearest Surface"
subtitle: Sample information from the nearest mesh collision
color: white
summary: The **Sample Nearest Points** node fetches all collisions within a given radius, and find the closest point on the closest collision. Works with simple colliders only.
splash: icons/icon_sampling-surface.svg
warning: This node works with collisions and as such can be very expensive on large datasets.
tagged: 
    - node
    - sampling
nav_order: 1
inputs:
    -   name : In
        desc : Points that will be used as origin for finding & sampling the nearest surface
        pin : points
    -   name : Point Filters
        desc : Points filters used to determine which points will be processed. Filtered out points will be treated as failed sampling.
        pin : params
outputs:
    -   name : Out
        desc : In with extra attributes and properties
        pin : points
---

{% include header_card_node %}

> Note that this only works with *simple* collisions -- 'use complex as simple' won't work either.
{: .warning-hl }

# Properties
<br>

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

---
## Collision Settings
<br>
{% include embed id='settings-collisions' %}

> Important note: under the hood this find the closest point on the closest collider -- this feature **is only supported for simple collider and won't work on complex ones**.
{: .error }