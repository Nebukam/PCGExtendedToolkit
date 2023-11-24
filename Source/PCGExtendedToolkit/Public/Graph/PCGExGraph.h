// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExLocalAttributeHelpers.h"
#include "PCGExGraph.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExTangentParams
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Arrive Tangent")
	FName ArriveTangentAttribute = "ArriveTangent";

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

	void CreateAttributes(
		const UPCGPointData* InData,
		const FPCGPoint& Start,
		const FPCGPoint& End,
		FVector& StartIn,
		FVector& StartOut,
		FVector& EndIn,
		FVector& EndOut)
	{
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

namespace PCGExGraph
{

	const FName SourceParamsLabel = TEXT("GraphParams");
	const FName OutputParamsLabel = TEXT("→");
	const FName OutputPatchesLabel = TEXT("Patches");
	
}
