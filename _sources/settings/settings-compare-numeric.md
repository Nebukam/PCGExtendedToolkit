---
title: settings-compare-numeric
has_children: false
nav_exclude: true
---

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