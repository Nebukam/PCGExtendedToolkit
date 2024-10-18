---
layout: page
title: Filter Ecosystem
subtitle: Combine multiple filters
summary: The **Uber Filter** node is a one-stop node for all your filtering needs.
color: red
splash: icons/icon_misc-sort-points.svg
preview_img: previews/index-filter-ecosystem.png
has_children: true
tagged: 
    - node
    - misc
nav_order: 11
---

{% include header_card_toc %}

> PCGEx comes with its own filter ecosystem, that is used by a lot of nodes to check if specific conditions or requirements are met on a node in order to process it. It also has two standalone, very powerful nodes that lets you combine any number of filters in the same spot.
{: .infos-hl }

PCGEx' filter system can be slightly slower than the regular filters but saves a LOT of the extra nodes that would be required to achieve the same results. 

---
## Filtering Nodes
<br>
{% include card_childs tagged='filter-ecosystem' %}

---
## Available Filters
<br>
{% include card_childs tagged='pointfilter' %}

---
## Cluster-specific filters
<br>
Some nodes support-cluster specific filters, leveraging intrinsic cluster properties that are not otherwise accessible. See {% include lk id='Cluster Filters' %} for more details.
{% include card_any tagged='clusterfilter' %}

---
## Optimizing 

> The biggest overhead of filtering is fetching attribute values : testing a lot of different attributes if much more expensive that a lot of different tests on the same attributes, as PCGEx caches attribute values before testing!
{: .infos-hl }

All filters come with a `Priority` property: that property is used internally to order filters **in ascending order**.  
The default filtering behavior is to exit the test loop as soon as possible, whether it has an `OR` or `AND` behavior. **Because of that, you should always order the tests that are the most likely to fail first**.  

> Not all filters are created equal, and some can be much more expensive than other to test against. Simple comparisons are cheap, but on the other end a filter like {% include lk id='🝖 Bounds' %} involves various transformations to be accurate.  
On the other end, {% include lk id='🝖 Mean Value' %} is a simple comparison but needs to first process all the available values to build statistics; hence there is no way to really compress the bulk of its cost.