---
layout: page
parent: Goal Pickers
grand_parent: ∷ Pathfinding
title: Random
subtitle: Match seeds to goals at a random index.
color: white
#summary: summary_goes_here
splash: icons/icon_placement-center.svg
preview_img: docs/splash-picker-random.png
toc_img: placeholder.jpg
tagged: 
    - goalpicker
nav_order: 1
---

{% include header_card_node %}

The random goal picker match each `Seed` with one or multiple `Goals`.

{% include img a='details/modules/picker-random.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
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

{% include embed id='settings-index-safety' %}