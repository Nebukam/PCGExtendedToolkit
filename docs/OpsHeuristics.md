# Heuristics

Heuristics are basically some under-the-hood maths used by [Pathfinding Algorythms](OpsPathfinding.md) to gauge whether one path is better than another.  Different algorithms use heuristics differently, but their values is computed consistently.

Right now, there are a few main available heuristics, on top of additive custom modifiers.
- [Direction](#Heuristics-:-Direction)
- [Shortest Distance](#Heuristics-:-Shortest-Distance)
- [Local Distance (MST)](#Heuristics-:-Local-Distance-(MST))
- [Node Count](#Heuristics-:-Node-Count)
- [Modifiers Only](#Heuristics-:-Modifiers-Only)

> Note that most of what the heuristics are computing right now, it would mostly be possible to compute the same values in nodes only. It's simply much more convenient to have them coded already, as a they often require just a few lines of code as opposed to a whole lot of nodes ;)

## Heuristics : Direction

## Heuristics : Shortest Distance

## Heuristics : Local Distance (MST)

## Heuristics : Node Count

## Heuristics : Modifiers Only
As the name implies, Modifiers Only does no specific calculations and rely solely on the modifiers. If you want full control, use this.

# Modifiers
