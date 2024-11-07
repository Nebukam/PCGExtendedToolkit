---
title: settings-adjacency
has_children: false
nav_exclude: true
---

---
## Adjacency

| Property       | Description          |
|:-------------|:------------------|
|:**Settings** :|
| Mode          | The mode lets you choose how to set the flags value, as a user.<br>- `Direct` is probably the most useful, as it can be set using an override pin.<br>- `Individual` lets you use an array where you can set individual bits by their position (index), and whether they're true or false.<br>- `Composite` lets you set individual bits using dropdowns. |


### Mode : All

This mode consolidate all the connections' values into a single one to test against.

|: **Consolidation** :|
| Individual | Test individual neighbors one by one |
| Individual | Test against averaged value of all neighbors |
| Individual | Test against the Min value of all neighbors |
| Individual | Test against the Max value of all neighbors |
| Individual | Test against the Sum value of all neighbors |

> DOC TBD
{: .warning }

### Mode : Some

This mode test individual connections, but only *some* of them are required to pass for the entire filter to be considered a success.  
How that "some" is defined relies on a comparison against a threshold: **how many neighbors pass the test vs. how many neighbors there are.**

|: **Threshold Settings** :|
| Threshold Comparison | TBD |
| Threshold Type | TBD |
| Threshold Source | TBD |
| Discrete Threshold | TBD |
| Relative Threshold | TBD |
| Threshold Attribute | TBD |
| Threshold Source | TBD |
| Rounding | TBD |
| Threshold Tolerance | TBD |

> DOC TBD
{: .warning }