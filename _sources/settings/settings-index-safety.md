---
title: settings-index-safety
has_children: false
nav_exclude: true
---


The index safety property controls how invalid/out of bounds input values are handled.

| Safety method       ||
| <span class="ebit">Ignore</span>           | Invalid indices will be ignored and won't be processed further.  |
| <span class="ebit">Tile</span>           | Index is tiled (*wrapped around*) the context' valid min/max range.|
| <span class="ebit">Clamp</span>           | Index is clamped between the context' valid min/max range.|
| <span class="ebit">Yoyo</span>           | Index bounces back and forth between the context' valid min/max range.|
{: .enum }