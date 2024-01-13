// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExMT.h"

#include "PCGExEdge.generated.h"

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true"))
enum class EPCGExEdgeType : uint8
{
	Unknown  = 0 UMETA(DisplayName = "Unknown", Tooltip="Unknown type."),
	Roaming  = 1 << 0 UMETA(DisplayName = "Roaming", Tooltip="Unidirectional edge."),
	Shared   = 1 << 1 UMETA(DisplayName = "Shared", Tooltip="Shared edge, both sockets are connected; but do not match."),
	Match    = 1 << 2 UMETA(DisplayName = "Match", Tooltip="Shared relation, considered a match by the primary socket owner; but does not match on the other."),
	Complete = 1 << 3 UMETA(DisplayName = "Complete", Tooltip="Shared, matching relation on both sockets."),
	Mirror   = 1 << 4 UMETA(DisplayName = "Mirrored relation", Tooltip="Mirrored relation, connected sockets are the same on both points."),
};


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExDebugEdgeSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FColor ValidEdgeColor = FColor::Cyan;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin=0.1, ClampMax=10))
	double ValidEdgeThickness = 0.5;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FColor InvalidEdgeColor = FColor::Red;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin=0.1, ClampMax=10))
	double InvalidEdgeThickness = 0.5;
};

namespace PCGExGraph
{
	const FName SourceEdgesLabel = TEXT("Edges");
	const FName OutputEdgesLabel = TEXT("Edges");

	const FName EdgeStartAttributeName = TEXT("PCGEx/EdgeStart");
	const FName EdgeEndAttributeName = TEXT("PCGEx/EdgeEnd");
	const FName ClusterIndexAttributeName = TEXT("PCGEx/Cluster");

	constexpr PCGExMT::AsyncState State_ReadyForNextEdges = __COUNTER__;
	constexpr PCGExMT::AsyncState State_ProcessingEdges = __COUNTER__;

	static uint64 GetUnsignedHash64(const uint32 A, const uint32 B)
	{
		return A > B ?
			       static_cast<uint64>(A) | (static_cast<uint64>(B) << 32) :
			       static_cast<uint64>(B) | (static_cast<uint64>(A) << 32);
	}

	struct PCGEXTENDEDTOOLKIT_API FEdge
	{
		uint32 Start = 0;
		uint32 End = 0;
		EPCGExEdgeType Type = EPCGExEdgeType::Unknown;
		bool bValid = true;

		FEdge()
		{
		}

		FEdge(const int32 InStart, const int32 InEnd):
			Start(InStart), End(InEnd)
		{
			bValid = InStart != InEnd && InStart != -1 && InEnd != -1;
		}

		FEdge(const int32 InStart, const int32 InEnd, const EPCGExEdgeType InType)
			: FEdge(InStart, InEnd)
		{
			Type = InType;
		}

		bool Contains(const int32 InIndex) const { return Start == InIndex || End == InIndex; }

		uint32 Other(const int32 InIndex) const
		{
			check(InIndex == Start || InIndex == End)
			return InIndex == Start ? End : Start;
		}

		bool operator==(const FEdge& Other) const
		{
			return Start == Other.Start && End == Other.End;
		}

		explicit operator uint64() const
		{
			return static_cast<uint64>(Start) | (static_cast<uint64>(End) << 32);
		}

		explicit FEdge(const uint64 InValue)
		{
			Start = static_cast<uint32>(InValue & 0xFFFFFFFF);
			End = static_cast<uint32>((InValue >> 32) & 0xFFFFFFFF);
			Type = EPCGExEdgeType::Unknown;
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FUnsignedEdge : public FEdge
	{
		FUnsignedEdge()
		{
		}

		FUnsignedEdge(const int32 InStart, const int32 InEnd):
			FEdge(InStart, InEnd)
		{
		}

		FUnsignedEdge(const int32 InStart, const int32 InEnd, const EPCGExEdgeType InType):
			FEdge(InStart, InEnd, InType)
		{
		}

		bool operator==(const FUnsignedEdge& Other) const
		{
			return GetUnsignedHash() == Other.GetUnsignedHash();
		}

		explicit FUnsignedEdge(const uint64 InValue)
		{
			Start = static_cast<uint32>(InValue & 0xFFFFFFFF);
			End = static_cast<uint32>((InValue >> 32) & 0xFFFFFFFF);
			Type = EPCGExEdgeType::Unknown;
		}

		uint64 GetUnsignedHash() const { return GetUnsignedHash64(Start, End); }
	};

	struct PCGEXTENDEDTOOLKIT_API FIndexedEdge : public FUnsignedEdge
	{
		int32 Index = -1;

		FIndexedEdge()
		{
		}

		FIndexedEdge(const int32 InIndex, const int32 InStart, const int32 InEnd)
			: FUnsignedEdge(InStart, InEnd),
			  Index(InIndex)
		{
		}
	};
}
