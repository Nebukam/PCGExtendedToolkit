// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGPolyLineData.h"
#include "UObject/Object.h"

namespace PCGExData
{
	namespace PolyLine
	{
		struct PCGEXTENDEDTOOLKIT_API FSegment
		{
			FSegment(const UPCGPolyLineData& InData, const int32 SegmentIndex)
				: PolyLine(&InData),
				  Segment(SegmentIndex),
				  Length(InData.GetSegmentLength(Segment)),
				  Start(InData.GetLocationAtDistance(Segment, 0))
			{
				End = InData.GetLocationAtDistance(Segment, Length);
				Bounds += Start;
				Bounds += End;
			}

			~FSegment()
			{
				PolyLine = nullptr;
			}

			const UPCGPolyLineData* PolyLine;
			int32 Segment = -1;
			double Length = 0;
			FVector Start;
			FVector End;
			double AccumulatedLength = 0;
			FBox Bounds = FBox(ForceInit);

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
	struct PCGEXTENDEDTOOLKIT_API FPolyLineIO
	{
		friend class FPolyLineIOGroup;

	protected:
		mutable FRWLock SegmentLock;

		TArray<PolyLine::FSegment> Segments;
		const UPCGPolyLineData* In;

	public:
		explicit FPolyLineIO(const UPCGPolyLineData& InPolyline);
		~FPolyLineIO();

		FPCGTaggedData Source; // Source struct		
		FBox Bounds;
		double TotalLength = 0;
		double TotalClosedLength = 0;

		PolyLine::FSegment* NearestSegment(const FVector& Location);
		PolyLine::FSegment* NearestSegment(const FVector& Location, const double Range);
		FTransform SampleNearestTransform(const FVector& Location, double& OutTime);
		bool SampleNearestTransform(const FVector& Location, const double Range, FTransform& OutTransform, double& OutTime);

	protected:
		void BuildCache();
	};

	/**
	 * 
	 */
	class PCGEXTENDEDTOOLKIT_API FPolyLineIOGroup
	{
	public:
		FPolyLineIOGroup();
		FPolyLineIOGroup(const FPCGContext* Context, FName InputLabel);
		explicit FPolyLineIOGroup(TArray<FPCGTaggedData>& Sources);

		~FPolyLineIOGroup();

		TArray<FPolyLineIO*> Lines;

		FPolyLineIO* Emplace_GetRef(const FPolyLineIO& PolyLine);

		FPolyLineIO* Emplace_GetRef(const FPCGTaggedData& Source, const UPCGPolyLineData* In);

		int32 Num() const { return Lines.Num(); }
		bool IsEmpty() const { return Lines.IsEmpty(); }

		bool SampleNearestTransform(const FVector& Location, FTransform& OutTransform, double& OutTime);
		bool SampleNearestTransformWithinRange(const FVector& Location, const double Range, FTransform& OutTransform, double& OutTime);

	protected:
		mutable FRWLock PairsLock;

		static UPCGPolyLineData* GetMutablePolyLineData(const UPCGSpatialData* InSpatialData);
		static UPCGPolyLineData* GetMutablePolyLineData(const FPCGTaggedData& Source);

		void Initialize(TArray<FPCGTaggedData>& Sources);

		void Initialize(
			TArray<FPCGTaggedData>& Sources,
			const TFunction<bool(UPCGPolyLineData*)>& ValidateFunc,
			const TFunction<void(FPolyLineIO*)>& PostInitFunc);
	};
}
