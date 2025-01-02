---
layout: page
family: ClusterGen
#grand_parent: All Nodes
parent: Clusters
title: Custom Graph
name_in_editor: "Cluster : Build Custom Graph"
subtitle: Build clusters using custom blueprints!
summary: The **Build Custom Graph** node allows users to create clusters and their edges using custom blueprints, offering flexibility to generate and manage graphs through blueprint base classes that can be extended.
color: white
splash: icons/icon_blueprint.svg
tagged: 
    - node
    - clusters
nav_order: 100
see_also: 
    - Working with Clusters
inputs:
    -   name : In
        desc : Points used to fetch actor references, depending on the selected mode.
        pin : points
outputs:
    -   name : Vtx
        extra_icon: IN_Vtx
        desc : Endpoints of the output Edges
        pin : points
    -   name : Edges
        extra_icon: OUT_Edges
        desc : Edges associated with the output Vtxs
        pin : points
---

{% include header_card_node %}

The **Build Custom Graph** relies on user-made blueprint to create clusters from scratch. It comes with two blueprint base classes that can be extended and used to create nodes & edges in a very straightforward yet flexible way.
{: .fs-5 .fw-400 } 

{% include img a='details/clusters-custom-graph/lead.png' %}

> It's highly recommended to look at the example project' Custom Graph example -- it's simple enough and covers 100% of the available features for this node.
{: .infos }

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
| Mode           | The selected mode drives how and which actors will be made available to the custom graph builder.<br>- `Owner` only forwards the PCG Component' owner.<br>- `Actor References` takes point dataset coming from a `GetActorData` node and forwards any actor reference that successfully resolve to a world actor. |
| Actor Reference Attribute           | When using `Actor Reference` mode, this is the name of the attribute that contains actor references paths. |
| Builder           | User-made custom graph builder object. |
| Mute Unprocessed Settings Warning           | This quiets the warning regarding invalid settings *(e.g settings that don't have enough nodes)* |
| Cluster Output Settings | *See [Working with Clusters - Cluster Output Settings](/PCGExtendedToolkit/doc-general/working-with-clusters.html#cluster-output-settings).* |

> Note that the PCG Graph will work with a **copy** of the selected builder, not the instance that exists within the setting panel. *In case you're doing fancy stuff in the constructor, which you probably shouldn't, keep in mind only public blueprint variables will be copied from it.*
{: .warning }

---
# Workflow

{% include img a='details/clusters-custom-graph/workflow.png' %}  

The custom graph workflow is made out of two pieces. The main one, `Custom Graph Builder`, drives how many graphs will be generated; The other, `Custom Graph Settings`, is a data/settings holder that is responsible for creating edges & updating individual nodes' PCG points.  
{: .fs-5 .fw-400 } 

1. First, create a new blueprint inheriting from `[PCGEx] Custom Graph Settings`.
2. Override the `InitializeSettings`, `BuildGraph` and `UpdateNodePoint`.
    * `InitializeSettings` is where you confirm the settings are valid, and optionally reserve some memory for your graph.
    * `BuildGraph` is where you will <u>add edges to the graph.</u>
    * `UpdateNodePoint` is where you will <u>set individual node' PCG point properties.</u>
3. Next, create a new blueprint, inheriting from `[PCGEx] Custom Graph Builder`.  
4. Override `InitializeWithContext`.
    <u>This is where you will create "settings" objects using the internal <code>CreateSettings</code> method and the class created before.</u> Each unique settings represent a single graph, that you will populate as you see fit.


> Only `InitializeWithContext` is executed on the main thread, **all other methods are called from asynchronous, multi-threaded places**. If you need custom initialization behavior that is guaranteed to run on the main thread, implement your own method and call it on the setting object after using `CreateSettings` during builder' initialization.
{: .warning }

### Order of operations

1. First, `InitializeSettings` will be called on the main builder instance.
2. Then, for each graph settings registered during initialization, individual Settings objects will have their `BuildGraph` method called. This is where you add edges. *Points will be automatically created based on the IDs you use to register edges.*
3. Finally, each individual Settings will have its `UpdateNodePoint` method called once per node created. *Since this part is heavily multi-threaded, there is no call order guarantee.*
4. That's it!

---
## Custom Graph Builder
<br>

The custom graph builder is only responsible for creating [Custom Graph Settings](#custom-graph-settings), and forwarding them with actor references provided by the host PCG node.  
The execution flow is controlled by the latter, so it's only important to implement `InitializeSettings`.
{: .fs-5 .fw-400 } 

This object has an internal read-only array, `InputActors`, which is populated by the host PCG node based on its selected `Mode`. It is assumed that you either use custom component or blueprint that store custom data from which you will be building custom graphs, so you can fetch & filter more custom components from these actors references.  

You may also access another internal read-only array, `GraphSettings`, which contains all settings created using the `CreateSettings` method. 

### InitializeWithContext

{% include img_link a='details/clusters-custom-graph/initialize-with-context.png' %}

--

You may override `BuildGraph` if you need to do some sort of pre-processing of the settings at the builder level, but it's highly unlikely that you ever need to do so. The multi-threaded nature of this node means that there is no guarantee that other concurrent graphs are properly initialized at this point, so everything should be kept in silos.  

---
## Custom Graph Settings
<br>

The custom graph settings are holding data & providing a handy way to cache per-graph data mapping, as well as a streamlined way to create edges and update graph PCG points. 
{: .fs-5 .fw-400 } 

### InitializeSettings

{% include img_link a='details/clusters-custom-graph/initialize-settings.png' %}
*Example where initialization uses a property on an actor to drive the number of node for these settings. Note that there is no relationship whatsoever between input actors and graph, they're just a convenience thing.*

### BuildGraph

BuildGraph is where you call `AddEdge` appropriately. The method takes two `int64` IDs -- those can be point index, or whatever you see fit. These IDs will be passed to `UpdateNodePoint` later on; along with the internal point index assocaited with it.  
`RemoveEdge` is also available if you want to remove a previously added edge.

> Custom Graph uses per-node IDs instead of indices so that you can build a graph as you discover connections. *It comes at the cost of a slight overhead, but it's much more flexible that way.*
{: .comment }

> You don't have to worry about duplicate/directed edges, the graph system uses an unsigned hash under the hood, so `A -> B` and `B -> A` refer to the same edge.
{: .infos }

{% include img_link a='details/clusters-custom-graph/build-graph.png' %}
*Example where a straight line of edges is created by using a simple loop index.*

### UpdateNodePoint

{% include img_link a='details/clusters-custom-graph/update-node-point.png' %}
*Example where a point position is oriented forward in space using an actor' transform, and setting the point Color from a variable on the actor.*


---
## Cluster Output Settings
*See [Working with Clusters - Cluster Output Settings](/PCGExtendedToolkit/doc-general/working-with-clusters.html#cluster-output-settings).*