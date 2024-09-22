---
layout: page
family: Probe
grand_parent: Clusters
parent: Connect Points
title: 🝆 Direction
name_in_editor: "Probe : Direction"
subtitle: Connects to the closest point in a given direction
summary: The **Direction** probe ...
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

This probes creates a single connections to another point in a specific direction, within an angular tolerance.
{: .fs-5 .fw-400 } 

{% include img a='details/connect-points/probe-direction-lead.png' %}

# Properties

| Property       | Description          |
|:-------------|:------------------|
|: **Angle settings** :|
| Use Component Wise Angle          | If enabled, the angle is checks `Yaw`, `roll` and `pitch` individually, as opposed to a cone.<br>*For obvious reasons, using component-wise check is more expensive.* |
| Max Angle           | Fixed maximum angle. |
| Max Angles           | Per-component maximum angle values. |

|: **Direction settings** :|
| Direction Source           | The type of value used for this probe' direction; either a `Constant` value or fetched from an`Attribute` |
| Direction <br>*(Constant or Attribute)*           | Direction of the probe, in world space. |
| Transform Direction           | If enabled, the probe direction will be transformed by the probed point' transform.<br>*This is effectively turning world space direction to point-local space ones.* |

|: **Other Settings** :|
| Favor           | Which metric should be favored when checking direction.<br>*See more below* |
| Do Chained Processing           | *Ignore this, it's part of a greater, very situational optimization trick that needs its own doc.* |

|: **Search Radius** :|
| Search Radius Source           | The type of value used for this probe' search radius; either a `Constant` value or fetched from an`Attribute` |
| Search Radius <br>*(Constant or Attribute)*           | Radius of the probe.<br>*Dynamic radiuses can be super expensive if they are different for each probe: search will use the greatest radius to sample to octree for this point.* |

---
## Favor distance vs alignment

|: Favor     ||
|:-------------|:------------------|
| {% include img a='placeholder.jpg' %}           | **Closest Position**<br>Favors closer points over better angular alignment. |
| {% include img a='placeholder.jpg' %}           | **Best Alignment **<br>Favors better aligned points over closer ones. |

## Angles

{% include img a='placeholder-wide.jpg' %}

Angles are checked using a Dot Product operation, meaning the angle in degree is `unsigned`, and goes both sides of the direction.  
**This means you may find youself wanting to use halved metrics -- i.e `45deg` to get an absolute `90deg` cone.**