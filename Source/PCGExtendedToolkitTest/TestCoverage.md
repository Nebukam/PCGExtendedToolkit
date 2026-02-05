# PCGEx Test Coverage Tracking

This document tracks test coverage across the PCGExtendedToolkit codebase.

## Coverage Legend
- `[x]` Tested (has dedicated tests)
- `[~]` Partially tested (some functions tested)
- `[ ]` Not tested
- `[N/A]` Not applicable for testing (infrastructure, macros, etc.)

## Module Priority Tiers

| Tier | Classification | Modules |
|------|---------------|---------|
| **1** | Core/Essential | PCGExCore, PCGExFoundations, PCGExGraphs, PCGExFilters, PCGExBlending, PCGExHeuristics, PCGExElementsPaths, PCGExElementsPathfinding, PCGExElementsClustersDiagrams, PCGExElementsSampling, PCGExElementsBridges |
| **2** | Commonly Used | PCGExCollections, PCGExMatching, PCGExProperties, PCGExElementsMeta, PCGExElementsSpatial, PCGExElementsShapes, PCGExElementsClusters, PCGExElementsClustersRefine, PCGExElementsFloodFill, PCGExElementsProbing, PCGExElementsClipper2 |
| **3** | Niche | PCGExNoise3D, PCGExPickers, PCGExElementsClustersRelax, PCGExElementsValency |
| **4** | Rarely Used | PCGExElementsTopology, PCGExElementsTensors, PCGExElementsActions |
| **Skip** | No Testing | PCGExElementsPathfindingNavmesh, All Editor modules, ThirdParty |

---

## Tier 1: Core/Essential

### PCGExCore
Test files: `Tests/Unit/Math/`, `Tests/Unit/Containers/`

#### Math (19 headers)
| Component | Status | Test File | Notes |
|-----------|--------|-----------|-------|
| **PCGExMath.h** | [~] | PCGExMathTests, PCGExMathUtilTests | Core functions covered |
| ├─ Tile | [x] | PCGExMathTests | int/float |
| ├─ Remap | [x] | PCGExMathTests | |
| ├─ SanitizeIndex | [x] | PCGExMathTests | All modes |
| ├─ Distance (Manhattan/Chebyshev) | [x] | PCGExMathTests | |
| ├─ Snap, Round10 | [x] | PCGExMathTests | |
| ├─ DegreesToDot | [x] | PCGExMathTests | |
| ├─ SignPlus/SignMinus | [x] | PCGExMathTests | |
| ├─ TruncateDbl | [x] | PCGExMathUtilTests | |
| ├─ FClosestPosition | [x] | PCGExMathUtilTests | |
| ├─ FSegment | [x] | PCGExMathUtilTests | |
| ├─ GetPerpendicularDistance | [x] | PCGExMathUtilTests | |
| ├─ GetMinMax | [x] | PCGExMathUtilTests | |
| ├─ ReverseRange | [x] | PCGExMathUtilTests | |
| └─ (remaining functions) | [ ] | | FastRand, ConeBox, etc. |
| **PCGExMathDistances.h** | [x] | PCGExMathDistancesTests | GetDistances factory, IDistances interface, Euclidean/Manhattan/Chebyshev metrics |
| **PCGExMathAxis.h** | [x] | PCGExMathAxisTests | Axis order, direction, swizzle, angles |
| **PCGExMathBounds.h** | [~] | PCGExMathBoundsTests | SanitizeBounds, EPCGExBoxCheckMode enum |
| **PCGExMathMean.h** | [x] | PCGExMathMeanTests | Average, Median, QuickSelect |
| **PCGExWinding.h** | [x] | PCGExWindingTests | IsWinded, FPolygonInfos, AngleCCW |
| **PCGExBestFitPlane.h** | [x] | PCGExBestFitPlaneTests | Plane fitting, centroid, normal, extents |
| **PCGExDelaunay.h** | [x] | PCGExDelaunayTests | FDelaunaySite2 (constructor, edge hash, ContainsEdge, GetSharedEdge, PushAdjacency), FDelaunaySite3 (constructor, ComputeFaces), TDelaunay2::Process, TDelaunay3::Process, RemoveLongestEdges, hull detection |
| **PCGExVoronoi.h** | [x] | PCGExVoronoiTests | TVoronoi2 (Process, bounds, metrics: Euclidean/Manhattan/Chebyshev, cell centers: Circumcenter/Centroid/Balanced), TVoronoi3 (Process, circumspheres, centroids), EPCGExVoronoiMetric, EPCGExCellCenter |
| **PCGExGeo.h** | [x] | PCGExGeoTests | Det, Centroid, Circumcenter, Barycentric, PointInTriangle/Polygon, L-inf transforms, edge paths, sphere fitting |
| **PCGExOBB.h** | [x] | PCGExOBBTests | Factory, PointInside, SphereOverlap, SATOverlap, SignedDistance, ClosestPoint, TestPoint modes, TestOverlap modes, FPolicy |
| **PCGExOBBTests.h** | [x] | PCGExOBBTests | All test utilities (TestPoint, TestOverlap, FPolicy runtime class) |
| **PCGExOBBCollection.h** | [x] | PCGExOBBCollectionTests | FCollection construction, Add, BuildOctree, IsPointInside, Overlaps, FindContaining, FindAllOverlaps, SegmentIntersectsAny, ClassifyPoints, FilterInside, Encompasses |
| **PCGExOBBSampling.h** | [x] | PCGExOBBSamplingTests | FSample struct, Sample, SampleFast, SampleWithWeight, UVW computation, weight functions |
| **PCGExOBBIntersections.h** | [x] | PCGExOBBIntersectionsTests | FCut, FIntersections (Sort, SortAndDedupe, GetBounds), SegmentBoxRaw, ProcessSegment, SegmentIntersects, EPCGExCutType |

#### Containers (5 headers)
| Component | Status | Test File | Notes |
|-----------|--------|-----------|-------|
| **PCGExIndexLookup.h** | [x] | PCGExIndexLookupTests | Full coverage |
| **PCGExHashLookup.h** | [x] | PCGExHashLookupTests | Full coverage |
| **PCGExScopedContainers.h** | [~] | PCGExShardedContainersTests | Sharded containers only |
| **PCGExManagedObjects.h** | [ ] | | |
| **PCGExManagedObjectsInterfaces.h** | [N/A] | | Interface only |

#### Helpers (12 headers)
Test files: `Tests/Unit/Helpers/`

| Component | Status | Test File | Notes |
|-----------|--------|-----------|-------|
| **PCGExArrayHelpers.h** | [x] | PCGExArrayHelpersTests | String parsing, reverse, reorder, indices |
| **PCGExMetaHelpers.h** | [x] | PCGExMetaHelpersTests | IsPCGExAttribute, MakePCGExAttributeName, IsWritableAttributeName, IsDataDomainAttribute, StripDomainFromName, GetPropertyType, GetPropertyNativeTypes |
| PCGExMetaHelpersMacros.h | [N/A] | | Macros |
| PCGExPointArrayDataHelpers.h | [ ] | | |
| PCGExBufferHelper.h | [ ] | | |
| PCGExAttributeMapHelpers.h | [ ] | | |
| PCGExStreamingHelpers.h | [ ] | | |
| PCGExAssetLoader.h | [ ] | | |
| PCGExAsyncHelpers.h | [ ] | | |
| **PCGExRandomHelpers.h** | [x] | PCGExRandomHelpersTests | FastRand01, ComputeSpatialSeed |
| PCGExPropertyHelpers.h | [ ] | | |
| PCGExFunctionPrototypes.h | [N/A] | | Forward decls |

#### Types (9 headers)
Test files: `Tests/Unit/Types/`

| Component | Status | Test File | Notes |
|-----------|--------|-----------|-------|
| **PCGExTypeOpsNumeric.h** | [x] | PCGExTypeOpsNumericTests | bool, int32, float, double ops; conversions, blends, hash |
| **PCGExTypeOpsVector.h** | [x] | PCGExTypeOpsVectorTests | FVector2D, FVector, FVector4 ops; conversions, blends, modulo |
| **PCGExTypeOpsRotation.h** | [x] | PCGExTypeOpsRotationTests | FRotator, FQuat, FTransform ops; conversions, blends, modulo, hash |
| **PCGExTypeOpsString.h** | [x] | PCGExTypeOpsStringTests | FString, FName, FSoftObjectPath, FSoftClassPath ops; conversions, blends |
| **PCGExTypeTraits.h** | [x] | PCGExTypeTraitsTests | TTraits<T> for all types; Type, TypeId, feature flags (bIsNumeric, bIsVector, bSupportsLerp, etc.) |
| PCGExAttributeIdentity.h | [ ] | |
| **PCGExTypes.h** | [x] | PCGExTypesTests | FScopedTypedValue (construction, lifecycle, move, all types), convenience functions (Convert, ComputeHash, AreEqual, Lerp, Clamp, Abs, Factor) |
| PCGExTypesCore.h | [ ] | |
| PCGExEnums.h | [ ] | |

#### Data (~31 headers)
| Component | Status | Notes |
|-----------|--------|-------|
| (All data structures) | [ ] | |

#### Bitmasks (3 headers)
Test files: `Tests/Unit/Bitmasks/`

| Component | Status | Test File | Notes |
|-----------|--------|-----------|-------|
| **PCGExBitmaskCommon.h** | [x] | PCGExBitmaskTests | EPCGExBitOp (Set/AND/OR/NOT/XOR), EPCGExBitflagComparison (MatchPartial/Full/Strict/NoMatchPartial/NoMatchFull), GetBitOp conversion |
| **PCGExBitmaskDetails.h** | [x] | PCGExBitmaskTests | FPCGExClampedBitOp::Mutate, FPCGExSimpleBitmask::Mutate, chained operations, common flag patterns |
| PCGExBitmaskCollection.h | [ ] | | UObject collection |

#### Other Core
| Component | Status | Test File | Notes |
|-----------|--------|-----------|-------|
| **PCGExLink.h** | [x] | PCGExClusterStructsTests | FLink construction, H64, equality, GetTypeHash |
| **PCGExEdge.h** | [x] | PCGExClusterStructsTests | FEdge construction, Other, Contains, equality, H64U, comparison; EPCGExEdgeDirectionMethod, EPCGExEdgeDirectionChoice enums |
| **PCGExNode.h** | [~] | PCGExClusterStructsTests | FNode construction, Num, IsEmpty, IsLeaf/IsBinary/IsComplex, LinkEdge, Link, IsAdjacentTo, GetEdgeIndex, NodeGUID (cluster-dependent functions not tested) |
| Clusters (remaining ~6 headers) | [ ] | | PCGExCluster, PCGExClusterCache, etc. |
| Paths (~5 headers) | [ ] | |
| Sorting (~4 headers) | [~] | PCGExSortingHelpersTests - FVectorKey, RadixSort |
| Factories (~4 headers) | [ ] | |

#### Utils
Test files: `Tests/Unit/Utils/`

| Component | Status | Test File | Notes |
|-----------|--------|-----------|-------|
| **PCGExCompare.h** | [x] | PCGExCompareTests | All comparison ops (==, !=, >=, <=, >, <, ~=, !~=), string comparisons, ToString |

---

### PCGExFoundations
| Component | Status | Notes |
|-----------|--------|-------|
| Elements (~25 headers) | [ ] | |
| Details (~12 headers) | [ ] | Config USTRUCTs |
| Core (~3 headers) | [ ] | |
| Other | [ ] | |

---

### PCGExGraphs
| Component | Status | Notes |
|-----------|--------|-------|
| Graph structures (~10 headers) | [ ] | |
| Cluster artifacts (~6 headers) | [ ] | |
| Core (~2 headers) | [ ] | |

---

### PCGExFilters
Test files: `Tests/Unit/Filters/`, `Tests/Integration/Filters/`

| Component | Status | Test File | Notes |
|-----------|--------|-----------|-------|
| **Framework** | [~] | PCGExFilterTests.spec | Enums, logic patterns |
| **PCGExConstantFilter.h** | [x] | PCGExConstantFilterTests | Full coverage |
| **PCGExNumericCompareFilter.h** | [~] | PCGExNumericCompareLogicTests | Logic simulation (all comparison ops, tolerance, edge cases) |
| PCGExStringCompareFilter.h | [ ] | | |
| **PCGExBooleanCompareFilter.h** | [~] | PCGExFilterLogicTests | Logic simulation (Equal/NotEqual) |
| PCGExDistanceFilter.h | [ ] | | |
| **PCGExDotFilter.h** | [~] | PCGExDotFilterLogicTests | Logic simulation (scalar/degrees domain, unsigned mode, all comparison ops, vector dot products) |
| PCGExAngleFilter.h | [ ] | | |
| PCGExAttributeCheckFilter.h | [ ] | | |
| PCGExBitmaskFilter.h | [ ] | | (bitmask ops tested in PCGExBitmaskTests) |
| **PCGExWithinRangeFilter.h** | [~] | PCGExFilterLogicTests | Logic simulation (inclusive/exclusive, invert) |
| PCGExRandomFilter.h | [ ] | | |
| PCGExRandomRatioFilter.h | [ ] | | |
| **PCGExModuloCompareFilter.h** | [~] | PCGExFilterLogicTests | Logic simulation (modulo ops, zero handling) |
| PCGExBoundsFilter.h | [ ] | | |
| PCGExMeanFilter.h | [ ] | | |
| (remaining ~15 filters) | [ ] | | |

---

### PCGExBlending
| Component | Status | Notes |
|-----------|--------|-------|
| Blenders (~3 headers) | [ ] | |
| Core (~7 headers) | [ ] | |
| Details (~3 headers) | [ ] | |
| Sampling (~4 headers) | [ ] | |
| SubPoints (~6 headers) | [ ] | |

---

### PCGExHeuristics
| Component | Status | Notes |
|-----------|--------|-------|
| All heuristic functions (~14 headers) | [ ] | |

---

### PCGExElementsPaths
| Component | Status | Notes |
|-----------|--------|-------|
| Path operations (~36 headers) | [ ] | Bevel, blend, offset, etc. |

---

### PCGExElementsPathfinding
| Component | Status | Notes |
|-----------|--------|-------|
| Search algorithms (~24 headers) | [ ] | A*, Dijkstra, etc. |

---

### PCGExElementsClustersDiagrams
| Component | Status | Notes |
|-----------|--------|-------|
| Diagram operations (~9 headers) | [ ] | Voronoi, Delaunay |

---

### PCGExElementsSampling
| Component | Status | Notes |
|-----------|--------|-------|
| Sampling operations (~16 headers) | [ ] | |

---

### PCGExElementsBridges
| Component | Status | Notes |
|-----------|--------|-------|
| Bridge operations (~2 headers) | [ ] | |

---

## Tier 2: Commonly Used

### PCGExCollections
Test files: `Tests/Unit/Collections/`

| Component | Status | Test File | Notes |
|-----------|--------|-----------|-------|
| **Core/PCGExAssetCollection.h** | [~] | PCGExCollectionCacheTests | Cache system (FCategory, FMicroCache, FCache) |
| ├─ FCategory | [x] | PCGExCollectionCacheTests | Empty, single, multi-entry, ascending/descending/weight order, random/weighted random |
| ├─ FMicroCache | [x] | PCGExCollectionCacheTests | Empty, single, multi-entry, weight sum, pick modes |
| ├─ FCache | [~] | PCGExCollectionCacheTests | Constructor, Main category initialization |
| └─ FPCGExEntryAccessResult | [x] | PCGExCollectionCacheTests | Default state, validity checks |
| **Core/PCGExAssetCollectionTypes.h** | [~] | PCGExCollectionCacheTests | FPCGExCollectionTypeSet, TypeIds |
| ├─ FPCGExCollectionTypeSet | [x] | PCGExCollectionCacheTests | Empty, single, initializer list, add/remove, union/intersection |
| └─ TypeIds | [x] | PCGExCollectionCacheTests | None, Base, Mesh, Actor, PCGDataAsset values |
| **PCGExCollectionsCommon.h** | [x] | PCGExCollectionCacheTests | EPCGExIndexPickMode, EPCGExDistribution, EPCGExAssetTagInheritance enums |
| **FPCGExAssetStagingData** | [x] | PCGExCollectionEntryTests | Default state, FindSocket (by name, by name+tag, first match) |
| **FPCGExAssetCollectionEntry** | [~] | PCGExCollectionEntryTests | Default state, GetTypeId, HasValidSubCollection, HasPropertyOverride, ClearSubCollection |
| **Details/PCGExStagingDetails.h** | [~] | PCGExCollectionEntryTests | FPCGExAssetTaggingDetails, FPCGExAssetDistributionDetails, FPCGExMicroCacheDistributionDetails, FPCGExAssetAttributeSetDetails, FPCGExAssetDistributionIndexDetails defaults |
| Helpers/PCGExCollectionsHelpers.h | [ ] | | FDistributionHelper, FMicroDistributionHelper |
| Collections/*.h | [ ] | | Mesh, Actor, PCGDataAsset collections |
| Elements/*.h | [ ] | | Staging elements |

| Module | Headers | Status | Notes |
|--------|---------|--------|-------|
| PCGExMatching | ~14 | [ ] | |
| PCGExProperties | ~6 | [ ] | |
| PCGExElementsMeta | ~20 | [ ] | |
| PCGExElementsSpatial | ~16 | [ ] | |
| PCGExElementsShapes | ~12 | [ ] | |
| PCGExElementsClusters | ~51 | [ ] | |
| PCGExElementsClustersRefine | ~20 | [ ] | |
| PCGExElementsFloodFill | ~17 | [ ] | |
| PCGExElementsProbing | ~20 | [ ] | |
| PCGExElementsClipper2 | ~15 | [ ] | |

---

## Tier 3: Niche

| Module | Headers | Status | Notes |
|--------|---------|--------|-------|
| PCGExNoise3D | ~22 | [ ] | |
| PCGExPickers | ~7 | [ ] | |
| PCGExElementsClustersRelax | ~10 | [ ] | |
| PCGExElementsValency | ~23 | [ ] | |

---

## Tier 4: Rarely Used

| Module | Headers | Status | Notes |
|--------|---------|--------|-------|
| PCGExElementsTopology | ~10 | [ ] | |
| PCGExElementsTensors | ~30 | [ ] | |
| PCGExElementsActions | ~5 | [ ] | |

---

## Test Infrastructure

### Fixtures & Helpers
| Component | Status | Location | Notes |
|-----------|--------|----------|-------|
| **FTestContext** | [x] | Fixtures/PCGExTestContext.h | Full PCG context infrastructure - creates FPCGExContext, world, actor, PCGComponent |
| **FScopedTestContext** | [x] | Fixtures/PCGExTestContext.h | RAII wrapper for FTestContext |
| FTestFixture | [x] | Fixtures/PCGExTestFixtures.h | Legacy fixture, now uses FTestContext internally |
| FPointDataBuilder | [x] | Helpers/PCGExPointDataHelpers.h | Builder pattern for test point data |
| PCGExTestHelpers | [x] | Helpers/PCGExTestHelpers.h | NearlyEqual, GetTestSeed, Generate*Positions |

### Test Context Features
| Feature | Method | Description |
|---------|--------|-------------|
| Create FPointIO | `CreatePointIO()` | Creates FPointIO for testing |
| Create Facade | `CreateFacade(NumPoints)` | Sequential points along X |
| Create Grid Facade | `CreateGridFacade(Origin, Spacing, X, Y, Z)` | 3D grid of points |
| Create Random Facade | `CreateRandomFacade(Bounds, NumPoints, Seed)` | Reproducible random positions |
| Access Context | `GetContext()` | Returns underlying FPCGExContext |

### Test Patterns Available
- Unit tests (IMPLEMENT_SIMPLE_AUTOMATION_TEST)
- BDD specs (BEGIN_DEFINE_SPEC)
- **Integration tests (FScopedTestContext + real PCG data)** - NEW
- Functional tests (full context)
- Performance/Stress tests

### Integration Tests
Test files: `Tests/Integration/`

| Component | Status | Test File | Notes |
|-----------|--------|-----------|-------|
| **TestContext.BasicInitialization** | [x] | PCGExFilterIntegrationTests | World, actor, PCGComponent, context creation |
| **TestContext.FacadeCreation** | [x] | PCGExFilterIntegrationTests | Point count, input/output validation |
| **TestContext.GridFacade** | [x] | PCGExFilterIntegrationTests | 2D/3D grid positioning |
| **TestContext.RandomFacade** | [x] | PCGExFilterIntegrationTests | Seed reproducibility |
| **Facade.AttributeBuffers** | [x] | PCGExFilterIntegrationTests | Create/read/write buffers |
| **Facade.PointProperties** | [x] | PCGExFilterIntegrationTests | Transform access |
| **PointIO.Creation** | [x] | PCGExFilterIntegrationTests | Output initialization |

### Performance Tests
Test files: `Tests/Performance/`

| Component | Test File | Description |
|-----------|-----------|-------------|
| OBBCollection.LargeDataset | PCGExPerformanceTests | 10K boxes, point queries, overlap queries |
| OBBCollection.BulkClassify | PCGExPerformanceTests | 50K points classified against 1K boxes |
| Delaunay3D.LargePointSet | PCGExPerformanceTests | 2K point tetrahedralization |
| Voronoi3D.LargePointSet | PCGExPerformanceTests | 1.5K point 3D Voronoi diagram |
| ClusterStructs.LargeGraph | PCGExPerformanceTests | 10K nodes, random edge connectivity, adjacency queries |
| ClusterStructs.EdgeHashing | PCGExPerformanceTests | 100K edge hash operations and lookups |
| IndexLookup.LargeDataset | PCGExPerformanceTests | 1M random access operations |
| Memory.OBBCollectionGrowth | PCGExPerformanceTests | Reserve vs grow, reset/reuse cycles |
| MixedOperations.InterleavedQueries | PCGExPerformanceTests | Interleaved point/overlap/segment queries |

---

## Change Log

| Date | Changes |
|------|---------|
| 2026-02-03 | Initial tracking document |
| 2026-02-03 | Added math utility tests (TruncateDbl, FClosestPosition, FSegment, etc.) |
| 2026-02-03 | Added PCGExConstantFilter tests |
| 2026-02-03 | Added sharded container tests (TH64SetShards, TH64MapShards) |
| 2026-02-03 | Reorganized tiers based on actual usage priorities |
| 2026-02-03 | Added PCGExMathAxis tests (GetAxesOrder, GetDirection, Swizzle, angle functions) |
| 2026-02-03 | Added PCGExMathMean tests (GetAverage, GetMedian, QuickSelect) |
| 2026-02-03 | Added PCGExWinding tests (IsWinded, FPolygonInfos, AngleCCW) |
| 2026-02-03 | Added PCGExArrayHelpers tests (string parsing, reverse, reorder, indices) |
| 2026-02-03 | Added PCGExRandomHelpers tests (FastRand01, ComputeSpatialSeed) |
| 2026-02-03 | Added PCGExBestFitPlane tests (plane fitting, centroid, normal, extents) |
| 2026-02-03 | Added PCGExGeo tests (Det, Centroid, Circumcenter, Barycentric, PointInTriangle/Polygon, L-inf transforms, edge paths, sphere fitting) |
| 2026-02-03 | Added PCGExCompare tests (all numeric comparisons, string comparisons, vector/transform comparisons) |
| 2026-02-03 | Added PCGExFilterLogic tests (BooleanCompare, WithinRange, ModuloCompare logic simulation) |
| 2026-02-03 | Added PCGExTypeOpsNumeric tests (bool, int32, float, double operations; conversions, blends, hash) |
| 2026-02-03 | Added PCGExTypeOpsVector tests (FVector2D, FVector, FVector4 operations; conversions, blends, modulo) |
| 2026-02-03 | Added PCGExTypeOpsRotation tests (FRotator, FQuat, FTransform operations; conversions, blends, modulo, hash) |
| 2026-02-03 | Added PCGExTypeOpsString tests (FString, FName, FSoftObjectPath, FSoftClassPath operations; conversions, blends) |
| 2026-02-03 | Added PCGExSortingHelpers tests (FVectorKey comparison/sorting, RadixSort for FIndexKey arrays) |
| 2026-02-03 | Added PCGExMathBounds tests (SanitizeBounds function, EPCGExBoxCheckMode enum) |
| 2026-02-03 | Added PCGExMetaHelpers tests (IsPCGExAttribute, MakePCGExAttributeName, IsWritableAttributeName, IsDataDomainAttribute, StripDomainFromName, GetPropertyType, GetPropertyNativeTypes) |
| 2026-02-03 | Added PCGExTypeTraits tests (TTraits<T> for all types; Type, TypeId, feature flags for bool, int32, int64, float, double, FVector2D, FVector, FVector4, FRotator, FQuat, FTransform, FString, FName, FSoftObjectPath, FSoftClassPath) |
| 2026-02-03 | Added PCGExCollectionCache tests (FCategory, FMicroCache, FCache, FPCGExEntryAccessResult, FPCGExCollectionTypeSet, TypeIds, EPCGExIndexPickMode, EPCGExDistribution, EPCGExAssetTagInheritance) |
| 2026-02-03 | Added PCGExCollectionEntry tests (FPCGExAssetStagingData FindSocket, FPCGExAssetCollectionEntry type/validation, staging details defaults) |
| 2026-02-04 | Added PCGExOBBIntersections tests (FCut, FIntersections container, SegmentBoxRaw, ProcessSegment, SegmentIntersects, EPCGExCutType) |
| 2026-02-04 | Extended PCGExOBB tests with TestOverlap modes, FPolicy runtime class |
| 2026-02-04 | Added PCGExDelaunay tests (FDelaunaySite2, FDelaunaySite3, TDelaunay2::Process, TDelaunay3::Process, hull detection, RemoveLongestEdges) |
| 2026-02-04 | Added PCGExVoronoi tests (TVoronoi2, TVoronoi3, metrics, cell center methods, enums) |
| 2026-02-04 | Added PCGExMathDistances tests (GetDistances factory, IDistances interface, Euclidean/Manhattan/Chebyshev metrics) |
| 2026-02-04 | Added PCGExOBBSampling tests (FSample struct, Sample, SampleFast, SampleWithWeight, UVW computation, weight functions) |
| 2026-02-04 | Added PCGExOBBCollection tests (FCollection construction, Add, BuildOctree, point queries, OBB queries, segment intersections, bulk ops) |
| 2026-02-04 | Added PCGExTypes tests (FScopedTypedValue with all supported types, lifecycle management, convenience functions) |
| 2026-02-04 | Added PCGExClusterStructs tests (FLink, FEdge, FNode basic functionality, NodeGUID, edge direction enums) |
| 2026-02-04 | Added Performance/Stress tests (OBBCollection large datasets, Delaunay/Voronoi scaling, cluster graph stress, edge hashing, index lookup, memory patterns) |
| 2026-02-04 | Added PCGExBitmask tests (EPCGExBitOp Set/AND/OR/NOT/XOR, EPCGExBitflagComparison all modes, GetBitOp conversion, FPCGExClampedBitOp::Mutate, FPCGExSimpleBitmask::Mutate, flag patterns) |
| 2026-02-04 | Added PCGExDotFilterLogic tests (scalar/degrees domain, degree-to-dot conversion, unsigned comparison, vector dot products, practical scenarios) |
| 2026-02-04 | Added PCGExNumericCompareLogic tests (all comparison ops, tolerance handling, edge cases, integer-like values, practical scenarios) |
| 2026-02-04 | **Added FTestContext infrastructure** - Full PCG context for integration testing (FPCGExContext, FPointIO, FFacade creation, world/actor/PCGComponent setup) |
| 2026-02-04 | Added FScopedTestContext RAII wrapper for automatic initialization/cleanup |
| 2026-02-04 | Updated FTestFixture to use FTestContext internally, added CreateGridFacade/CreateRandomFacade |
| 2026-02-04 | Added integration tests for TestContext, Facade, and PointIO (PCGExFilterIntegrationTests) |
