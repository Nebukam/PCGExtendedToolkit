---
layout: page
grand_parent: Pathfinding
parent: 🝓 Goal Pickers
title: 🝓 Random
subtitle: Match seeds to goals at a random index.
summary: The **Random** picker ...
color: blue
splash: icons/icon_placement-center.svg
tagged: 
    - module
    - goalpicker
nav_order: 1
---

{% include header_card_node %}

The random goal picker match each `Seed` with one or multiple `Goals`.

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|:**Settings**||
| Goal Count           | How many random goals each seed should connect to.<br>*See [Index Safety](#index-safety)* |
| Num Goals           | When specifying multiple goals, this is the maximum number of goals to connect each seeds to. |
| Index Safety           | Failsafe method if the picked `Goal` index is out of bounds.<br>*See [Index Safety](#index-safety)* |

## Goal Count

There are three different methods available:
- Single
- Random
- Fixed

### Single
Single will, well, connect to a single random goal.

### Multiple Fixed
Random will connect to a fixed number of random goals, specified in `Num Goals`.

### Multiple Random
Random will connect to a random number of random goals.  
The number of connection will be between `0` and `Num Goals`

### Index Safety
{% include embed id='settings-index-safety' %}