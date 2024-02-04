---
layout: page
parent: Edges
grand_parent: All Nodes
title: Prune by Length
subtitle: Remove edges from a graph using length data.
color: blue
summary: A very circumvoluted approach to removing edges using length-based statistics.
splash: icons/icon_edges-prune-by-length.svg
preview_img: docs/splash-edges-prune.png
toc_img: placeholder.jpg
tagged:
    - edges
nav_order: 5
---

{% include header_card_node %}

{% include img a='details/details-prune-by-length.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Measure           | The "unit" system to use for measuring edges.<br>*See [Measure](#measure).*  |
| Mean Method           | The method used to compute the **mean value** for a cluster.<br>**The mean is what will be used as a reference values to prune edges.**<br>*See [Mean Methods](#mean-methods).* |
| Mode Tolerance           | Value used for length equality tolerance.<br>*Only available when using `Mode (Shortest)` or `Mode (Longest)` mean methods.* |
|**Pruning**||
| Prune Below           | If enabled, edges with a value **below the mean from the specified threshold** will be pruned. |
| Prune Above           | If enabled, edges with a value **above the mean from the specified threshold** will be pruned. |
|**Output**||
| **Mean** Attribute Name           | When enabled, the computed mean value will be written to the specified attribute.<br>*Useful for debugging, and probably has other applications.<br>Note that the mean value is the same for all edges of a given cluster.* |

>Keep in mind that calculations are **relative to the mean value.**
{: .infos-hl }
#### Example
{: .no-toc }
If using `Relative` measure and `Average` mean method, the mean value will be the *average edge length*.  
The `Prune Below` value is substracted from that average to find the minimum acceptable length (in relative terms), while the `Prune Above` value is added to that average to find the maximum acceptable length.  

> In other words, if the average relative mean is `0.5`, using `Prune Below = 0.1` and `Prune Above = 0.1`, edges with a relative length `< 0.4` and `> 0.6` will be pruned.  
> *See [Easy pruning](#easy-pruning) if this gives you headaches.*

### Measure

| Method       | Description          |
|:-------------|:------------------|
| **Relative**           | When using `Relative` measure, edge lengths are pre-processed in order to find the min/max values, in order for the other calculation to work from **normalized** values.<br>This is a bit clumsy to use, but also highly scalable if you're doing sub-graph with no control over the input scale/dimensions. |
| **Absolute**           | This measure uses the **raw** edge length.<br>It's very straighforward to use, but for obvious reason scales poorly; or through PCG overrides by forwarding params from outside. |

---
## Mean Methods
The mean method is used to find the reference threshold value that will be used by `Prune Below` and `Prune Above`.  
Below are an explanation on how each method works.  

For the example, let's say we're working on a cluster with 10 edges that have the following lengths:
```cpp
Absolute lengths = [10, 20, 30, 40, 50, 50, 55, 60, 1200, 500]
Relative lengths = [0.0083, 0.016, 0.025, 0.03, 0.041, 0.041, 0.045, 0.05, 1, 0.41]
```

- [Average](#average-arithmetic-mean)
- [Median](#median)
- [Mode](#mode-shortest-or-longest)
- [Central](#central)
- [Fixed](#fixed)

### Average (Arithmetic mean)
Average mean is, well, the averaged value of all the lengths.  

```cpp
Absolute Average Mean = 201.5
Relative Average Mean = 0.125
```

### Median
Median uses the median value of all the available lengths, sorted.  

```cpp
Sorted Absolute lengths = [10, 20, 30, 40, 50, 50, 55, 60, 500, 1200]
Absolute Median Mean = 50 // [.., 50, 50, ...]

Sorted Relative lengths = [0.0083, 0.016, 0.025, 0.03, 0.041, 0.041, 0.045, 0.05, 0.41, 1]
Relative Median Mean = 0.041 // [.., 0.041, 0.041, ...]
```

### Mode (Shortest or Longest)
See [Mode (statitics) on Wikipedia](https://en.wikipedia.org/wiki/Mode_(statistics)).

If there are concurrent mode values (multiple buckets containing the same amount of values), the selected mode variant allows you to select either the mode with the *shortest* lengths, or the one with the *longest* out of the available conflicting modes.

```cpp
Absolute Mode Mean = 50 // [50, 50] is the largest bucket of equal values
Absolute Mode Mean = 0.041 // [0.041, 0.041] is the largest bucket of equal values
```
*In this is scenario, there is no conflicting frequencies.*

>The `Mode Tolerance` property is used to fill frequency buckets based on equality with already sampled values.

### Central
Central uses the middle value between the `shortest` and `longest` lengths as mean.

```cpp
Absolute Central Mean = 605 // 10 + (1200-10)/2
Relative Central Mean = 0.504 // 0.0083 + (1-0.0083)/2
```

### Fixed
Fixed mean is basically **user-defined** mean.  
This is the way to go if you don't care about statistics or if you have a consistent, metrics-driven setup.  

> This is the least flexible but most performant approach as there is no need to sample any statistics prior to pruning.

---
# Usage
## Easy pruning
If you just want to prune edge above or below a certain fixed length -- i.e `any edge longer than 500` or `any edge shorter than 42`, just do the following:

### Prune any edge longer than X
<br>
{% include img a='docs/prune-edges/above-500.png' %} 
<br>

Measure: `Absolute`, Mean Method: `Fixed`, Mean Value: `500`, Prune Above : `0`.
*This basically prune all edges which length is above `500 + 0`.*

### Prune any edge shorter than X
<br>
{% include img a='docs/prune-edges/below-42.png' %}
<br>

Measure: `Absolute`, Mean Method: `Fixed`, Mean Value: `42`, Prune Below : `0`.
*This basically prune all edges which length is below `42 - 0`.*

---
# Inputs & Outputs
## Vtx & Edges
See {% include lk id='Working with Graphs' %}