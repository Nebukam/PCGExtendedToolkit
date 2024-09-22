---
layout: page
parent: ∷ General
title: Plugin Settings
subtitle: PCGEx' Project Settings
summary: An overview of the plugin global settings that can be tweaked through the project settings panel.
splash: icons/icon_view-grid.svg
nav_order: 1
tagged:
    - basics
---

{% include header_card %}

Plugin settings are used globally by all nodes. *At the time of writing, it is not possible to do per-platform override, althought this will be a welcome addition in the future.*
{: .fs-5 .fw-400 } 

# Performance
<br>
Performance-related settings.

---
## Cluster
<br>

| Property       | Description          |
|:-------------|:------------------|
| Small Cluster Size          | How many points should be considered a "small cluster". Small clusters are batched together during processing, as opposed to have a dedicated thread. |
| Cluster Default Batch Chunk Size          | How many cluster points should be processed in a single parallel loop chunk.  |
| Cache Cluster          | If enabled, PCGEx will cache built cluster data. This offers tremendous performance gains at the expense of a bit of extra memory. |
| Default Build and Cache Clusters          | Default value for `Build and Cache Clusters` when creating a new cluster node.<br>*Can be overriden on a per-node basis.*  |
| Default Cache Expanded Clusters          | Default value for `Cache Expanded Clusters` when creating a new cluster node.<br>*Can be overriden on a per-node basis.* |

> Smaller values will lead to more thread hoarding, larger values will use less space at the expense of speed.
{: .infos }

---
## Points
<br>

| Property       | Description          |
|:-------------|:------------------|
| Small Points Size          | How many points should be considered a "small number of points". Small datasets are batched together during processing, as opposed to have a dedicated thread. |
| Points Default Batch Chunk Size          | How many points should be processed in a single parallel loop chunk.  |
| Cache Cluster          | If enabled, PCGEx will cache built cluster data. This offers tremendous performance gains at the expense of a bit of extra memory. |
| Default Build and Cache Clusters          | Default value for `Build and Cache Clusters` when creating a new PCGEx node.<br>*Can be overriden on a per-node basis.*  |
| Default Cache Expanded Clusters          | Default value for `Cache Expanded Clusters` when creating a new PCGEx node.<br>*Can be overriden on a per-node basis.* |

> Smaller values will lead to more thread hoarding, larger values will use less space at the expense of speed.
{: .infos }

---
## Async
<br>

| Property       | Description          |
|:-------------|:------------------|
| Default Work Priority          | Default falut for thread `Work Priority` when creating new PCGEx node.<br>*Can be overriden on a per-node basis.* |

> Work priority should not be changed trivially -- but rather leverage that setting in complex graphs where you have a lot of PCGEx nodes blocking work at the same level; *putting a high priority on the worst offenders will give you a teeny tiny bit of control over which part of the graph are processed first, resulting in micro performance improvement and overall smaller wall times.*

---
# Blending

---
## Attribute Types Defaults
<br>
Default blending values for attributes based on attribute type. These are mostly used with node that allow to batch-blend `All` attributes; which can anyway be finely controlled. Using `Default` will use the node' default blending mode.

| Property       | Description          |
|:-------------|:------------------|
| **Simple Types** ||
| `Boolean`          |  |
| `Float`          |  |
| `Double`          |  |
| `Integer 32`          |  |
| `Integer 64`          |  |

| **Vector Types** ||
| `Vector2`          |  |
| `Vector`          |  |
| `Vector4`          |  |

| **"Complex" Types** ||
| `Quaternion`          |  |
| `Transform`          |  |
| `Rotator`          |  |

| **"Text" Types** ||
| `String`          |  |
| `Name`          |  |

| **Soft Path Types** ||
| `SoftObjectPath` |  |
| `SoftClassPath` |  |

---
# Node Colors
<br>
Customize PCGEx nodes colors by categories. *These may require a relaunch for the colors to be visibly applied, sometimes just closing/re-opening the graph editor works.*

| Property       | Description          |
|:-------------|:------------------|
| Node Color Debug          | Default Hex Linear : `FF0000FF` |
| Node Color Misc          | Default Hex Linear : `FF9748FF` |
| Node Color Misc Write         | Default Hex Linear : `FF5100FF` |
| Node Color Misc Add         | Default Hex Linear : `00FF4CFF` |
| Node Color Misc Remove         | Default Hex Linear : `0D0303FF` |
| Node Color Sampler         | Default Hex Linear : `FF0026FF` |
| Node Color Sampler Neighbor        | Default Hex Linear : `720011FF` |
| Node Color Cluster Gen        | Default Hex Linear : `0051FFFF` |
| Node Color Cluster        | Default Hex Linear : `009DFFFF` |
| Node Color Probe        | Default Hex Linear : `2CAEFFFF` |
| Node Color Socket State        | Default Hex Linear : `004068FF` |
| Node Color Pathfinding        | Default Hex Linear : `00FFABFF` |
| Node Color Heuristics        | Default Hex Linear : `3E935FFF` |
| Node Color Heuristics Att        | Default Hex Linear : `7F833FFF` |
| Node Color Cluster Filter        | Default Hex Linear : `5ABEA5FF` |
| Node Color Edge        | Default Hex Linear : `00ABC2FF` |
| Node Color Cluster State        | Default Hex Linear : `004068FF` |
| Node Color Path        | Default Hex Linear : `003D29FF` |
| Node Color Filter Hub        | Default Hex Linear : `3AFF00FF` |
| Node Color Filter        | Default Hex Linear : `50BE2FFF` |
| Node Color Primitives        | Default Hex Linear : `0011FFFF` |
| Node Color Transform        | Default Hex Linear : `FF002FFF` |

> Note that there's a MASSIVE DISCREPANCY between the color you pick and how the node renders it, so starting to tweak those is one hell of a rabbit hole.
> To do live color tests, you can drop a {% include lk id='Flush Debug' %} node anywhere in a graph, it has an exposed `Custom Color` property to let you set its own custom color -- comes in very handy!
{: .infos }