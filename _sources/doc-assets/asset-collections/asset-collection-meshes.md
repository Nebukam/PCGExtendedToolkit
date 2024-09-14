---
layout: page
grand_parent: Staging
parent: Asset Collection
title: 🝱 Mesh Collection
subtitle: Mesh Collection DataAsset
summary: The **Mesh Collection** DataAsset is a list of Meshes with ISM/HISM Descriptors, that comes with all the Asset Collection goodies.
color: white
splash: icons/icon_component.svg
see_also: 
    - Asset Collection
tagged: 
    - assetcollection
    - staging
nav_order: 7
---

{% include header_card_node %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|: **Settings** :||
| Entries           | Entries are a list of meshes (soft) references descriptor and their associated shared properties.<br>*You really just need to set the Descriptor' StaticMesh property for it to work.* |
| Do Not Ignore Invalid Entries          | Forces distribution of this collection to NOT skip over invalid entries.<br>This can be useful to create 'weighted' spaces, and can be overriden on a per-node basis. |

|: **Advanced** :||
| Auto Rebuild Staging           | Enabled by default.<br>When enabled, any user-made modification to the asset collection will trigger a rebuilding of the staging data *(saves you a click, in case your forget about it.)* |

{% include embed id='settings-col-asset' %}