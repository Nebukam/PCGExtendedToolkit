// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGPolyLineData.h"
#include "UObject/Object.h"

#include "PCGExPolyLineIO.generated.h"

namespace PCGExPolyLine
{
	struct PCGEXTENDEDTOOLKIT_API FSegment
	{
		UPCGPolyLineData* PolyLine;
		int32 Segment = -1;
		FVector Start;
		FVector End;
		double AccumulatedLength;
		double Length;
		FBox Bounds;

		FSegment(UPCGPolyLineData* InData, int32 SegmentIndex)
		{
			PolyLine = InData;
			Segment = SegmentIndex;
			Length = InData->GetSegmentLength(Segment);
			Start = InData->GetLocationAtDistance(Segment, 0);
			End = InData->GetLocationAtDistance(Segment, Length);
			Bounds = FBox(ForceInit);
			Bounds += Start;
			Bounds += End;
		}

		FVector NearestLocation(const FVector& Location) const { return FMath::ClosestPointOnSegment(Location, Start, End); }

		FTransform NearestTransform(const FVector& Location) const
		{
			const FVector Point = FMath::ClosestPointOnSegment(Location, Start, End);
			return PolyLine->GetTransformAtDistance(Segment, FVector::Distance(Start, Point));
		}

		double GetAccumulatedLengthAt(const FVector& Location) const
		{
			return AccumulatedLength + FVector::Distance(Start, Location);
		}
	};
}

/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExPolyLineIO : public UObject
{
	GENERATED_BODY()

	friend class UPCGExPolyLineIOGroup;

public:
	UPCGExPolyLineIO();

	FPCGTaggedData Source;          // Source struct
	UPCGPolyLineData* In = nullptr; // Input PointData
	FBox Bounds;
	double TotalLength;
	double TotalClosedLength;

	void Flush()
	{
		Segments.Empty();
	}

protected:
	mutable FRWLock SegmentLock;
	TArray<PCGExPolyLine::FSegment> Segments;
	bool bParallelProcessing = false;

public:
	PCGExPolyLine::FSegment* NearestSegment(const FVector& Location);
	PCGExPolyLine::FSegment* NearestSegment(const FVector& Location, const double Range);
	FTransform SampleNearestTransform(const FVector& Location, double& OutTime);
	bool SampleNearestTransform(const FVector& Location, const double Range, FTransform& OutTransform, double& OutTime);

	virtual ~UPCGExPolyLineIO() override
	{
		Segments.Empty();
		In = nullptr;
	}

protected:
	bool bCacheDirty = true;
	void BuildCache();
};

/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExPolyLineIOGroup : public UObject
{
	GENERATED_BODY()

public:
	UPCGExPolyLineIOGroup();
	UPCGExPolyLineIOGroup(const FPCGContext* Context, FName InputLabel);
	UPCGExPolyLineIOGroup(TArray<FPCGTaggedData>& Sources);

	TArray<UPCGExPolyLineIO*> PolyLines;

	/**
	 * Initialize from Sources
	 * @param Sources 
	 */
	void Initialize(TArray<FPCGTaggedData>& Sources);

	void Initialize(
		TArray<FPCGTaggedData>& Sources,
		const TFunction<bool(UPCGPolyLineData*)>& ValidateFunc,
		const TFunction<void(UPCGExPolyLineIO*)>& PostInitFunc);

	UPCGExPolyLineIO* Emplace_GetRef(const UPCGExPolyLineIO& PolyLine);

	UPCGExPolyLineIO* Emplace_GetRef(const FPCGTaggedData& Source, UPCGPolyLineData* In);

	bool IsEmpty() const { return PolyLines.IsEmpty(); }

	bool SampleNearestTransform(const FVector& Location, FTransform& OutTransform, double& OutTime);
	bool SampleNearestTransformWithinRange(const FVector& Location, const double Range, FTransform& OutTransform, double& OutTime);

	void Flush()
	{
		for (UPCGExPolyLineIO* Line : PolyLines) { Line->Flush(); }
		PolyLines.Empty();
	}

protected:
	mutable FRWLock PairsLock;
	bool bProcessing = false;
	UPCGPolyLineData* GetMutablePolyLineData(const UPCGSpatialData* InSpatialData);
	UPCGPolyLineData* GetMutablePolyLineData(const FPCGTaggedData& Source);
};
