---
title: settings-col-asset
has_children: false
nav_exclude: true
---

---
### Common properties
#### Common properties are properties shared amonst all asset collection' entries
{: .no_toc }  

| Property       | Description          |
|:-------------|:------------------|
|: **Sub Collection** :||
| Sub Collection           | If `Is Sub Collection` is enabled, lets you pick an **Asset Collection** that will work like a list of alternative choices for this entry. |
| Is Sub Collection           | Whether this entry is a sub-collection or not.<br>Enabling this option will reveal the **Asset Collection** picker. |

|: **Settings** :||
| Weight       | The weight of this entry, relative to the others in the list.<br>Higher weights means higher chance of being picked, if the context is using weighted random selection. |
| Category       | A category associated with this entry. *Think of it as a unique tag.* |

> An entry `Weight` is not only used for Weighted Random selection, but is also used to sort entries in certain cases; among which `Indexed Weight` (Ascending/Descending) distribution mode of the [Asset Staging](../assets-staging.html) node.
{: .infos-hl }

---
### Variations

Optionally, you may enter a few random ranges for staging nodes to use, should you want to.  
*This can come in handy if you want to have high-level, per-asset variations.*

|: **Positions** :| *Additive* |
| Offset Min       | Minimum offset range. |
| Offset Max       | Maximum offset range. |
| Absolute offset       | Whether the offset should be applied in world space, or "local" to the point' transform. |

|: **Rotation** :| *Additive* |
| Rotation Min       | Minimum rotation offset range. |
| Rotation Max       | Maximum rotation offset range. |

|: **Scale** :| *Multiplicative* |
| Scale Min       | Minimum scale multiplier range. |
| Scale Max       | Maximum scale multiplier range. |
| Uniform Scale       | Whether the random scale multiplier should be applied per-component or uniformly (*in which case it uses `X` for all components*). |

> Note that variations are **never** applied by default by any nodes, and are considered an advanced tweak.
{: .comment }
