---
title: settings-cluster-output
has_children: false
nav_exclude: true
---

---
## Cluster Output Settings
*See {% include lk id='Working with Clusters' a='#graph-output-settings-' %}.*

| Property       | Description          |
|:-------------|:------------------|
| Edge Position           | If enabled, edge point' position will be the result of that value used as a lerp between its start and end `Vtx` point. |
|: **Pruning** :||
| Min Vtx Count           | If enabled, only ouputs clusters that have at least the specified number of `Vtx` points. |
| Max Vtx Count           | If enabled, only ouputs clusters that have at most the specified number of `Vtx` points. |
| Min Edge Count           | If enabled, only ouputs clusters that have at least the specified number of `Edge` points. |
| Max Edge Count           | If enabled, only ouputs clusters that have at most the specified number of `Edge` points. |
| Refresh Edge Seed           | If enabled, `Edge` points gets a fresh seed. |
| Build and Cache Cluster | If enabled, pre-build and cache cluster along with the point data.<br>**This has a slight memory cost associated to it, but can offer tremendous performance improvement.**<br>*If disabled, cluster processors that comes down the line have to rebuild clusters from point data, which is very costly as they are also tested for errors and possible disconnections in the process.* |
| Expand Clusters           | If enabled, also build & cache another layer of cache data.<br>**This can have a significant memory cost, as well as a minimal performance overhead, but can greatly improve certain specific operations down the line.** |