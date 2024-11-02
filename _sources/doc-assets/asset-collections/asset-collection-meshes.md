---
layout: page
grand_parent: Staging
parent: Asset Collection
title: 🝱 Mesh Collection
subtitle: Mesh Collection DataAsset
summary: The **Mesh Collection** DataAsset is a list of Meshes with ISM/HISM Descriptors, that comes with all the Asset Collection goodies.
color: blue
splash: icons/icon_blueprint.svg
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
| Collection Tags           | A set of unique tags associated with this collection data asset. |
| Entries           | Entries are a list of meshes (soft) references descriptor and their associated shared properties.<br>*You really just need to set the Descriptor' StaticMesh property for it to work.* |
| Do Not Ignore Invalid Entries          | Forces distribution of this collection to NOT skip over invalid entries.<br>This can be useful to create 'weighted' spaces, and can be overriden on a per-node basis. |

|: **Advanced** :||
| Auto Rebuild Staging           | Enabled by default.<br>When enabled, any user-made modification to the asset collection will trigger a rebuilding of the staging data *(saves you a click, in case your forget about it.)* |

---
# Individual Entry
<br>

| Property       | Description          |
|:-------------|:------------------|
|: **Settings** :||
| Static Mesh           | The static mesh associated with this entry. |
| ISM Descriptor          | Descriptor used for this static mesh when used with a `Instanced Static Mesh` component.<br>*Note that there is no need to re-set the Static Mesh, it will use the one set at the top-level of the entry itself.* |
| SM Descriptor          | Descriptor used for this static mesh when used with a regular `Static Mesh` component, such as `Spline Mesh`. |
| Sub Collection          | If this entry is marked as a sub-collection, this let you specify which collection this entry will get its data from. |

{% include embed id='settings-col-asset' %}
