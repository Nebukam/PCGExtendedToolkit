---
layout: page
parent: ∷ General
title: Basics
subtitle: General concepts & disambiguation
summary: An overview of the concepts PCGEx uses, and assumption around the meaning of certain keywords.
splash: icons/icon_view-grid.svg
preview_img: placeholder.jpg
nav_order: 0
tagged:
    - basics
---

{% include header_card %}

Throrough this doc, you will find mention of **points**, **Dataset**, **vtx**, **edges**, **clusters**, **paths** and more.  
While more advanced concepts are expanded on their own page, here are a few basics:

## Points
Points are smallest piece of data you work with within PCG. They have a list of **Static properties** such as `$Density`, `$Color`, `$Position`, `$Rotation` etc. While you can change their values, you can't remove any of them from a point. They're its DNA.  
On top of static properties, point can have **Dynamic attributes**, which are custom, user-defined properties which type, names and value you control.  

> PCGEx leverages attributes a lot, and doesn't deal too much in properties except for blending.
{: .comment }  

> Some PCGEx nodes uses attributes to store internal data. These are prefixed `PCGEx/`. Try not do modify these unless you know exactly what you're doing -- most of them are more complex data stored as `uint64`.
{: .warning }

## Dataset
Often *Point* Datasets, they are container for a single type of more atomic data -- i.e, points.  Contrary to points that share a common set of static properties, each Dataset can have its own custom list of attributes; with per-point values. Most PCGEx nodes can take multiple *Dataset* as input, and will process them in parallel.  

Some very specific nodes are processing Dataset against each others, but it's clear when they do so. It's important to understand that most PCGEx nodes will do their biding on individual Datasets, ignoring what's going on with others processed in parallel : things like min/max values for example are computed *per* Dataset, and never against the entierety of the inputs.

> As a rule of thumb, PCGEx will always process Dataset in solation of the others, and **output Dataset in the same order as they are in the input**.
{: .comment }

## Paths, Vtx and Edges
PCGEx uses the terms **Paths**, **Vtx** and **Edges** for some inputs & ouputs. **These are just regular Point Datasets whose name is used to convey certain assumptions**. Those assumptions are covered in more details on specific sections of the doc. Namely {% include lk id='Clusters' %} and {% include lk id='Paths' %}

> Keep in mind that these are *just points* and they can be used with any other regular node -- not just PCGEx!
{: .infos-hl }

## Clusters
Clusters is the word PCGEx uses to mean *Connected pair/groups of points*. Pairs/groups are identified & retrieved through tags at the moment; *that will change in the future for a more robust approach.*  While they are still *just points*, make sure to check out the {% include lk id='Clusters' %} specific documentation!
> Cluster-related nodes uses tags to store internal data. These are prefixed `PCGEx/`. Try not do modify these unless you know exactly what you're doing.
{: .warning }


