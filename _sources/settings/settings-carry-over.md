---
title: settings-carry-over
has_children: false
nav_exclude: true
---


Since this node merges multiple inputs together, "Carry over" settings lets you pick-and-choose which attributes & tags carry over to the new data.

| Property       | Description          |
|:-------------|:------------------|
| Preserve Attributes Default Value      | If enabled, the node will attempt to create attributes on the data in a way that preserve the original, underlying default value of the attribute.<br>*This can be critical in order to identify which data originally belonged to which, as well as properly initializing flags to a desirable default.*  |
| **Attributes**           | Lets you pick and choose which attributes to keep or dismiss. |
| **Tags**           | Lets you pick and choose which attributes to keep or dismiss. |

*Both Attributes & Tags share the same string-based filters.*

> Note that the filters look for a single valid match amongst the list; you cannot create and/or conditions.
{: .warning }

### Filter Details

| Property       | Description          |
|:-------------|:------------------|
| Filter Mode     | Chooses how the filter operates.<br>- `All` let everything pass.<br>- `Exclude` filters *out* the result of the filter.<br>- `Include` only allows the items that pass the filters.  |
| Matches           | Lets you define a list of checks pairs: a string value, and a `Match Mode`. |
| Comma Separated Names           | Easy to override, lets you specify a list of comma-separated names.<br>*The only caveat is that you can only pick a unique match mode used for each entry.* |
| Comma Separated Names Filter           | Which filter will be used along the Comma Separated Names. |
| Preserve PCGEx Data           | Most of the time you'll want to leave it to its default value. It ensures `PCGEx/` prefixed data are not captured by the filter. |

### Filter Modes

| Mode       | Description          |
|:-------------|:------------------|
| Equals        | Checks for strict equality of the filtered value and the associated string.  |
| Contains      | Checks if the filtered value contains the associated string. |
| Starts With    | Checks if the filtered value is prefixed with the associated string. |
| Ends With     | Checks if the filtered value is suffixed with the associated string. |
