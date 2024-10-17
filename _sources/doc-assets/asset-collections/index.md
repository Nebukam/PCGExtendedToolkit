---
layout: page
#grand_parent: All Nodes
parent: Staging
title: Asset Collection
subtitle: Glorified Data Tables
summary: Asset Collection represent collections of things (meshes, actors) that can be used with the **Asset Staging** node.
color: red
splash: icons/icon_custom-graphs-params.svg
has_children: true
use_child_thumbnails: true
nav_order: 2
tagged:
    - module
---

{% include header_card_toc %}

Asset Collections are basically lists of assets that you can then use inside PCG; that all share some basic properties. 
They exist in different flavors, for different purposes (i.e actors vs meshes), but their main appeal over *Data Table* or *PCG Data Asset* is **caching** and **nesting**.
{: .fs-5 .fw-400 } 

---
### Cached bounds data
{: .no_toc }
Each entry has some basic information cached, amongst which the bounds of the referenced asset.  
This is especially useful as it allows nodes to access an asset bounds' **without having to load the asset in memory**.  
It enables nodes like {% include lk id='Asset Staging' %} to do some interesting work on points before spawning anything in the world.

---
### Nested collections
{: .no_toc }
**Each entry can be another collection.** This is where the Asset Collection shines, because a collection can be weighted like any other entry. PCGEx will recognize sub-collection and will keep on digging in these until an asset is found.  
This is particulary handy when doing random weighted selection, where you want only a *subset* of items to be associated to a particular weight range, and then have a new random weighted pick from inside that range. *Collections can be nested without limits, offering highly granular control over weighted & random distribution.*

---
### Re-usability & templating
{: .no_toc }
Since they're regular DataAssets, Asset Collections can easily be extended, re-used, shared amongst different settings & PCG setups. 

---
### Convertible
{: .no_toc }
In order to support any workflow, {% include lk id='Asset Collection to Set' %} makes it easy to convert any PCGEx' Asset Collection to a good ol' attribute set, on the fly.


> Note: Collections currently don't properly check for circular dependencies, so be careful or you're in for a ride.
{: .warning }

---
## Creating new collections

You can create **Asset Collection** just like any other **DataAsset**:  

{% include img a='guides/data-asset-collection.jpg' %}

---
## Rebuilding staging data

"Staging data" is basically per-item cached information. It's pretty lightweight and consist mostly of internal stuffs; and more importantly, asset bounds.

> Note: Staging data is refreshed & stored whenever an update is made to the collection, *but won't refresh when the referenced assets are updated.*
{: .error }

As such you will need to trigger a manual refresh from to time. You can do so using the three buttons at the top of any open Asset Collection:

| Button       | Effect          |
|:-------------|:------------------|
| <span class="ebit">Rebuild Staging</span> | Rebuilds the currently open Asset Collection |
| <span class="ebit">Rebuild Staging (Recursive)</span> | Rebuild the currently open Asset Collection, as well as any sub-collections; recursively. |
| <span class="ebit">Rebuild Staging (Project)</span> | Rebuild ALL the project' Asset Collection.<br>Use carefully as assets needs to be loaded temporarily in memory in order to compute their bounds. |

---
## Available Collections
<br>
{% include card_childs tagged='assetcollection' %}

---
## Entries & Collections Tags
<br>

Both entries and collections can hold tags. Some of these tags are entry-bound (i.e they exist in the context of a given collection only), and some are asset-bound (i.e they are always the same no matter where that entry is referenced).  
**Some nodes, such as {% include lk id='Path Spline Mesh' %} can use & add those tags to the components they generate.**  

Since collections can be nested, and both entries *and* collections can have tags, you can pick'n choose which tags should be grabbed during the entry selection process:  


|: Flags     ||
| <span class="ebit">None</span>           | Tags will be ignored |
| <span class="ebit">Asset</span>           | Grab tags from the **final** picked entry. |
| <span class="ebit">Hierarchy</span>          | Grab tags from the **entries that have been traversed** to the final pick. |
| <span class="ebit">Collection</span>          | Grab tags from the **sub-collections that have been traversed** to the final pick.<br>*This does not include the root collection.* |
| <span class="ebit">Root Collection</span>          | Grab tags from the "root", or main **collection**.<br>*This does not include nested & sub-collections.* |
| <span class="ebit">Root Asset</span>          | Grab tags from the "root", or first traversed entry.<br>**This invalidates the <span class="ebit">Hierarchy</span> flag.** |
{: .enum }

{% include img a='guides/data-asset-collection-grab-tag.png' %}

