// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOperation.h"
#include "Data/PCGExGraphDefinition.h"
#include "UObject/Object.h"

#include "PCGExCustomGraphSolver.generated.h"

namespace PCGExGraph
{
	struct PCGEXTENDEDTOOLKIT_API FPointCandidate
	{
		FPointCandidate()
		{
		}

		double Distance = TNumericLimits<double>::Max();
		double Dot = -1;
		int32 Index = -1;
	};

	/** Per-socket temp data structure for processing only*/
	struct PCGEXTENDEDTOOLKIT_API FSocketProbe
	{
		FSocketProbe(): SocketInfos(nullptr)
		{
			Candidates.Empty();
		}

		explicit FSocketProbe(const FSocketInfos* InSocketInfos)
			: SocketInfos(InSocketInfos)
		{
			Candidates.Empty();
		}

		const FSocketInfos* SocketInfos;

		FVector Origin = FVector::Zero();
		FVector Direction = FVector::UpVector;
		double DotThreshold = 0.707;
		double Radius = 100.0f;
		TObjectPtr<UCurveFloat> DotOverDistanceCurve = nullptr;

		TArray<FPointCandidate> Candidates;
		FPointCandidate BestCandidate;

		FBox CompoundBounds;

		double IndexedRating = TNumericLimits<double>::Max();
		double IndexedDistanceRating = 0;
		double IndexedDotRating = 0;
		double IndexedDotWeight = 0;

		double ProbedDistanceMax = 0;
		double ProbedDistanceMin = TNumericLimits<double>::Max();
		double ProbedDotMax = 0;
		double ProbedDotMin = TNumericLimits<double>::Max();

		void OutputTo(const int32 Index) const
		{
			SocketInfos->Socket->SetTargetIndex(Index, BestCandidate.Index);
		}

		void Cleanup()
		{
			Candidates.Empty();
			SocketInfos = nullptr;
			DotOverDistanceCurve = nullptr;
		}

		FVector GetTargetCenter(const FPCGPoint& TargetPoint) const
		{
			return SocketInfos->Socket->Descriptor.DistanceSettings.GetTargetCenter(
				TargetPoint, TargetPoint.Transform.GetLocation(), Origin);
		}

		~FSocketProbe()
		{
			Cleanup();
		}
	};
}

/**
 * 
 */
UCLASS(DisplayName = "Simple Solver", meta=(ToolTip ="Simple graph solver."))
class PCGEXTENDEDTOOLKIT_API UPCGExCustomGraphSolver : public UPCGExOperation
{
	GENERATED_BODY()

public:
	virtual void InitializeProbe(PCGExGraph::FSocketProbe& Probe) const;
	virtual bool ProcessPoint(PCGExGraph::FSocketProbe& Probe, const PCGEx::FPointRef& Point) const;
	virtual void ResolveProbe(PCGExGraph::FSocketProbe& Probe) const;

	virtual double PrepareProbesForPoint(const TArray<PCGExGraph::FSocketInfos>& SocketInfos, const PCGEx::FPointRef& Point, TArray<PCGExGraph::FSocketProbe>& OutProbes) const;
	virtual double PrepareProbeForPointSocketPair(const PCGEx::FPointRef& Point, PCGExGraph::FSocketProbe& Probe, const PCGExGraph::FSocketInfos& InSocketInfos) const;
};
