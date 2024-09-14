---
layout: page
grand_parent: Clusters
parent: Flag Nodes
title: Node Flag
name_in_editor: "Cluster : Node Flag"
subtitle: A single flag definition
summary: The **Node Flag** uses a variety of filters to perform bitmask-based flag operations that can be set in three modes; Direct, Individual, or Composite, with various logical operations like AND, OR, NOT, and XOR.
color: white
splash: icons/icon_misc-write-index.svg
preview_img: placeholder.jpg
see_also:
    - Working with Clusters
    - Flag Nodes
    - Uber Filter
tagged: 
    - node
    - clusters
    - flagger
nav_order: 0
inputs:
    -   name : Filters 
        desc : Any number of filters. Supports both regular point filters as well as special cluster filters.
        pin : params
outputs:
    -   name : Flag
        desc : A single flag definition used along with a Flag Node node.
        pin : params
    -   name : Pass Bitmask
        desc : (Advanced output) - Outputs the Pass State Flags as a re-usable bitmask.
        pin : params
    -   name : Fail Bitmask
        desc : (Advanced output) - Outputs the Fail State Flags as a re-usable bitmask.
        pin : params
---

{% include header_card_node %}

The **Node Flag**, *not to be confused with  {% include lk id='Flag Nodes' %}*, performs a **single per-point bitmask-based operation** on an incoming value; based on any number of input filters.
{: .fs-5 .fw-400 } 

{% include img a='placeholder-wide.jpg' %}

# Properties

| Property       | Description          |
|:-------------|:------------------|
|: **Settings** :|
| Name          | If enabled, the direction of the probe will be adjusted by the current probing point' transform.<br>*If disabled, the direction is in world space.* |
| Priority           | Fixed maximum connections for every point. |

| Pass State Flags           | If enabled, will apply the specified flags to the incoming bitmask **when the filters are true.**<br>*See below for more infos on flag operation.* |
| Fail State Flags           | If enabled, will apply the specified flags to the incoming bitmask **when the filters return false.**<br>*See below for more infos on flag operation.* |

---
## State Flags

| Property       | Description          |
|:-------------|:------------------|
|: **Settings** :|
| Mode          | The mode lets you choose how to set the flags value, as a user.<br>- `Direct` is probably the most useful, as it can be set using an override pin.<br>- `Individual` lets you use an array where you can set individual bits by their position (index), and whether they're true or false.<br>- `Composite` lets you set individual bits using dropdowns. |


### Mode : Direct

This is probably the most useful & straightforward mode, as **it can be set using an override pin**.

| Bitmask           | Sets the raw bitmask value, as an `int64`. |
| Op           | Which operation to do on the selected bit using the `value` set below.<br>Can be either:`=`, `AND`, `OR`, `NOT`, or `XOR`.<br>*See below for more infos.* |

### Mode : Individual

Individual mode lets you cherry pick which operation you want to perform on specific bits, using an array.  
*Note that the order of the array itself is of no consequence; the `Bit Index` property is what matters.*

|: **Individual Operation** :|
| Bit Index          | The index of the bit this operation will be applied to.<br>Keep the ranges between `0` and `63`|
| Op           | Which operation to do on the selected bit using the `value` set below.<br>Can be either:`=`, `AND`, `OR`, `NOT`, or `XOR`.<br>*See below for more infos.* |
| Value           | If `Individual` is selected, this an array where each bit is an index/operation pair. |

### Mode : Composite

This lets you set the full width of the `int64` using a single dropdown per byte.  

| Op           | Which operation to do on the incoming flags using the bitmask created from the dropdowns.<br>Can be either:`=`, `AND`, `OR`, `NOT`, or `XOR`.<br>*See below for more infos.* |
| 0-64 Bits           | If `Composite` is selected, a whole bunch of dropdowns will show up. |

## Operation

|: **Op** :|
| `=`          | Set the bit value, with no regard for its current value. |
| `AND`           | TBD |
| `OR`           | TBD |
| `NOT`           | TBD |
| `XOR`           | TBD |

---
## Available Cluster Filters
<br>
{% include card_any tagged="clusterfilter" %}

---
## Available Filters
<br>
{% include card_any tagged="filter" %}