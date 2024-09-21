---
title: settings-forward
has_children: false
nav_exclude: true
---


"Forwarding" settings lets you pick-and-choose which attributes & tags carry over from some data to another.

| Property       | Description          |
|:-------------|:------------------|
| Enabled | Whether attribute forwarding is enabled or not. |
| Preserve Attributes Default Value      | If enabled, the node will attempt to create attributes on the data in a way that preserve the original, underlying default value of the attribute.<br>*This can be critical in order to identify which data originally belonged to which, as well as properly initializing flags to a desirable default.*  |

> Note that the filters look for a single valid match amongst the list; you cannot create and/or conditions.
{: .comment }

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
| <span class="ebit">Equals</span>        | Checks for strict equality of the filtered value and the associated string.  |
| <span class="ebit">Contains</span>      | Checks if the filtered value contains the associated string. |
| <span class="ebit">Starts With</span>    | Checks if the filtered value is prefixed with the associated string. |
| <span class="ebit">Ends With</span>     | Checks if the filtered value is suffixed with the associated string. |
{: .enum }
