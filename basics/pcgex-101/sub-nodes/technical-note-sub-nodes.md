---
hidden: true
icon: microchip
---

# Technical Note : Sub Nodes

Notes:\
\- Most Sub Nodes are actually factories\
\- Processors are created on the fly, per-data\
\- There's a massive opportunity for memory optimization, as right now Sub Nodes and Instanced Behavior inherit from the same base UObject class, but mostly for legacy reasons.
