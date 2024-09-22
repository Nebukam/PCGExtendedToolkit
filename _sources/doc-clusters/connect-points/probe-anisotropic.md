---
layout: page
family: Probe
grand_parent: Clusters
parent: Connect Points
title: 🝆 Anisotropic
name_in_editor: "Probe : Anisotropic"
subtitle: Find connections in 16 directions
summary: The **Anisotropic** probe ...
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
        desc : A single probe, to be used with the 'Connect Points' node
        pin : params
---

{% include header_card_node %}

An anisotropic probe is a preset to find connection in 16 different directions over the `X` & `Y` axis.  
*Anisotropic pathfinding is a popular approach to finding high-level plots, it made sense to have an hardcoded version as opposed to hand crafting 16 directional probes to the same effect*
{: .fs-5 .fw-400 } 

{% include img a='details/connect-points/probe-anisotropic-lead.png' %}

# Properties

| Property       | Description          |
|:-------------|:------------------|
|: **Settings** :|
| Transform Direction          | If enabled, the direction of the probe will be adjusted by the current probing point' transform.<br>*If disabled, the direction is in world space.* |

|: **Search Radius** :|
| Search Radius Source           | The type of value used for this probe' search radius; either a `Constant` value or fetched from an`Attribute` |
| Search Radius <br>*(Constant or Attribute)*           | Radius of the probe.<br>*Dynamic radiuses can be super expensive if they are different for each probe: search will use the greatest radius to sample to octree for this point.* |

> Contrary to {% include lk id='🝆 Direction' %}, the anisotropic probe will *always* favor alignment over distance.
> *Because of that, it often yields more 'canon' results with grid-aligned points.*
{: .comment }