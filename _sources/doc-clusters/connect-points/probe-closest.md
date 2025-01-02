---
layout: page
family: Probe
grand_parent: Clusters
parent: Connect Points
title: 🝆 Closest
name_in_editor: "Probe : Closests"
subtitle: Connects to the closest point within a given radius
summary: The **Closest** probe ...
color: param
splash: icons/icon_path-to-edges.svg
see_also:
    - Connect Points
tagged: 
    - node
    - probe
    - clusters
nav_order: 7
outputs:
    -   name : Probe
        extra_icon: OUT_Probe
        desc : A single probe, to be used with the 'Connect Points' node
        pin : params
---

{% include header_card_node %}

This probes creates unique connections to the closest points within a radius, up to a maximum number.
{: .fs-5 .fw-400 } 

{% include img a='details/connect-points/probe-closest-lead.png' %}

# Properties

| Property       | Description          |
|:-------------|:------------------|
|: **Max Connections** :|
| Max Connection Source          | If enabled, the direction of the probe will be adjusted by the current probing point' transform.<br>*If disabled, the direction is in world space.* |
| Max Connection Constant           | Fixed maximum connections for every point. |
| Max Connection Attribute           | Per-point attribute value maximum. |

|: **Coincidence** :|
| Coincidence Prevention Tolerance          | If enabled, prevents multiple connections from happening in the same direction, within that tolerance.<br>*This avoids the creation of overlapping edges when testing in near-collinear situations.* |

> Note that connections skipped due to coincidence don't count toward the maximum.

|: **Search Radius** :|
| Search Radius Source           | The type of value used for this probe' search radius; either a `Constant` value or fetched from an`Attribute` |
| Search Radius <br>*(Constant or Attribute)*           | Radius of the probe.<br>*Dynamic radiuses can be super expensive if they are different for each probe: search will use the greatest radius to sample to octree for this point.* |