---
title: settings-dot-comparison
has_children: false
nav_exclude: true
---


| Property       | Description          |
|:-------------|:------------------|
| Comparison          | Which comparison to use.<br>*See [Numeric comparisons](/PCGExtendedToolkit/doc-general/comparisons.html#numeric-comparisons).* |
| Dot Units             | Let you choose to work either with normalized dot range (`-1 / 1`) or degrees.<br>*This affects how the attribute Dot value will be interpreted as well.* |
| Unsigned Dot           | When enabled, the comparison will occur against an absolute dot value.<br>*This is especially useful when testing against undirected lines.* |
| Dot Value           | The type of value used for this probe' search radius; either a `Constant` value or fetched from an`Attribute` |
| *(Dot or Degrees)* Constant  | Constant to compare against. |
| *(Dot or Degrees)* Attribute | Constant to compare against. |
| *(Dot or Degrees)* Tolerance  | Comparison tolerance, if the selected comparison is an approximative (`~`) one. |
