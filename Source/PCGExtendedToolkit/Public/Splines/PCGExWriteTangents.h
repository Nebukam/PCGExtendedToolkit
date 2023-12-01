// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "PCGExWriteTangents.generated.h"

namespace PCGExTangents
{
	struct PCGEXTENDEDTOOLKIT_API FPair
	{
		FVector Leave;
		FVector Arrive;
	};
}

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSingleTangentParams
{
	GENERATED_BODY()

	FPCGExSingleTangentParams()
	{
		Direction.Selector.Update("$Transform");
		Direction.Axis = EPCGExAxis::Backward;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName TangentName = "ArriveTangent";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExInputDescriptorWithDirection Direction;
	PCGEx::FLocalDirectionGetter DirectionGetter;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle))
	bool bUseLocalScale = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="bUseLocalScale"))
	FPCGExInputDescriptorWithSingleField LocalScale;
	PCGEx::FLocalSingleFieldGetter ScaleGetter;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	double DefaultScale = 10;

	FPCGMetadataAttribute<FVector>* Attribute = nullptr;

	void PrepareForData(const UPCGPointData* InData)
	{
		DirectionGetter.Capture(Direction);
		DirectionGetter.Validate(InData);

		if (bUseLocalScale)
		{
			ScaleGetter.bEnabled = true;
			ScaleGetter.Capture(LocalScale);
			ScaleGetter.Validate(InData);
		}
		else { ScaleGetter.bEnabled = false; }
		Attribute = InData->Metadata->FindOrCreateAttribute<FVector>(TangentName, FVector::ZeroVector);
	}

	FVector GetDirection(const FPCGPoint& Point) const { return DirectionGetter.GetValue(Point); }
	FVector GetTangent(const FPCGPoint& Point) const { return DirectionGetter.GetValue(Point) * ScaleGetter.GetValueSafe(Point, DefaultScale); }

	void SetValue(const FPCGPoint& Point, const FVector& InValue) const { Attribute->SetValue(Point.MetadataEntry, InValue); }
};


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExTangentParams
{
	GENERATED_BODY()

	FPCGExTangentParams()
	{
		Arrive.TangentName = "ArriveTangent";
		Leave.TangentName = "LeaveTangent";
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExSingleTangentParams Arrive;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Leave == Arrive"))
	bool bLeaveCopyArrive = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExSingleTangentParams Leave;

	void PrepareForData(const UPCGExPointIO* PointIO)
	{
		const UPCGPointData* InData = PointIO->Out;
		Arrive.PrepareForData(InData);
		Leave.PrepareForData(InData);
	}

	void ComputePointTangents(
		const int64 Index,
		const UPCGExPointIO* PointIO,
		TMap<int64, PCGExTangents::FPair>* Cache = nullptr) const
	{
		const FPCGPoint& Current = PointIO->GetOutPoint(Index);
		const FVector ArriveTangent = Arrive.GetTangent(Current);
		const FVector LeaveTangent = bLeaveCopyArrive ? ArriveTangent : Leave.GetTangent(Current);

		Leave.SetValue(Current, LeaveTangent);
		Arrive.SetValue(Current, ArriveTangent);

		if (Cache) { Cache->Emplace(Index, PCGExTangents::FPair(LeaveTangent, ArriveTangent)); }
	}

	void ComputeRelationalTangents(
		const int64 Index,
		const UPCGExPointIO* PointIO,
		TMap<int64, PCGExTangents::FPair>* Cache) const
	{
		// Note: ComputePointTangents should have been called on all points prior to using this method
		const FPCGPoint* PrevPtr = PointIO->TryGetOutPoint(Index - 1);
		const FPCGPoint& Current = PointIO->GetOutPoint(Index);
		const FPCGPoint* NextPtr = PointIO->TryGetOutPoint(Index + 1);


		const FVector Origin = Current.Transform.GetLocation();

		if (PrevPtr)
		{
			const FPCGPoint& Prev = *PrevPtr;
			const FVector OriginTangent = Cache->Find(Index)->Arrive;
			const FVector NewTangent = ComputeRelational(
				Origin, OriginTangent,
				Prev.Transform.GetLocation(),
				Cache->Find(Index - 1)->Leave);

			Arrive.SetValue(Current, NewTangent);
		}

		if (NextPtr)
		{
			const FPCGPoint& Next = *NextPtr;
			const FVector OriginTangent = Cache->Find(Index)->Leave;
			const FVector NewTangent = ComputeRelational(
				Origin, OriginTangent,
				Next.Transform.GetLocation(),
				Cache->Find(Index + 1)->Arrive);

			Leave.SetValue(Current, NewTangent);
		}
	}

protected:
	static FVector ComputeRelational(const FVector& Origin, const FVector& OriginTangent, const FVector& Other, const FVector& OtherTangent)
	{
		const FVector Lerped = FMath::Lerp(Origin + OriginTangent, Other + OtherTangent, 0.5);
		return Lerped - Origin;
	}
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
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorSpline; }
#endif

	virtual PCGExIO::EInitMode GetPointOutputInitMode() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(FullyExpand=true))
	FPCGExTangentParams TangentParams;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExWriteTangentsContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExWriteTangentsElement;

public:
	mutable FRWLock MapLock;
	TMap<int64, PCGExTangents::FPair> TangentCache;
	FPCGExTangentParams TangentParams;
};

class PCGEXTENDEDTOOLKIT_API FPCGExWriteTangentsElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
