---
description: PCGEx v0.64 Changelog
---

# v0.64

{% hint style="danger" %}
If you're upgrading from 5.x to 5.6, make sure to check[ this page](5.x-5.6-important-changes.md), as there has been important changes!
{% endhint %}

## New Nodes

### Clusters / Pathfinding:

#### [Find Cluster Hull](../../../node-library/pathfinding/contours/find-all-cells-1.md)

Find cluster hull is a very handy shortcut node to automatically find the wrapping hull of any and all clusters.

### Sampling:

[Sample Nearest Path](../../../node-library/sampling/nearest-spline-1.md)

Sample nearest path is similar to Sample nearest spline, but allows to sample path point attributes and path data-level domain values.

## New Filters

## Tweaks

* Make sure to check [5.x to 5.6 Important Changes](5.x-5.6-important-changes.md)!
* [Mesh Selector Staged](../../../node-library/assets-management/asset-staging/mesh-selector-staged.md) for the `Static Mesh Spawner` is now fully implemented if you use the [Asset Staging](../../../node-library/assets-management/asset-staging/) node in Collection Map mode.\
  &#xNAN;_&#x57;hat this means is that the ISM descriptors setup in the mesh collections will be applied as-is, including component tags, material, etc._
* [Path Crossings](../../../node-library/paths/crossings.md) now properly detect if a crossing happens when precisely intersecting with another path' point as opposed to segment, as well as optionally flagging those specific cases. \
  &#xNAN;_&#x50;reviously that specific situation would simply be ignored and the crossing discarded._
* [Path Smoothing' Moving Average](../../../node-library/paths/smooth/smooth-moving-average.md) now support different ways to deal with out-of-bounds indices.

Documentation shortcut wishful thinking



## Bugfixes

A lot, LOT of bugfixes, small mathematical errors got fixed thorough the refactor.
