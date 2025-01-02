---
layout: page
family: Filter
#grand_parent: Misc
parent: Filter Ecosystem
title: 🝖 Mean Value
name_in_editor: "Filter : Mean"
subtitle: The **Mean Value Filter** compares per-point values of an attribute against the mean statistical value of that same attribute.
color: param
summary: The **Mean Value Filter** compares per-point values of an attribute against its mean statistical value, allowing you to exclude points above or below the mean using various calculation methods such as Average, Median, Mode, Central, or Fixed, with options for relative or absolute measures.
splash: icons/icon_misc-sort-points.svg
tagged: 
    - node
    - filter
    - pointfilter
    - misc
nav_order: 30
outputs:
    -   name : Filter
        extra_icon: OUT_Filter
        desc : A single filter definition
        pin : params
---

{% include header_card_node %}

The **Mean Filter** node is useful to filter out statistical aberrations.
{: .fs-5 .fw-400 } 

{% include img a='details/filter-ecosystem/filter-compare-mean-lead.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|:**Settings**||
| Measure           | The "unit" system to use for measuring data.<br>*See [Measure](#measure).*  |
| Mean Method           | The method used to compute the **mean value** for a given attribute.<br>**The mean is what will be used as a reference values by the filter.**<br>*See [Mean Methods](#mean-methods).* |
| Mode Tolerance           | Value used for equality tolerance.<br>*Only available when using `Mode (Shortest)` or `Mode (Longest)` mean methods.* |
|**Pruning**||
| Exclude Below           | If enabled, points with a value **below the mean from the specified threshold** will be excluded (fail). |
| Exclude Above           | If enabled, points with a value **above the mean from the specified threshold** will be excluded (fail). |

>Keep in mind that calculations are **relative to the mean value.**
{: .infos-hl }
#### Example
{: .no_toc }  
If using `Relative` measure and `Average` mean method, the mean value will be the *average of all values*.  
The `Exclude Below` value is substracted from that average to find the minimum acceptable value (in relative terms), while the `Exclude Above` value is added to that average to find the maximum acceptable value.  

> In other words, if the average relative mean is `0.5`, using `Exclude Below = 0.1` and `Exclude Above = 0.1`, values `< 0.4` and `> 0.6` will be excluded.  

### Measure

| Method       | Description          |
|:-------------|:------------------|
| **Relative**           | When using `Relative` measure, values are pre-processed in order to find the min/max, in order for the other calculation to work from **normalized** values.<br>This is a bit clumsy to use, but also highly scalable if you're doing sub-graph with no control over the input scale/dimensions. |
| **Absolute**           | This measure uses the **raw** values.<br>It's very straighforward to use, but for obvious reason scales poorly; or through PCG overrides by forwarding params from outside. |

---
## Mean Methods
The mean method is used to find the reference threshold value that will be used by `Exclude Below` and `Exclude Above`.  
Below are an explanation on how each method works.  

For the example, let's say we're working on a dataset with 10 points that have a `float` attribute with the following values:
```cpp
Absolute values = {10, 20, 30, 40, 50, 50, 55, 60, 1200, 500}
Relative values = {0.0083, 0.016, 0.025, 0.03, 0.041, 0.041, 0.045, 0.05, 1, 0.41}
```

- [Average](#average-arithmetic-mean)
- [Median](#median)
- [Mode](#mode-shortest-or-longest)
- [Central](#central)
- [Fixed](#fixed)

### Average (Arithmetic mean)
Average mean is, well, the averaged value of all the values.  

{% include img a='details/filter-ecosystem/filter-compare-mean-average.png' %}

```cpp
Absolute Average Mean = 201.5
Relative Average Mean = 0.125
```

### Median
Median uses the median value of all the available lengths, sorted.  

{% include img a='details/filter-ecosystem/filter-compare-mean-median.png' %}

```cpp
Sorted Absolute values = [10, 20, 30, 40, 50, 50, 55, 60, 500, 1200]
Absolute Median Mean = 50 // [.., 50, 50, ...]

Sorted Relative values = [0.0083, 0.016, 0.025, 0.03, 0.041, 0.041, 0.045, 0.05, 0.41, 1]
Relative Median Mean = 0.041 // [.., 0.041, 0.041, ...]
```

### Mode (Shortest or Longest)
See [Mode (statitics) on Wikipedia](https://en.wikipedia.org/wiki/Mode_(statistics)).

If there are concurrent mode values (multiple buckets containing the same amount of values), the selected mode variant allows you to select either the mode with the *smallest* values, or the one with the *largest* out of the available conflicting modes.

{% include img a='details/filter-ecosystem/filter-compare-mean-mode.png' %}

```cpp
Absolute Mode Mean = 50 // [50, 50] is the largest bucket of equal values
Absolute Mode Mean = 0.041 // [0.041, 0.041] is the largest bucket of equal values
```
*In this is scenario, there is no conflicting frequencies.*

>The `Mode Tolerance` property is used to fill frequency buckets based on equality with already sampled values.

### Central
Central uses the middle value between the `smallest` and `largest` values as mean.

{% include img a='details/filter-ecosystem/filter-compare-mean-central.png' %}

```cpp
Absolute Central Mean = 605 // 10 + (1200-10)/2
Relative Central Mean = 0.504 // 0.0083 + (1-0.0083)/2
```

### Fixed
Fixed mean is basically **user-defined** mean.  
This is the way to go if you don't care about statistics or if you have a consistent, metrics-driven setup.  

> This is the least flexible but most performant approach as there is no need to compute any statistics prior to pruning.

{% include img a='details/filter-ecosystem/filter-compare-mean-fixed.png' %}

---
## Comparison modes
<br>
{% include embed id='settings-compare-numeric' %}