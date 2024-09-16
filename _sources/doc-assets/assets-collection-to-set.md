---
layout: page
#grand_parent: All Nodes
parent: Staging
title: Asset Collection to Set
name_in_editor: Asset Collection to Set
subtitle: Converts an asset collection to an attribute set.
summary: The **Asset Collection to Set** offers a variety of options & settings to turn an Asset Collection to a more common Attribute Set.
color: white
splash: icons/icon_custom-graphs-params.svg
see_also: 
    - Asset Collection
tagged: 
    - node
    - staging
nav_order: 1
outputs:
    -   name : Attribute Set
        desc : Attribute set from asset collection
        pin : params
---

{% include header_card_node %}

The **Asset Collection to Set** exists in order to support Attribute-set based workflows and still be able to rely and re-use {% include lk id='Asset Collection' %}s in meaningful ways.
{: .fs-5 .fw-400 } 

It's a very straightfoward node that takes an Asset Collection as input and outputs an attribute set from that. Since an {% include lk id='Asset Collection' %} support the concept of nested collections, **it can also be leveraged as an interesting way to generate random pre-selections of assets!**

---
# Properties

| Property       | Description          |
|:-------------|:------------------|
| Asset Collection           | The {% include lk id='Asset Collection' %} to build an attribute set from. |
| Sub Collection Handling           | Defines how sub-collection entries are handled during the generation process *(more below)* |
| Allow Duplicates           | Whether or not to allow duplicate entries inside the output attribute set.<br>*(Same object path is considered a duplicate)* |
| Omit Invalid and Empty           | Whether or not to strip out invalid or empty entries from the output attribute set.<br>*(invalid or empty entries are null or invalid object paths)* |

### Sub Collection Handling

|: Handling     ||
|:-------------|:------------------|
| <span class="ebit">Ignore</span>           | Skips entry. |
| <span class="ebit">Expand</span>           | Recursively add sub-collection entries. |
| <span class="ebit">Random</span>           | Picks a random entry from the sub-collection, recursive until it's a non-collection entry. |
| <span class="ebit">Random Weighted</span>           | Picks a random weighted entry from the sub-collection, recursive until it's a non-collection entry. |
| <span class="ebit">First Item</span>           | Picks the first entry from the sub-collection, recursive until it's a non-collection entry. |
| <span class="ebit">Last Item</span>           | Picks the first entry from the sub-collection, recursive until it's a non-collection entry. |
{: .enum }

### Outputs
You can choose which property from the Asset Collection to build the attribute set from.

|: Output properties    ||
|:-------------|:------------------|
| Asset Path           | *FSoftbjectPath* |
| Weight           | *int32* |
| Category           | *FName* |
| Extents           | *FVector* |
| BoundsMin           | *FVector* |
| BoundsMax           | *FVector* |
| Nesting Depth           | *int32* |