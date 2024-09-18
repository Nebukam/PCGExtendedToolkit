---
title: settings-projection
has_children: false
nav_exclude: true
---

| Property       | Description          |
|:-------------|:------------------|
| Target Distance           | Target Distance reference..<br>*Whether to consider point bounds, and if so, how.* |
| Fuse Method               | Lets you choose the method for finding neighbors & collocated points |
| Voxel Grid Offset         | Offset the voxelized grid by an constant amount.<br>*By default the center of the grid is 0,0,0, which may look like an undersirable offset. That offset can be manually compensated using this parameter.* |
| Inline Insertion          | **Using the Octree fuse method is not deterministic by default.**<br>Enabling inlined insertion will make it so, at the cost of speed. |
| Source Distance           | Source Distance reference.<br>*Whether to consider point bounds, and if so, how.*  |

| **Tolerance** ||
| Component Wise Tolerance  | If enabled, lets you set individual tolerance in world space for each `X`, `Y` and `Z` axis.  |
| Tolerance                 | Uniform tolerance. This represent the radius within which elements will be considered in fuse range.  |
| Tolerances                | If enabled, represent individual axis' radius within which elements will be considered in fuse range.  |
| Local Tolerance           | If enabled, lets your use per-point tolerance value.<br>**NOT IMPLEMENTED**  |
