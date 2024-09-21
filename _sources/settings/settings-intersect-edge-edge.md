---
title: settings-intersect-edge-edge
has_children: false
nav_exclude: true
---

| Property       | Description          |
|:-------------|:------------------|
| Enable Self Intersection  | If enabled, a cluster will test if intersection exists against itself. Otherwise, only check against other clusters.  |

| **Tolerance** ||
| Tolerance                 | Uniform tolerance. This represent the radius within which elements will be considered in fuse range.  |
| Min Angle                 | If enabled, only considers edges to be intersecting if their angle is greater than the specified value.  |
| Max Angle                 | If enabled, only considers edges to be intersecting if their angle is smaller than the specified value.  |

| **Outputs** ||
| <span class="eout">Crossing Attribute Name</span><br>`bool`        | If enabled, flag the edges' intersection point. |
| Flag Crossing    | **NOT IMPLEMENTED**. |
