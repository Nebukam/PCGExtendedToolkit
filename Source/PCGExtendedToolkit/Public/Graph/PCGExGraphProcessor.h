// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGExPointsProcessor.h"
#include "PCGExGraphHelpers.h"
#include "PCGExGraphProcessor.generated.h"

class UPCGExGraphParamsData;

namespace PCGExGraph
{
	struct PCGEXTENDEDTOOLKIT_API FPointCandidate
	{
		FPointCandidate()
		{
		}

		double Distance = 0;
		double Dot = 0;

		int32 Index = -1;
		PCGMetadataEntryKey EntryKey = PCGInvalidEntryKey;
	};

	/** Per-socket temp data structure for processing only*/
	struct PCGEXTENDEDTOOLKIT_API FSocketProbe : FPCGExSocketAngle
	{
		FSocketProbe()
		{
			Candidates.Empty();
		}

	public:
		FSocketInfos* SocketInfos = nullptr;
		FVector Origin = FVector::Zero();

		int32 BestIndex = -1;
		PCGMetadataEntryKey BestEntryKey = PCGInvalidEntryKey;

		TArray<FPointCandidate> Candidates;

		double IndexedRating = TNumericLimits<double>::Max();
		double IndexedDistanceRating = 0;
		double IndexedDotRating = 0;
		double IndexedDotWeight = 0;

		double ProbedDistanceMax = 0;
		double ProbedDistanceMin = TNumericLimits<double>::Max();
		double ProbedDotMax = 0;
		double ProbedDotMin = TNumericLimits<double>::Max();


		bool ProcessPoint(const FPCGPoint* Point, int32 Index)
		{
			const FVector PtPosition = Point->Transform.GetLocation();
			const double Dot = Direction.Dot((PtPosition - Origin).GetSafeNormal());

			if (Dot < DotThreshold) { return false; }
			const double PtDistance = FVector::DistSquared(Origin, PtPosition);

			if (PtDistance > MaxDistance) { return false; }

			ProbedDistanceMin = FMath::Min(ProbedDistanceMin, PtDistance);
			ProbedDistanceMax = FMath::Max(ProbedDistanceMax, PtDistance);
			ProbedDotMin = FMath::Min(ProbedDotMin, Dot);
			ProbedDotMax = FMath::Max(ProbedDotMax, Dot);

			FPointCandidate& Candidate = Candidates.Emplace_GetRef();

			Candidate.Dot = Dot;
			Candidate.Distance = PtDistance;

			Candidate.Index = Index;
			Candidate.EntryKey = Point->MetadataEntry;

			return true;
		}

		void ProcessCandidates()
		{
			for (const FPointCandidate& Candidate : Candidates)
			{
				const double DotRating = 1-PCGEx::Maths::Remap(Candidate.Dot, ProbedDotMin, ProbedDotMax);
				const double DistanceRating = PCGEx::Maths::Remap(Candidate.Distance, ProbedDistanceMin, ProbedDistanceMax);
				const double DotWeight = FMathf::Clamp(DotOverDistanceCurve->GetFloatValue(DistanceRating), 0, 1);
				const double Rating = (DotRating * DotWeight) + (DistanceRating * (1 - DotWeight));

				bool bBetterCandidate = false;
				if (Rating < IndexedRating || BestIndex == -1)
				{
					bBetterCandidate = true;
				}
				else if (Rating == IndexedRating)
				{
					if (DotWeight > IndexedDotWeight)
					{
						if (DotRating < IndexedDotRating ||
							(DotRating == IndexedDotRating && DistanceRating < IndexedDistanceRating))
						{
							bBetterCandidate = true;
						}
					}
					else
					{
						if (DistanceRating < IndexedDistanceRating ||
							(DistanceRating == IndexedRating && DotRating < IndexedDotRating))
						{
							bBetterCandidate = true;
						}
					}
				}

				if (bBetterCandidate)
				{
					IndexedRating = Rating;
					IndexedDistanceRating = DistanceRating;
					IndexedDotRating = DotRating;
					IndexedDotWeight = DotWeight;

					BestIndex = Candidate.Index;
					BestEntryKey = Candidate.EntryKey;
				}
			}
		}

		void OutputTo(PCGMetadataEntryKey Key) const
		{
			SocketInfos->Socket->SetTargetIndex(Key, BestIndex);
			SocketInfos->Socket->SetTargetEntryKey(Key, BestEntryKey);
		}

		~FSocketProbe()
		{
			SocketInfos = nullptr;
		}
	};
}

/**
 * A Base node to process a set of point using GraphParams.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExGraphProcessorSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(GraphProcessorSettings, "Graph Processor Settings", "TOOLTIP_TEXT");
	virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor(80.0f / 255.0f, 241.0f / 255.0f, 168.0f / 255.0f, 1.0f); }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

	//virtual  bool GetPinExtraIcon(const UPCGPin* InPin, FName& OutExtraIcon, FText& OutTooltip) const override;
	//~End UPCGSettings interface
};

struct PCGEXTENDEDTOOLKIT_API FPCGExGraphProcessorContext : public FPCGExPointsProcessorContext
{
	friend class UPCGExGraphProcessorSettings;

public:
	PCGExGraph::FParamsInputs Params;

	int32 GetCurrentParamsIndex() const { return CurrentParamsIndex; };
	UPCGExGraphParamsData* CurrentGraph = nullptr;

	bool AdvanceGraph(bool bResetPointsIndex = false);
	bool AdvancePointsIO(bool bResetParamsIndex = false);

	virtual void Reset() override;

	FPCGMetadataAttribute<int64>* CachedIndex;
	TArray<PCGExGraph::FSocketInfos> SocketInfos;

	void ComputeEdgeType(const FPCGPoint& Point, int32 ReadIndex, const UPCGExPointIO* IO);
	double PrepareProbesForPoint(const FPCGPoint& Point, TArray<PCGExGraph::FSocketProbe>& OutProbes);

	void OutputParams() { Params.OutputTo(this); }

	void OutputPointsAndParams()
	{
		OutputPoints();
		OutputParams();
	}

protected:
	int32 CurrentParamsIndex = -1;
	virtual void PrepareProbeForPointSocketPair(const FPCGPoint& Point, PCGExGraph::FSocketProbe& Probe, PCGExGraph::FSocketInfos SocketInfos);
};

class PCGEXTENDEDTOOLKIT_API FPCGExGraphProcessorElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual PCGEx::EIOInit GetPointOutputInitMode() const { return PCGEx::EIOInit::DuplicateInput; }
	virtual bool Validate(FPCGContext* InContext) const override;
	virtual void InitializeContext(
		FPCGExPointsProcessorContext* InContext,
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) const override;
};
