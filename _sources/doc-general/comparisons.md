---
layout: page
parent: ∷ General
title: Comparisons
subtitle: PCGEx' comparison overview
summary: This page lists the comparison used by a bunch of different nodes.
splash: icons/icon_view-grid.svg
preview_img: placeholder.jpg
nav_order: 10
tagged:
    - basics
---

{% include header_card %}

There's a lot of node in PCGEx that offer comparison settings -- either numeric or string based. This is a summary of these; individual pages will link to this.
{: .fs-5 .fw-400 } 

---
# Numeric comparisons
<br>

| Comparison       | Data          |
|:-------------|:------------------|
| `==`           | Strictly equal. |
| `!=`           | Strictly not equal. |
| `>=`           | Equal or greater. |
| `<=`           | Equal or smaller. |
| `>`           | Strictly greater. |
| `<`           | Strictly smaller. |
| `~=`           | Nearly equal. |
| `!~=`           | Nearly not equal. |

> Approximative comparison will reveal an additional parameter, dubbed `Tolerance`. This represents the size of acceptable approximation for the comparison to pass.
> For example, when checking if `0.5 ~= 0.4` with a tolerance of `0.1` will return `true`.
{: .infos }

> Large tolerances can be a great, cheap way to achieve results akin to a "within range" comparison!
{: .comment } 

---
# String comparisons
<br>

| Comparison       | Data          |
|:-------------|:------------------|
| `==`           | Strictly equal. |
| `!=`           | Strictly not equal. |
| `== (Length)`           | String lengths are strictly equal. |
| `!= (Length)`           | String lengths are strictly not equal. |
| `>= (Length)`           | String length is equal or greater. |
| `<= (Length)`           | String lengths are is equal or smaller. |
| `> (Length)`           | String lengths is strictly greater. |
| `< (Length)`           | String lengths is Strictly smaller. |
| `> (Locale)`           | String locale is strictly greater.<br>*In alphabetical order. (Z is greater than A)* |
| `< (Locale)`           | String locale is strictly smaller.<br>*In alphabetical order. (A is smaller than Z)* |
| `Contains`           | Check if string is contained in another one.<br>*Useful if you have solid naming conventions.* |
| `Starts With`           | Check if the string starts with another one.<br>*Useful for prefixes.* |
| `Ends With`           | Check if the string ends with another one.<br>*Useful for suffixes.* |