---
layout: page
#grand_parent: All Nodes
parent: Pathfinding
title: Navmesh Pathfinding
subtitle: Sample the navmesh to find paths
summary: The **Navmesh** node ...
color: white
splash: icons/icon_pathfinding-navmesh.svg
preview_img: docs/splash-navmesh.png
tagged: 
    - node
    - pathfinder
nav_order: 3
inputs:
    -   name : Seeds
        desc : Seed points
        pin : points
    -   name : Goals
        desc : Goal points
        pin : points
outputs:
    -   name : Paths
        desc : A point collection per path found
        pin : points
---

{% include header_card_node %}

>Important: Currently, the navigation data used by the node is the one returned by `GetDefaultNavDataInstance()`; **hence it requires a navmesh to be built and loaded at the time of execution.**
{: .error }

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Add Seed to Path           | Prepends the *seed position* at the beginning of the output path.<br>*This will create a point with the position of the seed.* |
| Add Goal to Path           | Appends the *goal position* at the end of the output path.<br>*This will create a point with the position of the goal.* |
| Require Naviguable End Location           | Ensures the picked goal is close enough to an naviguable location, otherwise doesn't generate a path. |

|**Post-processing**||
| Fuse Distance          | Fuse points in the sampled path that are too close together.<br>*The navigation system may sometimes generate intricate paths which points that are very close to each other, which may or may not be suitable for your usecase. This settings gives you a bit of control over that.*|
| **Blending**          | Controls how data is blended on the path points between the `Seed` and `Goal` point.<br>*See {% include lk id='Blending' %}.*|

> Remaining properties are [Unreal' navigation system](https://docs.unrealengine.com/4.27/en-US/InteractiveExperiences/ArtificialIntelligence/NavigationSystem/) query specifics.
> **Despite using the right API, they seem to be ignored for the most part, which is something I need to look into.**
{: .warning }