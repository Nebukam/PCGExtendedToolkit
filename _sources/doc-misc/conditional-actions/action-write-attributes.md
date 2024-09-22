---
layout: page
family: Misc
grand_parent: Misc
parent: Conditional Actions
title: 🝐 Write Attributes
name_in_editor: "Action : Write Attributes"
subtitle: The **Write Attribute** action...
color: white
summary: TBD
splash: icons/icon_misc-write-index.svg
tagged: 
    - node
    - action
    - misc
nav_order: 7
inputs:
    -   name : Conditions
        desc : Filters used as condition to check which match to choose from
        pin : 
    -   name : Match Success
        desc : Attributes that will be written if the conditions pass
        pin : any
    -   name : Match Fail
        desc : Attributes that will be written if the conditions fails
        pin : any
outputs:
    -   name : Action
        desc : A single action definition
        pin : params
---

{% include header_card_node %}

# Properties
<br>

> DOC TDB
{: .warning }