// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGExAttributeHelpers.h"

#include "PCGExGraph.generated.h"

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

ENUM_CLASS_FLAGS(EPCGExEdgeType)

namespace PCGExGraph
{
	const FName SourceParamsLabel = TEXT("GraphParams");
	const FName OutputParamsLabel = TEXT("→");
	const FName OutputPatchesLabel = TEXT("Patches");

	struct PCGEXTENDEDTOOLKIT_API FEdge
	{
		uint32 Start = 0;
		uint32 End = 0;
		EPCGExEdgeType Type = EPCGExEdgeType::Unknown;

		FEdge()
		{
		}

		FEdge(const int32 InStart, const int32 InEnd, const EPCGExEdgeType InType):
			Start(InStart), End(InEnd), Type(InType)
		{
		}

		bool operator==(const FEdge& Other) const
		{
			return Start == Other.Start && End == Other.End;
		}

		explicit operator uint64() const
		{
			return (static_cast<uint64>(Start) << 32) | End;
		}

		explicit FEdge(const uint64 InValue)
		{
			Start = static_cast<uint32>((InValue >> 32) & 0xFFFFFFFF);
			End = static_cast<uint32>(InValue & 0xFFFFFFFF);
			// You might need to set a default value for Type based on your requirements.
			Type = EPCGExEdgeType::Unknown;
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FUnsignedEdge : public FEdge
	{
		FUnsignedEdge(): FEdge()
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
			Start = static_cast<uint32>((InValue >> 32) & 0xFFFFFFFF);
			End = static_cast<uint32>(InValue & 0xFFFFFFFF);
			// You might need to set a default value for Type based on your requirements.
			Type = EPCGExEdgeType::Unknown;
		}

		uint64 GetUnsignedHash() const
		{
			return Start > End ?
				       (static_cast<uint64>(Start) << 32) | End :
				       (static_cast<uint64>(End) << 32) | Start;
		}
	};
}

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExTangentParams
{
	GENERATED_BODY()

	FPCGExTangentParams()
	{
		ArriveDirection.Selector.Update("$Transform");
		ArriveDirection.Axis = EPCGExAxis::Forward;
		LeaveDirection.Selector.Update("$Transform");
		LeaveDirection.Axis = EPCGExAxis::Forward;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Arrive Tangent")
	FName ArriveTangentAttribute = "ArriveTangent";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Arrive Tangent")
	FPCGExInputDescriptorWithDirection ArriveDirection;
	PCGEx::FLocalDirectionInput LocalArriveDirection;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Arrive Tangent", meta=(InlineEditConditionToggle))
	bool bUseLocalArriveIn = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Arrive Tangent", meta=(EditCondition="bUseLocalArriveIn"))
	FPCGExInputDescriptorWithSingleField ArriveInScale;
	PCGEx::FLocalSingleComponentInput LocalArriveInScale;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Arrive Tangent", meta=(InlineEditConditionToggle))
	bool bUseLocalArriveOut = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Arrive Tangent", meta=(EditCondition="bUseLocalArriveOut"))
	FPCGExInputDescriptorWithSingleField ArriveOutScale;
	PCGEx::FLocalSingleComponentInput LocalArriveOutScale;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Arrive Tangent")
	double DefaultArriveScale = 10;


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Leave Tangent")
	FName LeaveTangentAttribute = "LeaveTangent";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Leave Tangent")
	FPCGExInputDescriptorWithDirection LeaveDirection;
	PCGEx::FLocalDirectionInput LocalLeaveDirection;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Leave Tangent", meta=(InlineEditConditionToggle))
	bool bUseLocalLeaveIn = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Leave Tangent", meta=(EditCondition="bUseLocalLeaveIn"))
	FPCGExInputDescriptorWithSingleField LeaveInScale;
	PCGEx::FLocalSingleComponentInput LocalLeaveInScale;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Leave Tangent", meta=(InlineEditConditionToggle))
	bool bUseLocalLeaveOut = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Leave Tangent", meta=(EditCondition="bUseLocalLeaveOut"))
	FPCGExInputDescriptorWithSingleField LeaveOutScale;
	PCGEx::FLocalSingleComponentInput LocalLeaveOutScale;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Leave Tangent")
	double DefaultLeaveScale = 10;

	void PrepareForData(const UPCGPointData* InData)
	{
		
		LocalArriveDirection.Capture(ArriveDirection);
		LocalArriveDirection.Validate(InData);

		LocalLeaveDirection.Capture(LeaveDirection);
		LocalLeaveDirection.Validate(InData);

		if (bUseLocalArriveIn)
		{
			LocalArriveInScale.bEnabled = true;
			LocalArriveInScale.Capture(ArriveInScale);
			LocalArriveInScale.Validate(InData);
		}
		else { LocalArriveInScale.bEnabled = false; }

		if (bUseLocalArriveOut)
		{
			LocalArriveOutScale.bEnabled = true;
			LocalArriveOutScale.Capture(ArriveOutScale);
			LocalArriveOutScale.Validate(InData);
		}
		else { LocalArriveOutScale.bEnabled = false; }

		if (bUseLocalLeaveIn)
		{
			LocalLeaveInScale.bEnabled = true;
			LocalLeaveInScale.Capture(LeaveInScale);
			LocalLeaveInScale.Validate(InData);
		}
		else { LocalLeaveInScale.bEnabled = false; }

		if (bUseLocalLeaveOut)
		{
			LocalLeaveOutScale.bEnabled = true;
			LocalLeaveOutScale.Capture(LeaveOutScale);
			LocalLeaveOutScale.Validate(InData);
		}
		else { LocalLeaveOutScale.bEnabled = false; }
	}

	void ComputeTangents(
		const UPCGPointData* InData,
		const FPCGPoint& Start,
		const FPCGPoint& End) const
	{

		FVector StartIn = LocalArriveDirection.GetValue(Start);
		FVector StartOut = StartIn * -1;
		FVector EndIn = LocalLeaveDirection.GetValue(End);
		FVector EndOut = EndIn * -1;
		
		FPCGMetadataAttribute<FVector>* ArriveTangent = InData->Metadata->FindOrCreateAttribute<FVector>(ArriveTangentAttribute, FVector::ZeroVector);
		FPCGMetadataAttribute<FVector>* LeaveTangent = InData->Metadata->FindOrCreateAttribute<FVector>(LeaveTangentAttribute, FVector::ZeroVector);

		ScaleArriveIn(Start, StartIn);
		ArriveTangent->SetValue(Start.MetadataEntry, StartIn);

		ScaleArriveOut(Start, StartOut);
		LeaveTangent->SetValue(Start.MetadataEntry, StartOut);

		ScaleLeaveOut(End, EndOut);
		LeaveTangent->SetValue(End.MetadataEntry, EndOut);

		ScaleLeaveIn(End, EndIn);
		ArriveTangent->SetValue(End.MetadataEntry, EndIn);
	}

	void ScaleArriveIn(const FPCGPoint& Point, FVector& Tangent) const { Tangent *= LocalArriveInScale.GetValueSafe(Point, DefaultArriveScale); }
	void ScaleArriveOut(const FPCGPoint& Point, FVector& Tangent) const { Tangent *= LocalArriveOutScale.GetValueSafe(Point, DefaultArriveScale); }
	void ScaleLeaveIn(const FPCGPoint& Point, FVector& Tangent) const { Tangent *= LocalLeaveInScale.GetValueSafe(Point, DefaultLeaveScale); }
	void ScaleLeaveOut(const FPCGPoint& Point, FVector& Tangent) const { Tangent *= LocalLeaveOutScale.GetValueSafe(Point, DefaultLeaveScale); }
};
