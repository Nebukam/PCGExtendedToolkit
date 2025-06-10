---
description: PCGEx v0.64 Changelog
---

# v0.64

{% hint style="danger" %}
If you're upgrading from 5.x to 5.6, make sure to check[ this page](5.x-5.6-important-changes.md), as there has been important changes!
{% endhint %}

## New Nodes

#### Clusters / Pathfinding:

### [Find Cluster Hull](../../../node-library/pathfinding/contours/find-all-cells-1.md)

Find cluster hull is a very handy shortcut node to automatically find the wrapping hull of any and all clusters.

#### Sampling:

### [Sample Nearest Path](../../../node-library/sampling/nearest-spline-1.md) (WIP)

Sample nearest path is similar to Sample nearest spline, but allows to sample path point attributes and path data-level domain values.

## Filters

{% hint style="success" %}
All regular filters that relies on @Data domain are now going through the optimized code path that's internal to the [Uber Filter (Collection)](../../../node-library/filters/uber-filter-collection.md) node — in other words, it will skip per-point evaluation.
{% endhint %}

This optimization affects the following filters:

* Bitmask
* Bool Compare&#x20;
* Dot
* Modulo Compare
* Numeric Compare
* String Compare
* Random
* Within Range

Additionally, the Attribute Check filter now support additional constraints relative to domains.

## Tweaks

* Make sure to check [5.x to 5.6 Important Changes](5.x-5.6-important-changes.md)!
* [Mesh Selector Staged](../../../node-library/assets-management/asset-staging/mesh-selector-staged.md) for the `Static Mesh Spawner` is now fully implemented if you use the [Asset Staging](../../../node-library/assets-management/asset-staging/) node in Collection Map mode — _including optional time slicing!_\
  &#xNAN;_&#x57;hat this means is that the ISM descriptors setup in the mesh collections will be applied as-is, including component tags, material, etc._
* [Path Crossings](../../../node-library/paths/crossings.md) now properly detect if a crossing happens when precisely intersecting with another path' point as opposed to segment, as well as optionally flagging those specific cases. \
  &#xNAN;_&#x50;reviously that specific situation would simply be ignored and the crossing discarded._
* [Path Smoothing' Moving Average](../../../node-library/paths/smooth/smooth-moving-average.md) now support different ways to deal with out-of-bounds indices.
* Keep Corner Point on the line [B](../../../node-library/paths/bevel.md)[evel](../../../node-library/paths/bevel.md) mode is back, it was gone by mistake.  This is pretty handy to create new points on both sides of an existing one while retaining the original aspect. _Careful though, because subdivision goes both ways!_
* [Simplify Cluster](../../../node-library/clusters/simplify.md) now supports edge filters to drive keep/preserve nodes, as well as fusing collocated points within a given distance.
* Documentation wishful thinking : each node now has a button that opens the matching documentation page on gitbook!



## Bugfixes

A lot, LOT of bugfixes, small mathematical errors got fixed thorough the refactor.
