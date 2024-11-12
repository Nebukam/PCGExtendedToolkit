---
title: settings-heuristics-local-weight
has_children: false
nav_exclude: true
---

|**Local Weight**||
| Use Local Weight Multiplier     | If enabled, this heuristic will be using a dynamic, per-point weight factor. |
| Local Weight Multiplier Source    | Whether to read the weight from `Vtx` or `Edges` points. |
| Local Weight Multiplier Attribute    | Attribute to read the local weight from. |

|**Roaming**||
| UVW Seed     | Bounds-relative roaming seed point |
| UVW Goal    | Bounds-relative roaming goal point |

> Roaming seed/goal points are used as fallback in contexts that are using heuristics but don't have explicit seed/goals; such as **Cluster Refine**'MST or Score-based refinements.