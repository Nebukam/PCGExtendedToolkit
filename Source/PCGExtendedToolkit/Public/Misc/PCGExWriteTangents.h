// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "PCGExWriteTangents.generated.h"


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExTangentParams
{
	GENERATED_BODY()

	FPCGExTangentParams()
	{
		ArriveDirection.Selector.Update("$Transform");
		ArriveDirection.Axis = EPCGExAxis::Backward;
		LeaveDirection.Selector.Update("$Transform");
		LeaveDirection.Axis = EPCGExAxis::Forward;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bSmoothTangents = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Arrive Tangent")
	FName ArriveTangentName = "ArriveTangent";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Arrive Tangent")
	FPCGExInputDescriptorWithDirection ArriveDirection;
	PCGEx::FLocalDirectionInput LocalArriveDirection;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Arrive Tangent", meta=(InlineEditConditionToggle))
	bool bUseLocalArrive = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Arrive Tangent", meta=(EditCondition="bUseLocalArrive"))
	FPCGExInputDescriptorWithSingleField ArriveScale;
	PCGEx::FLocalSingleComponentInput LocalArriveScale;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Arrive Tangent")
	double DefaultArriveScale = 10;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Leave Tangent")
	FName LeaveTangentName = "LeaveTangent";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Leave Tangent")
	FPCGExInputDescriptorWithDirection LeaveDirection;
	PCGEx::FLocalDirectionInput LocalLeaveDirection;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Leave Tangent", meta=(InlineEditConditionToggle))
	bool bUseLocalLeave = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Leave Tangent", meta=(EditCondition="bUseLocalLeave"))
	FPCGExInputDescriptorWithSingleField LeaveScale;
	PCGEx::FLocalSingleComponentInput LocalLeaveScale;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Leave Tangent")
	double DefaultLeaveScale = 10;

	FPCGMetadataAttribute<FVector>* ArriveTangentAttribute = nullptr;
	FPCGMetadataAttribute<FVector>* LeaveTangentAttribute = nullptr;

	void PrepareForData(const UPCGExPointIO* PointIO)
	{
		const UPCGPointData* InData = PointIO->Out;

		LocalArriveDirection.Capture(ArriveDirection);
		LocalArriveDirection.Validate(InData);

		LocalLeaveDirection.Capture(LeaveDirection);
		LocalLeaveDirection.Validate(InData);

		if (bUseLocalArrive)
		{
			LocalArriveScale.bEnabled = true;
			LocalArriveScale.Capture(ArriveScale);
			LocalArriveScale.Validate(InData);
		}
		else { LocalArriveScale.bEnabled = false; }

		if (bUseLocalLeave)
		{
			LocalLeaveScale.bEnabled = true;
			LocalLeaveScale.Capture(LeaveScale);
			LocalLeaveScale.Validate(InData);
		}
		else { LocalLeaveScale.bEnabled = false; }

		ArriveTangentAttribute = PointIO->Out->Metadata->FindOrCreateAttribute<FVector>(ArriveTangentName, FVector::ZeroVector);
		LeaveTangentAttribute = PointIO->Out->Metadata->FindOrCreateAttribute<FVector>(LeaveTangentName, FVector::ZeroVector);
		
	}

	void ComputeTangentsFromData(
		const int64 Index,
		const UPCGExPointIO* PointIO) const
	{
		const FPCGPoint& Current = PointIO->GetOutPoint(Index);

		FVector LeaveTangent = LocalLeaveDirection.GetValue(Current);
		FVector ArriveTangent;

		ScaleLeave(Current, LeaveTangent);

		if (const FPCGPoint* Next = PointIO->TryGetOutPoint(Index + 1))
		{
			ArriveTangent = LocalArriveDirection.GetValue(*Next);
			ScaleArrive(*Next, ArriveTangent);
			ArriveTangentAttribute->SetValue(Next->MetadataEntry, ArriveTangent);
		}

		LeaveTangentAttribute->SetValue(Current.MetadataEntry, LeaveTangent);

		if (Index == 0)
		{
			// Is first point, need to take care of the arrive
			ArriveTangent = LocalArriveDirection.GetValue(Current);
			ScaleArrive(Current, ArriveTangent);
			ArriveTangentAttribute->SetValue(Current.MetadataEntry, ArriveTangent);
		}
	}

	void ScaleArrive(const FPCGPoint& Point, FVector& Tangent) const { Tangent *= LocalArriveScale.GetValueSafe(Point, DefaultArriveScale); }
	void ScaleLeave(const FPCGPoint& Point, FVector& Tangent) const { Tangent *= LocalLeaveScale.GetValueSafe(Point, DefaultLeaveScale); }
};

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExWriteTangentsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(WriteTangents, "Write Tangents", "Computes & writes points tangents.");
#endif

	virtual PCGExIO::EInitMode GetPointOutputInitMode() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ShowOnlyInnerProperties))
	FPCGExTangentParams TangentParams;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExWriteTangentsContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExWriteTangentsElement;

public:
	mutable FRWLock MapLock;
	FPCGExTangentParams TangentParams;
};

class PCGEXTENDEDTOOLKIT_API FPCGExWriteTangentsElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;
	virtual bool Validate(FPCGContext* InContext) const override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
