---
title: settings-projection
has_children: false
nav_exclude: true
---

| Property       | Description          |
|:-------------|:------------------|
| Enable Self Intersection  | If enabled, a cluster will test if intersection exists against itself. Otherwise, only check against other clusters.  |

| **Fuse Details** ||
| Source Distance           | Source Distance reference.<br>*Whether to consider point bounds, and if so, how.*  |
| Component Wise Tolerance  | If enabled, lets you set individual tolerance in world space for each `X`, `Y` and `Z` axis.  |
| Tolerance                 | Uniform tolerance. This represent the radius within which elements will be considered in fuse range.  |
| Tolerances                | If enabled, represent individual axis' radius within which elements will be considered in fuse range.  |
| Local Tolerance           | If enabled, lets your use per-point tolerance value.<br>**NOT IMPLEMENTED**  |

| **Outputs** ||
| Snap on Edge        | If enabled, snap the intersection position onto the original edge, as opposed to the reverse. |
| Intersector Attribute Name<br>`bool`    | If enabled, flag the points that intersected with an edge. |
