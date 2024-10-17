---
title: settings-bounds-type
has_children: false
nav_exclude: true
---

|: Bounds type      ||
| <span class="ebit">Scaled Bounds</span>           | Point' bounds scaled by point' scale.<br>*These are the bounds shown by default when using the debug node in extent mode.* |
| <span class="ebit">Density Bounds</span>           | Point' bounds scaled by point' scale, expanded by point' steepness property.<br>*These are the bounds used by a lot of vanilla PCG nodes.* |
| <span class="ebit">Bounds</span>           | Raw, unscaled point' bounds. |

> *Point bounds are determined by its BoundsMin and BoundsMax properties.*
{: .comment }