---
layout: page
parent: ∷ General
title: Comparisons
subtitle: PCGEx' comparison overview
summary: This page lists the comparison used by a bunch of different nodes.
splash: icons/icon_view-grid.svg
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
|<span class="ebit">==</span>          | Strictly equal. |
|<span class="ebit">!=</span>           | Strictly not equal. |
|<span class="ebit"> >=</span>          | Equal or greater. |
|<span class="ebit"><=</span>           | Equal or smaller. |
|<span class="ebit"> ></span>           | Strictly greater. |
|<span class="ebit">< </span>          | Strictly smaller. |
|<span class="ebit">~=</span>           | Nearly equal. |
|<span class="ebit">!~=</span>           | Nearly not equal. |
{: .enum }

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
| <span class="ebit">==</span>           | Strictly equal. |
| <span class="ebit">!=</span>          | Strictly not equal. |
| <span class="ebit">== (Length)</span>           | String lengths are strictly equal. |
| <span class="ebit">!= (Length)</span>           | String lengths are strictly not equal. |
| <span class="ebit"> >= (Length)</span>           | String length is equal or greater. |
| <span class="ebit"><= (Length)</span>           | String lengths are is equal or smaller. |
| <span class="ebit"> > (Length)</span>           | String lengths is strictly greater. |
| <span class="ebit">< (Length)</span>          | String lengths is Strictly smaller. |
| <span class="ebit"> > (Locale)</span>           | String locale is strictly greater.<br>*In alphabetical order. (Z is greater than A)* |
| <span class="ebit">< (Locale)</span>          | String locale is strictly smaller.<br>*In alphabetical order. (A is smaller than Z)* |
| <span class="ebit">Contains</span>          | Check if string is contained in another one.<br>*Useful if you have solid naming conventions.* |
| <span class="ebit">Starts With</span>          | Check if the string starts with another one.<br>*Useful for prefixes.* |
| <span class="ebit">Ends With</span>          | Check if the string ends with another one.<br>*Useful for suffixes.* |
{: .enum }

---
# Bitmask comparisons
<br>

| Comparison       | Data          |
|:-------------|:------------------|
| <span class="ebit">Match (any)</span>           | Value & Mask != 0 (At least some flags in the mask are set) |
| <span class="ebit">Match (all)</span>           | Value & Mask == Mask (All the flags in the mask are set) |
| <span class="ebit">Match (strict)</span>           | Value == Mask (Flags strictly equals mask) |
| <span class="ebit">No Match (any)</span>           | Value & Mask == 0 (Flags does not contains any from mask) |
| <span class="ebit">No Match (all)</span>           | Value & Mask != Mask (Flags does not contains the mask) |
{: .enum }