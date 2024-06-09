---
title: settings-weighting
has_children: false
nav_exclude: true
---

## Weighting

There is two weighting method available. `Full Range` and `Effective Range`.
Each method outputs a `[0..1]` value that will be used to sample the `Weight Over Distance` curve.  
**However, there is a critical nuance between the two:**
- `Full Range` is a simple normalization, each target distance is divided by the longest one. *As such, it's very unlikely the curve will get sampled close to `x=0`.*
- `Effective Range` **remaps** each target distance using the shortest & longest distance as min/max. *As such, the shortest sampled distance will sample the curve at `x=0`, and the longest at `x=1`.*

> Important note: when using the `Within range` sample method, some outputs will use the final weighted position/transforms for their calculations; although mathematically correct, **this may yield unusuable/innacurate results**.
{: .error }