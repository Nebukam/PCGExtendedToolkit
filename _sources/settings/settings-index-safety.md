---
title: settings-index-safety
has_children: false
nav_exclude: true
---

## Index Safety
The index safety property controls how invalid indices are handled.

| Safety method       | Description          |
|:-------------|:------------------|
| Ignore           | Invalid indices will be ignored and won't be processed further.  |
| Tile           | Index is tiled (*wrapped around*) the context' valid min/max range.|
| Clamp           | Index is clamped between the context' valid min/max range.|

> Tiling index means that for a range of `[0..10]`, a value of `11` will be sanitized to `1` and a value of `-1` sanitized to `9`.
{: .warning-hl }