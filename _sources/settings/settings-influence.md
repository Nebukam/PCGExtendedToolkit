---
title: settings-blending
has_children: false
nav_exclude: true
---


| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Influence | Interpolate between the original position and the final, relaxed position.<br>- `1.0` means fully relaxed<br>- `0.0` means the original position is preserved.  |
| **Local Influence** | When enabled, this allows you to use a per-point influence value.<br>*This is especially useful for "pinning" specific points.*|
| Progressive Influence | When enabled, `Influence` is **applied between each iteration**, instead of once after all iterations have been processed.<br>*This is more expensive but yield more good looking results, especially with non-uniform local influence.*|

>Note that while the default `Influence` is clamped, the local influence **is purposefully not clamped**, allowing for undershooting or overshooting the influence' interpolation between the relaxed and original position. Use carefully.
{: .comment }