// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExGraphParamsData.h"

#include "PCGExGraphProcessor.generated.h"

namespace PCGExGraph
{
	struct PCGEXTENDEDTOOLKIT_API FGraphInputs
	{
		FGraphInputs()
		{
			Params.Empty();
			ParamsSources.Empty();
		}

		FGraphInputs(FPCGContext* Context, FName InputLabel): FGraphInputs()
		{
			TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(InputLabel);
			Initialize(Context, Sources);
		}

		FGraphInputs(FPCGContext* Context, TArray<FPCGTaggedData>& Sources): FGraphInputs()
		{
			Initialize(Context, Sources);
		}

	public:
		TArray<UPCGExGraphParamsData*> Params;
		TArray<FPCGTaggedData> ParamsSources;

		/**
		 * Initialize from Sources
		 * @param Context 
		 * @param Sources 
		 * @param bInitializeOutput 
		 */
		void Initialize(FPCGContext* Context, TArray<FPCGTaggedData>& Sources, bool bInitializeOutput = false)
		{
			Params.Empty(Sources.Num());
			TSet<uint64> UniqueParams;
			for (FPCGTaggedData& Source : Sources)
			{
				const UPCGExGraphParamsData* GraphData = Cast<UPCGExGraphParamsData>(Source.Data);
				if (!GraphData) { continue; }
				if (UniqueParams.Contains(GraphData->UID)) { continue; }
				UniqueParams.Add(GraphData->UID);
				Params.Add(const_cast<UPCGExGraphParamsData*>(GraphData));
				ParamsSources.Add(Source);
			}
			UniqueParams.Empty();
		}

		void ForEach(FPCGContext* Context, const TFunction<void(UPCGExGraphParamsData*, const int32)>& BodyLoop)
		{
			for (int i = 0; i < Params.Num(); i++)
			{
				UPCGExGraphParamsData* ParamsData = Params[i];
				BodyLoop(ParamsData, i);
			}
		}

		void OutputTo(FPCGContext* Context) const
		{
			for (int i = 0; i < ParamsSources.Num(); i++)
			{
				FPCGTaggedData& OutputRef = Context->OutputData.TaggedData.Add_GetRef(ParamsSources[i]);
				OutputRef.Pin = PCGExGraph::OutputParamsLabel;
				OutputRef.Data = Params[i];
			}
		}

		bool IsEmpty() const { return Params.IsEmpty(); }

		~FGraphInputs()
		{
			Params.Empty();
			ParamsSources.Empty();
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FPointCandidate
	{
		FPointCandidate()
		{
		}

		double Distance = TNumericLimits<double>::Max();
		double Dot = -1;

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

		TArray<FPointCandidate> Candidates;
		FPointCandidate BestCandidate;

		FBox LooseBounds;

		double IndexedRating = TNumericLimits<double>::Max();
		double IndexedDistanceRating = 0;
		double IndexedDotRating = 0;
		double IndexedDotWeight = 0;

		double ProbedDistanceMax = 0;
		double ProbedDistanceMin = TNumericLimits<double>::Max();
		double ProbedDotMax = 0;
		double ProbedDotMin = TNumericLimits<double>::Max();


		bool ProcessPointComplex(const FPCGPoint* Point, const int32 Index)
		{
			const FVector PtPosition = Point->Transform.GetLocation();

			if (!LooseBounds.IsInside(PtPosition)) { return false; }
			
			const double PtDistance = FVector::DistSquared(Origin, PtPosition);
			if (PtDistance > MaxDistance) { return false; }

			const double Dot = Direction.Dot((PtPosition - Origin).GetSafeNormal());
			if (Dot < DotThreshold) { return false; }

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

		bool ProcessPointSimple(const FPCGPoint* Point, const int32 Index)
		{
			const FVector PtPosition = Point->Transform.GetLocation();

			if (!LooseBounds.IsInside(PtPosition)) { return false; }

			const double PtDistance = FVector::DistSquared(Origin, PtPosition);
			if (PtDistance > MaxDistance) { return false; }
			if (PtDistance > BestCandidate.Distance) { return false; }

			const double Dot = Direction.Dot((PtPosition - Origin).GetSafeNormal());
			if (Dot < DotThreshold) { return false; }

			BestCandidate.Dot = Dot;
			BestCandidate.Distance = PtDistance;

			BestCandidate.Index = Index;
			BestCandidate.EntryKey = Point->MetadataEntry;

			return true;
		}

		void ProcessCandidates()
		{
			for (const FPointCandidate& Candidate : Candidates)
			{
				const double DotRating = 1 - PCGExMath::Remap(Candidate.Dot, ProbedDotMin, ProbedDotMax);
				const double DistanceRating = PCGExMath::Remap(Candidate.Distance, ProbedDistanceMin, ProbedDistanceMax);
				const double DotWeight = FMathf::Clamp(DotOverDistanceCurve->GetFloatValue(DistanceRating), 0, 1);
				const double Rating = (DotRating * DotWeight) + (DistanceRating * (1 - DotWeight));

				bool bBetterCandidate = false;
				if (Rating < IndexedRating || BestCandidate.Index == -1)
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

					BestCandidate.Index = Candidate.Index;
					BestCandidate.EntryKey = Candidate.EntryKey;
				}
			}
		}

		void OutputTo(PCGMetadataEntryKey Key) const
		{
			SocketInfos->Socket->SetTargetIndex(Key, BestCandidate.Index);
			SocketInfos->Socket->SetTargetEntryKey(Key, BestCandidate.EntryKey);
		}

		~FSocketProbe()
		{
			SocketInfos = nullptr;
		}
	};
}

class UPCGExGraphParamsData;

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
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorGraph; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings interface

	virtual FName GetMainPointsInputLabel() const override;
	virtual FName GetMainPointsOutputLabel() const override;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExGraphProcessorContext : public FPCGExPointsProcessorContext
{
	friend class UPCGExGraphProcessorSettings;

public:
	PCGExGraph::FGraphInputs Params;

	int32 GetCurrentParamsIndex() const { return CurrentParamsIndex; };
	UPCGExGraphParamsData* CurrentGraph = nullptr;

	bool AdvanceGraph(bool bResetPointsIndex = false);
	bool AdvancePointsIO(bool bResetParamsIndex = false);

	virtual void Reset() override;

	FPCGMetadataAttribute<int64>* CachedIndex;
	TArray<PCGExGraph::FSocketInfos> SocketInfos;

	void ComputeEdgeType(const FPCGPoint& Point, int32 ReadIndex, const UPCGExPointIO* PointIO);
	double PrepareProbesForPoint(const FPCGPoint& Point, TArray<PCGExGraph::FSocketProbe>& OutProbes);

	void PrepareCurrentGraphForPoints(const UPCGPointData* InData, bool bEnsureEdgeType);

	void OutputGraphParams() { Params.OutputTo(this); }

	void OutputPointsAndParams()
	{
		OutputPoints();
		OutputGraphParams();
	}

protected:
	int32 CurrentParamsIndex = -1;
	virtual double PrepareProbeForPointSocketPair(const FPCGPoint& Point, PCGExGraph::FSocketProbe& Probe, PCGExGraph::FSocketInfos SocketInfos);
};

class PCGEXTENDEDTOOLKIT_API FPCGExGraphProcessorElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Validate(FPCGContext* InContext) const override;
	virtual void InitializeContext(
		FPCGExPointsProcessorContext* InContext,
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) const override;
};
