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
| **PCGExMathDistances.h** | [ ] | | |
| **PCGExMathAxis.h** | [x] | PCGExMathAxisTests | Axis order, direction, swizzle, angles |
| **PCGExMathBounds.h** | [~] | PCGExMathBoundsTests | SanitizeBounds, EPCGExBoxCheckMode enum |
| **PCGExMathMean.h** | [x] | PCGExMathMeanTests | Average, Median, QuickSelect |
| **PCGExWinding.h** | [x] | PCGExWindingTests | IsWinded, FPolygonInfos, AngleCCW |
| **PCGExBestFitPlane.h** | [x] | PCGExBestFitPlaneTests | Plane fitting, centroid, normal, extents |
| **PCGExDelaunay.h** | [ ] | | |
| **PCGExVoronoi.h** | [ ] | | |
| **PCGExGeo.h** | [x] | PCGExGeoTests | Det, Centroid, Circumcenter, Barycentric, PointInTriangle/Polygon, L-inf transforms, edge paths, sphere fitting |
| **PCGExOBB.h** | [~] | PCGExOBBTests | |
| **PCGExOBBCollection.h** | [ ] | | |
| **PCGExOBBSampling.h** | [ ] | | |
| **PCGExOBBIntersections.h** | [ ] | | |

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
| PCGExTypes.h | [ ] | |
| PCGExTypesCore.h | [ ] | |
| PCGExEnums.h | [ ] | |

#### Data (~31 headers)
| Component | Status | Notes |
|-----------|--------|-------|
| (All data structures) | [ ] | |

#### Other Core
| Component | Status | Notes |
|-----------|--------|-------|
| Clusters (~9 headers) | [ ] | |
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
| PCGExNumericCompareFilter.h | [ ] | | |
| PCGExStringCompareFilter.h | [ ] | | |
| **PCGExBooleanCompareFilter.h** | [~] | PCGExFilterLogicTests | Logic simulation (Equal/NotEqual) |
| PCGExDistanceFilter.h | [ ] | | |
| PCGExDotFilter.h | [ ] | | |
| PCGExAngleFilter.h | [ ] | | |
| PCGExAttributeCheckFilter.h | [ ] | | |
| PCGExBitmaskFilter.h | [ ] | | |
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

| Module | Headers | Status | Notes |
|--------|---------|--------|-------|
| PCGExCollections | ~26 | [ ] | |
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
| Component | Status | Location |
|-----------|--------|----------|
| FTestFixture | [x] | Fixtures/ |
| FPointDataBuilder | [x] | Helpers/ |
| PCGExTestHelpers | [x] | Helpers/ |
| PCGExPointDataHelpers | [x] | Helpers/ |

### Test Patterns Available
- Unit tests (IMPLEMENT_SIMPLE_AUTOMATION_TEST)
- BDD specs (BEGIN_DEFINE_SPEC)
- Functional tests (full context)

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
