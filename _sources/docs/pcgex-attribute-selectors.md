---
layout: page
parent: Documentation
title: Attribute Selectors
subtitle: Selecting sub-components and fields
summary: Attribute selection are not just names, but also provide additional way to select more specific data from a given attribute.
splash: icons/icon_ltr.svg
preview_img: docs/splash-attribute-selectors.png
nav_order: 2
tagged:
    - basics
#has_children: true
---

{% include header_card %}

A lot of the nodes in PCGEx allow you to select local values or attributes to further tweak and alter the way data is processed. These selectors inherit from native PCG implementation and support both point properties and attribute names as *strings*.
{: .fs-5 .fw-400 } 

{% include img a='docs/pcgex-selector.png' %} 

> In PCGEx, this is only supported when **reading** from attributes, not writing to them.
{: .error}

---
## Component selection

PCG natively support suffixing properties & attribute with selectors, such as `.X`, `.Y`, `.Z`; as shown in the debug view. This means you can safely use `$Position.Z` inside an attribute selector in order to select the `Z` value of the `Position` vector.

## Extra Selectors
PCGEx expand a tiny bit on that and support additional properties, depending on the underlying data type:

> Note that these extra selectors are not case sensitive, and can be used additively: `$Transform.Backward.Length` is a valid selectors.
{: .infos }

| Selector       | Data          |
|:-------------|:------------------|
|: **Vectors** :||
| {% include shortcut keys="X" %}           | Uses the X component (`Vector2D`, `Vector`, `Vector4`) |
| {% include shortcut keys="Y" %}           | Uses the Y component (`Vector2D`, `Vector`, `Vector4`) |
| {% include shortcut keys="Z" %}           | Uses the Z component (`Vector`, `Vector4`), fallbacks to `Y`. |
| {% include shortcut keys="W" %}           | Uses the W component (`Vector4`), fallbacks to `Z` |
| {% include shortcut keys="L" %}, {% include shortcut keys="Len" %}, {% include shortcut keys="Length" %}           | Uses the length of the vector |

|: **Color** :||
| {% include shortcut keys="R" %}           | Uses the Red value |
| {% include shortcut keys="G" %}           | Uses the Green value |
| {% include shortcut keys="B" %}           | Uses the Blue value |
| {% include shortcut keys="A" %}           | Uses the Alpha value |

|: **Rotators** :||
| {% include shortcut keys="R" %}, {% include shortcut keys="RX" %}, {% include shortcut keys="Roll" %}          | Uses the Roll component |
| {% include shortcut keys="Y" %}, {% include shortcut keys="RY" %}, {% include shortcut keys="Yaw" %}          | Uses the Yaw component |
| {% include shortcut keys="P" %}, {% include shortcut keys="RZ" %}, {% include shortcut keys="Pitch" %}           | Uses the Pitch component |

|: **Quaternions & Transforms** :||
| {% include shortcut keys="Forward" %}, {% include shortcut keys="Front" %}           | Uses the Forward direction vector |
| {% include shortcut keys="Backward" %}, {% include shortcut keys="Back" %}           | Uses the Backward direction vector |
| {% include shortcut keys="Right" %}           | Uses the Right direction vector |
| {% include shortcut keys="Left" %}           | Uses the Roll direction vector |
| {% include shortcut keys="Up" %}, {% include shortcut keys="Top" %}           | Uses the Up direction vector |
| {% include shortcut keys="Down" %}, {% include shortcut keys="Bottom" %}           | Uses the Down direction vector |

|: **Transforms** :||
| {% include shortcut keys="Position" %}, {% include shortcut keys="Pos" %}          | Uses the Position component (`Vector`) |
| {% include shortcut keys="Rotation" %}, {% include shortcut keys="Rot" %}, {% include shortcut keys="Orient" %}         | Uses the Rotation component (`Quaternion`) |
| {% include shortcut keys="Scale" %}           | Uses the Scale component (`Vector`) |
