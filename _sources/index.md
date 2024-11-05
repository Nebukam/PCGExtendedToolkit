---
layout: page
title: PCG Extended Toolkit
nav_order: 0
has_children: true
---
<div class="product-header large" style="--img:url('{{ site.baseurl }}/assets/images/logo.png');"><div class="infos" markdown="1">
# PCGEx {% include github.html repo="PCGExtendedToolkit" %}  
The PCG Extended Toolkit is a free (libre) plugin that extends Unreal Engine' Procedural Content Generation pipeline, with a focus on **building clusters & pathfinding**.  
{: .fs-6 .fw-300 }  
   
{% include link_btn title="Installation" color="white" link="installation" %} 
{% include link_btn title="Example Project" color="red" link="https://github.com/Nebukam/PCGExExampleProject" icon="left" %}
{% include link_btn title="Discord" color="blue" link="https://discord.gg/mde2vC5gbE" icon="left" %}
</div></div>

---
>When working with specific nodes, make sure to check out the home of the category they belong to as it often contains important infos pertaining to their family of nodes as a whole!
{: .infos-hl }

<br>

## Nodes Categories
<br>
{% include card_any tagged='category-index' wrappercss="duo" %}

---
# All Nodes
## {% include lk id='Clusters' %}
<br>
{% include card_childs reference="Clusters" tagged='clusters' %}

---
## {% include lk id='Pathfinding' %}
<br>
{% include card_childs reference="Pathfinding" tagged='pathfinder' %}

---
## {% include lk id='Paths' %}
<br>
{% include card_childs reference="Paths" tagged='paths' %}

---
## {% include lk id='Staging' %}
<br>
{% include card_childs reference="Staging" tagged='staging' %}

---
## {% include lk id='Misc' %}
<br>
{% include card_childs reference="Misc" tagged='misc' %}

---
## {% include lk id='Sampling' %}
<br>
{% include card_childs reference="Sampling" tagged='sampling' %}

---
## {% include lk id='Shape Ecosystem' %}
<br>
{% include card_childs reference="Shape Ecosystem" tagged='node' %}

---
## {% include lk id='Filter Ecosystem' %}
<br>
{% include card_childs reference="Filter Ecosystem" tagged='filter-ecosystem' %}

---
## Individual Filters
*There are here to enable `ctrl+f` users. You can find all about them in the {% include lk id='Filter Ecosystem' %} section.*
<br>
{% include card_any tagged='filter' %}