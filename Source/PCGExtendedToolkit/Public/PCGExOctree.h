// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#ifndef PCGEX_OCTREE_MACROS
#define PCGEX_OCTREE_MACROS

#define PCGEX_OCTREE_SEMANTICS(_ITEM, _BOUNDS, _EQUALITY)\
struct _ITEM##Semantics{ \
enum { MaxElementsPerLeaf = 16 }; \
enum { MinInclusiveElementsPerNode = 7 }; \
enum { MaxNodeDepth = 12 }; \
using ElementAllocator = TInlineAllocator<MaxElementsPerLeaf>; \
static const FBoxSphereBounds& GetBoundingBox(const _ITEM* Element) _BOUNDS \
static const bool AreElementsEqual(const _ITEM* A, const _ITEM* B) _EQUALITY \
static void ApplyOffset(_ITEM& Element){ ensureMsgf(false, TEXT("Not implemented")); } \
static void SetElementId(const _ITEM* Element, FOctreeElementId2 OctreeElementID){ }}; \
using _ITEM##Octree = TOctree2<_ITEM*, _ITEM##Semantics>;

#define PCGEX_OCTREE_SEMANTICS_REF(_ITEM, _BOUNDS, _EQUALITY)\
struct PCGEXTENDEDTOOLKIT_API _ITEM##Semantics{ \
enum { MaxElementsPerLeaf = 16 }; \
enum { MinInclusiveElementsPerNode = 7 }; \
enum { MaxNodeDepth = 12 }; \
using ElementAllocator = TInlineAllocator<MaxElementsPerLeaf>; \
static const FBoxSphereBounds& GetBoundingBox(const _ITEM& Element) _BOUNDS \
static const bool AreElementsEqual(const _ITEM& A, const _ITEM& B) _EQUALITY \
static void ApplyOffset(_ITEM& Element){ ensureMsgf(false, TEXT("Not implemented")); } \
static void SetElementId(const _ITEM& Element, FOctreeElementId2 OctreeElementID){ }}; \
using _ITEM##Octree = TOctree2<_ITEM, _ITEM##Semantics>;

#endif

namespace PCGExOctree
{
	struct PCGEXTENDEDTOOLKIT_API FItem
	{
		int32 Index;
		FBoxSphereBounds Bounds;

		FItem(const int32 InIndex, const FBoxSphereBounds& InBounds)
			: Index(InIndex), Bounds(InBounds)
		{
		}
	};

	PCGEX_OCTREE_SEMANTICS_REF(FItem, { return Element.Bounds;}, { return A.Index == B.Index; })
}
