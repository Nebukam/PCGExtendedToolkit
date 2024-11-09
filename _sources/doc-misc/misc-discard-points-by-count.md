---
layout: page
family: MiscRemove
#grand_parent: All Nodes
parent: Misc
title: Discard by Points Count
name_in_editor: "Discard By Points Count"
subtitle: Filter point dataset by their point count.
summary: The **Discard Points by Count** node filters point datasets by minimum and maximum point counts, potentially removing entire datasets if they fall outside the defined range, which may also prevent further nodes from processing any data.
color: white
splash: icons/icon_misc-discard-by-count.svg
tagged: 
    - node
    - misc
nav_order: 20
---

{% include header_card_node %}

> Note that this node may output no data at all, depending on its settings -- *in which case subsequent nodes in the graph will be culled.*
{: .warning }

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|:**Settings**||
| Min Point Count      | If enabled, does not output data with a point count smaller than the specified amount.  |
| Max Point Count      | If enabled, does not output data with a point count larger than the specified amount. |