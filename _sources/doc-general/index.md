---
layout: page
title: ∷ General
subtitle: Notes & informations
summary: This section is dedicated to broader & shared documentation topics. Node specifics can be found on the dedicated node page.
splash: icons/icon_view-grid.svg
preview_img: placeholder.jpg
nav_order: 10
has_children: true
---

{% include header_card %}

>When working with specific nodes, make sure to check out the home of the category they belong to as it often contains important infos pertaining to their family of nodes as a whole!
{: .infos-hl }

<br>

{% include card_childs tagged="basics" wrappercss="duo" %}

---
<br>
Some topics and their modules are more complex, and have a dedicated section:
- {% include lk id='Pathfinding' %}
- {% include lk id='Custom Graphs' %} 
{: .fs-6 .fw-400 } 
<br>

---
## Modules & Sub-processors
Modules and sub-processors are instanced classes used across a variety of nodes.  
<br>
{% include card_childs tagged="module" wrappercss="duo" %}
