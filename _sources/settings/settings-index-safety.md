---
title: settings-index-safety
has_children: false
nav_exclude: true
---

### Index Safety
The index safety property controls how invalid/out of bounds input values are handled.

| Safety method       ||
| Ignore           | Invalid indices will be ignored and won't be processed further.  |
| Tile           | Index is tiled (*wrapped around*) the context' valid min/max range.|
| Clamp           | Index is clamped between the context' valid min/max range.|
| Yoyo           | Index bounces back and forth between the context' valid min/max range.|
{: .enum }