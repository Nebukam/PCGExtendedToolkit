---
title: settings-compare-bitmask
has_children: false
nav_exclude: true
---

| Comparison       | Data          |
|:-------------|:------------------|
| <span class="ebit">Match (any)</span>           | Value & Mask != 0 (At least some flags in the mask are set) |
| <span class="ebit">Match (all)</span>           | Value & Mask == Mask (All the flags in the mask are set) |
| <span class="ebit">Match (strict)</span>           | Value == Mask (Flags strictly equals mask) |
| <span class="ebit">No Match (any)</span>           | Value & Mask == 0 (Flags does not contains any from mask) |
| <span class="ebit">No Match (all)</span>           | Value & Mask != Mask (Flags does not contains the mask) |
{: .enum }