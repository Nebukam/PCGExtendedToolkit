---
title: settings-attribute-to-tags
has_children: false
nav_exclude: true
---


Attribute to tags settings lets you pick attributes on references points that will be converted to tags on the data they are matched to.

| Property       | Description          |
|:-------------|:------------------|
| Add Index Tag     | Use reference point index to tag output data. |
| <span class="etag">Index Tag Prefix</span>      | If `Add Index Tag` is enabled, lets you set a prefix to the reference index.  |
| Attributes      | List of attributes on reference points to turn into tags.  |

> Note that while any attribute type is supported, they will be converted to a string under the hood, which may yield unexpected values.
{: .comment }