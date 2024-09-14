---
layout: page
grand_parent: Clusters
parent: Connect Points
title: 🝆 Closest
name_in_editor: "Probe : Closests"
subtitle: Connects to the closest point within a given radius
summary: The **Closest** probe ...
color: white
splash: icons/icon_misc-write-index.svg
see_also:
    - Connect Points
tagged: 
    - node
    - probe
    - clusters
nav_order: 7
outputs:
    -   name : Probe
        desc : A single probe, to be used with the 'Connect Points' node
        pin : params
---

{% include header_card_node %}

This probes creates unique connections to the closest points within a radius, up to a maximum number.
{: .fs-5 .fw-400 } 

{% include img a='placeholder-wide.jpg' %}

# Properties

| Property       | Description          |
|:-------------|:------------------|
|: **Max Connections** :|
| Max Connection Source          | If enabled, the direction of the probe will be adjusted by the current probing point' transform.<br>*If disabled, the direction is in world space.* |
| Max Connection Constant           | Fixed maximum connections for every point. |
| Max Connection Attribute           | Per-point attribute value maximum. |

|: **Search Radius** :|
| Search Radius Source           | The type of value used for this probe' search radius; either a `Constant` value or fetched from an`Attribute` |
| Search Radius Constant           | Fixed radius of the probe. |
| Search Radius Attribute           | Per-point attribute value radius of the probe.<br>*Dynamic radiuses can be super expensive if they are different for each probe: search will use the greatest radius to sample to octree for this point.* |