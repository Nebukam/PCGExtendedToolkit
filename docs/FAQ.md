# F.A.Q.

## Q: I get "Ensure condition failed: InDependenciesCrc.IsValid()" when caching/uncaching PCGEx nodes!
Yes. It's ok, you can ignore these. PCG isn't natively architectured to support node that switch between cacheable/not-cacheable, which sometime generates failed checks. It's annoying but is harmless.  
The benefits of switching caching mode outweights the assert in my opinion -- if that bugs you, don't touch to the checkbox, it's generally safer *not* to ship with cached data anyway.

