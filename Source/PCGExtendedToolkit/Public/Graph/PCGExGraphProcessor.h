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
	/** Per-socket temp data structure for processing only*/
	struct PCGEXTENDEDTOOLKIT_API FSocketProbe : FPCGExSocketAngle
	{
		FSocketProbe()
		{
		}

	public:
		FSocketInfos* SocketInfos = nullptr;
		FVector Origin = FVector::Zero();
		int32 Index = -1;
		PCGMetadataEntryKey EntryKey = PCGInvalidEntryKey;
		double IndexedDistance = TNumericLimits<double>::Max();
		double IndexedDot = -1;

		bool ProcessPoint(const FPCGPoint* Point)
		{
			const FVector PtPosition = Point->Transform.GetLocation();
			const FVector DirToPt = (PtPosition - Origin).GetSafeNormal();

			const double SquaredDistance = FVector::DistSquared(Origin, PtPosition);

			// Is distance smaller than last registered one?
			if (SquaredDistance > IndexedDistance) { return false; }

			//UE_LOG(LogTemp, Warning, TEXT("Dist %f / %f "), SquaredDistance, MaxDistance * MaxDistance)
			// Is distance inside threshold?
			if (SquaredDistance >= (MaxDistance * MaxDistance)) { return false; }

			const double Dot = Direction.Dot(DirToPt);

			// Is dot within tolerance?
			if (Dot < DotTolerance) { return false; }

			if (IndexedDistance == SquaredDistance)
			{
				// In case of distance equality, favor candidate closer to dot == 1
				if (Dot < IndexedDot) { return false; }
			}

			IndexedDistance = SquaredDistance;
			IndexedDot = Dot;

			return true;
		}

		void OutputTo(PCGMetadataEntryKey Key) const
		{
			SocketInfos->Socket->SetTargetIndex(Key, Index);
			SocketInfos->Socket->SetTargetEntryKey(Key, EntryKey);
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
	//~End UPCGSettings interface
};

struct PCGEXTENDEDTOOLKIT_API FPCGExGraphProcessorContext : public FPCGExPointsProcessorContext
{
	friend class UPCGExGraphProcessorSettings;

public:
	PCGExGraph::FParamsInputs Params;

	int32 GetCurrentParamsIndex() const { return CurrentParamsIndex; };
	UPCGExGraphParamsData* CurrentParams = nullptr;

	bool AdvanceParams(bool bResetPointsIndex = false);
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
	virtual  bool Validate(FPCGContext* InContext) const override;
	virtual void InitializeContext(
		FPCGExPointsProcessorContext* InContext,
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) const override;
};
