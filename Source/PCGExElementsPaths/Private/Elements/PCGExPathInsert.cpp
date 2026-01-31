// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExPathInsert.h"

#include "PCGParamData.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExSettingsDetails.h"
#include "Helpers/PCGExRandomHelpers.h"
#include "Helpers/PCGExTargetsHandler.h"
#include "Paths/PCGExPath.h"
#include "Paths/PCGExPathsCommon.h"
#include "Paths/PCGExPathsHelpers.h"
#include "SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExPathInsertElement"
#define PCGEX_NAMESPACE PathInsert

#if WITH_EDITORONLY_DATA
void UPCGExPathInsertSettings::PostInitProperties()
{
	if (!HasAnyFlags(RF_ClassDefaultObject) && IsInGameThread())
	{
		if (!Blending) { Blending = NewObject<UPCGExSubPointsBlendInterpolate>(this, TEXT("Blending")); }
	}
	Super::PostInitProperties();
}
#endif

TArray<FPCGPinProperties> UPCGExPathInsertSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINTS(PCGExCommon::Labels::SourceTargetsLabel, "The point data to insert into paths.", Required)
	PCGEX_PIN_OPERATION_OVERRIDES(PCGExBlending::Labels::SourceOverridesBlendingOps)
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(PathInsert)
PCGEX_ELEMENT_BATCH_POINT_IMPL(PathInsert)

bool FPCGExPathInsertElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathInsert)

	PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendInstancedFactory, PCGExBlending::Labels::SourceOverridesBlendingOps)

	Context->TargetsHandler = MakeShared<PCGExMatching::FTargetsHandler>();
	Context->NumMaxTargets = Context->TargetsHandler->Init(Context, PCGExCommon::Labels::SourceTargetsLabel);

	if (!Context->NumMaxTargets)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No valid targets to insert."));
		return false;
	}

	return true;
}

bool FPCGExPathInsertElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathInsertElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathInsert)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		Context->SetState(PCGExCommon::States::State_FacadePreloading);

		TWeakPtr<FPCGContextHandle> WeakHandle = Context->GetOrCreateHandle();
		Context->TargetsHandler->TargetsPreloader->OnCompleteCallback = [Settings, Context, WeakHandle]()
		{
			PCGEX_SHARED_CONTEXT_VOID(WeakHandle)

			PCGEX_ON_INVALILD_INPUTS_C(SharedContext.Get(), FTEXT("Some inputs have less than 2 points and won't be processed."))

			if (!Context->StartBatchProcessingPoints(
				[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
				{
					if (Entry->GetNum() < 2)
					{
						Entry->InitializeOutput(PCGExData::EIOInit::Forward);
						if (Settings->bTagIfNoInserts) { Entry->Tags->AddRaw(Settings->NoInsertsTag); }
						bHasInvalidInputs = true;
						return false;
					}
					return true;
				}, [&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
				{
				}))
			{
				Context->CancelExecution(TEXT("Could not find any paths to process."));
			}
		};

		Context->TargetsHandler->StartLoading(Context->GetTaskManager());
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	PCGEX_OUTPUT_VALID_PATHS(MainPoints)

	return Context->TryComplete();
}

#pragma region FProcessor

namespace PCGExPathInsert
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPathInsert::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InTaskManager)) { return false; }

		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

		bClosedLoop = PCGExPaths::Helpers::GetClosedLoop(PointIO->GetIn());
		LastIndex = PointDataFacade->GetNum() - 1;

		// Build path structure
		Path = MakeShared<PCGExPaths::FPath>(PointIO->GetIn(), 0);
		Path->IOIndex = PointIO->IOIndex;
		PathLength = Path->AddExtra<PCGExPaths::FPathEdgeLength>();
		Path->ComputeAllEdgeExtra();

		// Initialize edge inserts array
		EdgeInserts.SetNum(Path->NumEdges);
		StartIndices.SetNum(PointDataFacade->GetNum());

		// Initialize range getter if filtering by range
		if (Settings->bWithinRange)
		{
			RangeGetter = Settings->Range.GetValueSetting();
			if (!RangeGetter->Init(PointDataFacade)) { return false; }
		}

		// Create sub-blending operation
		SubBlending = Context->Blending->CreateOperation();
		SubBlending->bClosedLoop = bClosedLoop;

		// Stage 1: Gather candidates from all targets
		// For each target point, find closest edge and alpha

		TConstPCGValueRange<FTransform> PathTransforms = PointIO->GetIn()->GetConstTransformValueRange();

		Context->TargetsHandler->ForEachTargetPoint([&](const PCGExData::FConstPoint& TargetPoint)
		{
			const FVector TargetLocation = TargetPoint.GetLocation();

			double BestDistSq = MAX_dbl;
			int32 BestEdgeIndex = -1;
			double BestAlpha = 0;
			FVector BestPathLocation = FVector::ZeroVector;

			// Find closest point on any edge
			for (int32 EdgeIdx = 0; EdgeIdx < Path->NumEdges; EdgeIdx++)
			{
				const PCGExPaths::FPathEdge& Edge = Path->Edges[EdgeIdx];

				const FVector Start = PathTransforms[Edge.Start].GetLocation();
				const FVector End = PathTransforms[Edge.End].GetLocation();

				// Find closest point on segment
				const FVector ClosestPoint = FMath::ClosestPointOnSegment(TargetLocation, Start, End);
				const double DistSq = FVector::DistSquared(TargetLocation, ClosestPoint);

				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					BestEdgeIndex = EdgeIdx;
					BestPathLocation = ClosestPoint;

					// Compute alpha (0-1 along edge)
					const double EdgeLength = FVector::Dist(Start, End);
					if (EdgeLength > KINDA_SMALL_NUMBER)
					{
						BestAlpha = FVector::Dist(Start, ClosestPoint) / EdgeLength;
					}
					else
					{
						BestAlpha = 0;
					}
				}
			}

			if (BestEdgeIndex < 0) { return; }

			const double BestDist = FMath::Sqrt(BestDistSq);

			// Check range filter if enabled
			if (RangeGetter && BestDist > RangeGetter->Read(0))
			{
				return;
			}

			// Add candidate
			FInsertCandidate Candidate;
			Candidate.TargetIOIndex = TargetPoint.IO;
			Candidate.TargetPointIndex = TargetPoint.Index;
			Candidate.EdgeIndex = BestEdgeIndex;
			Candidate.Alpha = BestAlpha;
			Candidate.Distance = BestDist;
			Candidate.PathLocation = BestPathLocation;
			Candidate.OriginalLocation = TargetLocation;

			EdgeInserts[BestEdgeIndex].Add(Candidate);
		});

		// Sort each edge's inserts by alpha
		for (FEdgeInserts& EI : EdgeInserts)
		{
			if (!EI.IsEmpty()) { EI.SortByAlpha(); }
		}

		// Count total inserts
		TotalInserts = 0;
		for (const FEdgeInserts& EI : EdgeInserts)
		{
			TotalInserts += EI.Num();
		}

		return true;
	}

	void FProcessor::CompleteWork()
	{
		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

		// No inserts? Just forward the data
		if (TotalInserts == 0)
		{
			PCGEX_INIT_IO_VOID(PointIO, PCGExData::EIOInit::Forward)
			if (Settings->bTagIfNoInserts) { PointIO->Tags->AddRaw(Settings->NoInsertsTag); }
			return;
		}

		// Calculate output size and start indices
		const int32 NumOriginalPoints = PointDataFacade->GetNum();
		const int32 NumOutputPoints = NumOriginalPoints + TotalInserts;

		int32 WriteIndex = 0;
		for (int32 i = 0; i < Path->NumEdges; i++)
		{
			StartIndices[i] = WriteIndex++;
			WriteIndex += EdgeInserts[i].Num();
		}

		// Handle last point for open paths
		if (!bClosedLoop)
		{
			StartIndices[LastIndex] = WriteIndex;
		}

		// Allocate output
		PCGEX_INIT_IO_VOID(PointIO, PCGExData::EIOInit::New)

		const UPCGBasePointData* InPoints = PointIO->GetIn();
		UPCGBasePointData* OutPoints = PointIO->GetOut();
		UPCGMetadata* Metadata = OutPoints->Metadata;

		PCGExPointArrayDataHelpers::SetNumPointsAllocated(OutPoints, NumOutputPoints, InPoints->GetAllocatedProperties());

		TConstPCGValueRange<int64> InMetadataEntries = InPoints->GetConstMetadataEntryValueRange();
		TPCGValueRange<int64> OutMetadataEntries = OutPoints->GetMetadataEntryValueRange();

		// Build index mapping for original points and initialize metadata
		TArray<int32> WriteIndices;
		WriteIndices.SetNum(NumOriginalPoints);

		WriteIndex = 0;
		for (int32 i = 0; i < Path->NumEdges; i++)
		{
			WriteIndices[i] = WriteIndex;

			// Copy original point's metadata
			OutMetadataEntries[WriteIndex] = InMetadataEntries[i];
			Metadata->InitializeOnSet(OutMetadataEntries[WriteIndex]);
			WriteIndex++;

			// Initialize metadata for inserted points (inherit from edge start)
			const int32 NumInserts = EdgeInserts[i].Num();
			for (int32 j = 0; j < NumInserts; j++)
			{
				OutMetadataEntries[WriteIndex] = PCGInvalidEntryKey;
				Metadata->InitializeOnSet(OutMetadataEntries[WriteIndex], InMetadataEntries[i], InPoints->Metadata);
				WriteIndex++;
			}
		}

		// Handle last point for open paths
		if (!bClosedLoop)
		{
			WriteIndices[LastIndex] = WriteIndex;
			OutMetadataEntries[WriteIndex] = InMetadataEntries[LastIndex];
			Metadata->InitializeOnSet(OutMetadataEntries[WriteIndex]);
		}

		// Copy original points to new locations
		PointIO->InheritPoints(WriteIndices);

		// Prepare blending
		if (!SubBlending->PrepareForData(Context, PointDataFacade, &ProtectedAttributes))
		{
			bIsProcessorValid = false;
			return;
		}

		// Tag output
		if (Settings->bTagIfHasInserts) { PointIO->Tags->AddRaw(Settings->HasInsertsTag); }

		// Process ranges to set positions and blend
		StartParallelLoopForRange(Path->NumEdges);
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

		TPCGValueRange<FTransform> OutTransforms = PointIO->GetOut()->GetTransformValueRange(false);
		TPCGValueRange<int32> OutSeeds = PointIO->GetOut()->GetSeedValueRange(false);

		TConstPCGValueRange<FTransform> InTransforms = PointIO->GetIn()->GetConstTransformValueRange();

		PCGEX_SCOPE_LOOP(EdgeIndex)
		{
			const FEdgeInserts& EI = EdgeInserts[EdgeIndex];
			if (EI.IsEmpty()) { continue; }

			const PCGExPaths::FPathEdge& Edge = Path->Edges[EdgeIndex];
			const int32 OutStartIdx = StartIndices[EdgeIndex];

			const FVector EdgeStart = InTransforms[Edge.Start].GetLocation();
			const FVector EdgeEnd = InTransforms[Edge.End].GetLocation();

			// Determine the end index for blending
			int32 EndPointIndex;
			if (EdgeIndex == Path->LastEdge && !bClosedLoop)
			{
				EndPointIndex = StartIndices[LastIndex];
			}
			else
			{
				EndPointIndex = StartIndices[(EdgeIndex + 1) % Path->NumEdges];
			}

			// Build path metrics for blending
			PCGExPaths::FPathMetrics Metrics = PCGExPaths::FPathMetrics(EdgeStart);

			const int32 NumInserts = EI.Num();
			for (int32 i = 0; i < NumInserts; i++)
			{
				const FInsertCandidate& Insert = EI.Inserts[i];
				const int32 InsertIndex = OutStartIdx + 1 + i;

				// Set position based on snap setting
				const FVector Position = Settings->bSnapToPath ? Insert.PathLocation : Insert.OriginalLocation;

				OutTransforms[InsertIndex].SetLocation(Position);
				OutSeeds[InsertIndex] = PCGExRandomHelpers::ComputeSpatialSeed(Position);

				Metrics.Add(Position);
			}

			Metrics.Add(EdgeEnd);

			// Apply sub-blending for attributes
			PCGExData::FScope SubScope = PointDataFacade->GetOutScope(OutStartIdx + 1, NumInserts);
			SubBlending->ProcessSubPoints(
				PointDataFacade->GetOutPoint(OutStartIdx),
				PointDataFacade->GetOutPoint(EndPointIndex),
				SubScope,
				Metrics);
		}
	}
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
