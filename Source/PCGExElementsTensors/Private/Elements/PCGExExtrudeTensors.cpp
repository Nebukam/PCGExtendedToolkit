// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExExtrudeTensors.h"

#include "Containers/PCGExScopedContainers.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataTags.h"
#include "Core/PCGExPointFilter.h"
#include "Core/PCGExTensorFactoryProvider.h"
#include "Data/PCGExPointIO.h"
#include "Helpers/PCGExArrayHelpers.h"
#include "Sorting/PCGExPointSorter.h"
#include "Sorting/PCGExSortingDetails.h"

#define LOCTEXT_NAMESPACE "PCGExExtrudeTensorsElement"
#define PCGEX_NAMESPACE ExtrudeTensors

PCGEX_SETTING_VALUE_IMPL(UPCGExExtrudeTensorsSettings, MaxLength, double, MaxLengthInput, MaxLengthAttribute, MaxLength)
PCGEX_SETTING_VALUE_IMPL(UPCGExExtrudeTensorsSettings, MaxPointsCount, int32, MaxPointsCountInput, MaxPointsCountAttribute, MaxPointsCount)
PCGEX_SETTING_VALUE_IMPL_BOOL(UPCGExExtrudeTensorsSettings, Iterations, int32, bUsePerPointMaxIterations, IterationsAttribute, Iterations)

//
// FExtrusionConfig Implementation
//

namespace PCGExExtrudeTensors
{
	void FExtrusionConfig::InitFromContext(const FPCGExExtrudeTensorsContext* InContext, const UPCGExExtrudeTensorsSettings* InSettings, bool bHasStopFilters)
	{
		// Transform settings
		bTransformRotation = InSettings->bTransformRotation;
		RotationMode = InSettings->Rotation;
		AlignAxis = InSettings->AlignAxis;

		// Limits
		FuseDistance = InSettings->FuseDistance;
		FuseDistanceSquared = FuseDistance * FuseDistance;
		StopHandling = InSettings->StopConditionHandling;
		bAllowChildExtrusions = InSettings->bAllowChildExtrusions;

		// External intersection
		bDoExternalIntersections = InSettings->bDoExternalPathIntersections;
		bIgnoreIntersectionOnOrigin = InSettings->bIgnoreIntersectionOnOrigin;

		// Self intersection
		bDoSelfIntersections = InSettings->bDoSelfPathIntersections;
		bMergeOnProximity = InSettings->bMergeOnProximity;
		ProximitySegmentBalance = InSettings->ProximitySegmentBalance;
		IntersectionPriority = InSettings->SelfIntersectionPriority;

		// Closed loop detection
		bDetectClosedLoops = InSettings->bDetectClosedLoops;
		ClosedLoopSquaredDistance = FMath::Square(InSettings->ClosedLoopSearchDistance);
		ClosedLoopSearchDot = PCGExMath::DegreesToDot(InSettings->ClosedLoopSearchAngle);

		// Copy intersection details
		ExternalPathIntersections = InSettings->ExternalPathIntersections;
		ExternalPathIntersections.Init();

		SelfPathIntersections = InSettings->SelfPathIntersections;
		SelfPathIntersections.Init();

		MergeDetails = InSettings->MergeDetails;
		MergeDetails.Init();

		// Compute flags
		uint32 FlagBits = 0;
		if (bAllowChildExtrusions) { FlagBits |= static_cast<uint32>(EExtrusionFlags::AllowsChildren); }
		if (bDetectClosedLoops) { FlagBits |= static_cast<uint32>(EExtrusionFlags::ClosedLoop); }
		if (bHasStopFilters) { FlagBits |= static_cast<uint32>(EExtrusionFlags::Bounded); }
		if (!InContext->ExternalPaths.IsEmpty() || bDoSelfIntersections) { FlagBits |= static_cast<uint32>(EExtrusionFlags::CollisionCheck); }

		Flags = static_cast<EExtrusionFlags>(FlagBits);
	}
}

//
// Settings Implementation
//

TArray<FPCGPinProperties> UPCGExExtrudeTensorsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_FACTORIES(PCGExTensor::SourceTensorsLabel, "Tensors", Required, FPCGExDataTypeInfoTensor::AsId())
	PCGEX_PIN_FILTERS(PCGExFilters::Labels::SourceStopConditionLabel, "Extruded points will be tested against those filters. If a filter returns true, the extrusion point is considered 'out-of-bounds'.", Normal)

	if (bDoExternalPathIntersections) { PCGEX_PIN_POINTS(PCGExPaths::Labels::SourcePathsLabel, "Paths that will be checked for intersections while extruding.", Normal) }
	else { PCGEX_PIN_POINTS(PCGExPaths::Labels::SourcePathsLabel, "(This is only there to preserve connections, enable it in the settings.)", Advanced) }

	PCGExSorting::DeclareSortingRulesInputs(PinProperties, bDoSelfPathIntersections ? EPCGPinStatus::Normal : EPCGPinStatus::Advanced);

	return PinProperties;
}

bool UPCGExExtrudeTensorsSettings::GetSortingRules(FPCGExContext* InContext, TArray<FPCGExSortRuleConfig>& OutRules) const
{
	OutRules.Append(PCGExSorting::GetSortingRules(InContext, PCGExSorting::Labels::SourceSortingRules));
	return !OutRules.IsEmpty();
}

PCGEX_INITIALIZE_ELEMENT(ExtrudeTensors)
PCGEX_ELEMENT_BATCH_POINT_IMPL_ADV(ExtrudeTensors)

FName UPCGExExtrudeTensorsSettings::GetMainInputPin() const { return PCGExCommon::Labels::SourceSeedsLabel; }
FName UPCGExExtrudeTensorsSettings::GetMainOutputPin() const { return PCGExPaths::Labels::OutputPathsLabel; }

//
// Element Implementation
//

bool FPCGExExtrudeTensorsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ExtrudeTensors)

	if (!PCGExFactories::GetInputFactories(InContext, PCGExTensor::SourceTensorsLabel, Context->TensorFactories, {PCGExFactories::EType::Tensor}))
	{
		return false;
	}

	GetInputFactories(Context, PCGExFilters::Labels::SourceStopConditionLabel, Context->StopFilterFactories, PCGExFactories::PointFilters, false);
	PCGExPointFilter::PruneForDirectEvaluation(Context, Context->StopFilterFactories);

	if (Context->TensorFactories.IsEmpty())
	{
		PCGEX_LOG_MISSING_INPUT(InContext, FTEXT("Missing tensors."))
		return false;
	}

	return true;
}

bool FPCGExExtrudeTensorsElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExExtrudeTensorsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(ExtrudeTensors)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		Context->AddConsumableAttributeName(Settings->IterationsAttribute);

		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
				NewBatch->bPrefetchData = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to subdivide."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	PCGEX_OUTPUT_VALID_PATHS(MainPoints)

	return Context->TryComplete();
}

//
// FExtrusion Implementation
//

namespace PCGExExtrudeTensors
{
	FExtrusion::FExtrusion(const int32 InSeedIndex, const TSharedRef<PCGExData::FFacade>& InFacade, const int32 InMaxIterations, const FExtrusionConfig& InConfig)
		: Config(InConfig), SeedIndex(InSeedIndex), RemainingIterations(InMaxIterations), PointDataFacade(InFacade)
	{
		ExtrudedPoints.Reserve(InMaxIterations);
		Origin = InFacade->Source->GetInPoint(SeedIndex);
		ProxyHead = PCGExData::FProxyPoint(Origin);
		ExtrudedPoints.Emplace(Origin.GetTransform());
		Flags = Config.Flags;
		SetHead(ExtrudedPoints.Last());
	}

	void FExtrusion::SetHead(const FTransform& InHead)
	{
		LastInsertion = InHead.GetLocation();
		Head = InHead;
		ProxyHead.Transform = InHead;
		ExtrudedPoints.Last() = Head;
		Metrics = PCGExPaths::FPathMetrics(LastInsertion);

		// Use 0 tolerance during initial construction (when no segments exist yet)
		// This matches the original behavior where Context was nullptr during construction
		const double Tol = SegmentBounds.Num() > 0 ? Config.SelfPathIntersections.Tolerance : 0;
		PCGEX_SET_BOX_TOLERANCE(Bounds, (Metrics.Last + FVector::OneVector * 1), (Metrics.Last + FVector::OneVector * -1), Tol);
	}

	PCGExMath::FSegment FExtrusion::GetHeadSegment() const
	{
		return PCGExMath::FSegment(ExtrudedPoints.Last(1).GetLocation(), ExtrudedPoints.Last().GetLocation(), Config.ExternalPathIntersections.Tolerance);
	}

	PCGExMath::FSegment FExtrusion::GetCurrentHeadSegment() const
	{
		// Returns segment from last inserted point to current head position
		// Used for self-intersection checking between active extrusions
		return PCGExMath::FSegment(ExtrudedPoints.Last().GetLocation(), Head.GetLocation(), Config.SelfPathIntersections.Tolerance);
	}

	bool FExtrusion::Advance()
	{
		if (State == EExtrusionState::Stopped || State == EExtrusionState::Completed)
		{
			return false;
		}

		bAdvancedOnly = true;

		const FVector PreviousHeadLocation = Head.GetLocation();

		// Sample tensor field
		bool bSuccess = false;
		const PCGExTensor::FTensorSample Sample = TensorsHandler->Sample(SeedIndex, Head, bSuccess);

		if (!bSuccess)
		{
			Stop(EStopReason::SamplingFailed);
			return false;
		}

		ExtrusionDirection = Sample.DirectionAndSize.GetSafeNormal();

		// Apply rotation based on configuration
		ApplyRotation(Sample);

		// Move head forward
		const FVector HeadLocation = PreviousHeadLocation + Sample.DirectionAndSize;
		Head.SetLocation(HeadLocation);
		ActiveTransform = Head;

		// Check for closed loop (if enabled)
		if (HasFlag(Flags, EExtrusionFlags::ClosedLoop))
		{
			if (CheckClosedLoop(PreviousHeadLocation))
			{
				Stop(EStopReason::ClosedLoop);
				return false;
			}
		}

		// Check stop filters (if bounded)
		if (HasFlag(Flags, EExtrusionFlags::Bounded))
		{
			ProxyHead.Transform = ActiveTransform;

			if (CheckStopFilters())
			{
				// Hit stop filter
				if (State == EExtrusionState::Extruding)
				{
					if (Config.StopHandling == EPCGExTensorStopConditionHandling::Include)
					{
						Insert(Head);
					}

					Complete();

					if (!HasFlag(Flags, EExtrusionFlags::AllowsChildren))
					{
						Stop(EStopReason::StopFilter);
						return false;
					}
				}

				// Still probing or completed - continue advancing
				RemainingIterations--;
				return RemainingIterations > 0;
			}

			// We're out of stop filter region
			if (State == EExtrusionState::Completed)
			{
				// Re-entered valid region after completion - start child
				if (HasFlag(Flags, EExtrusionFlags::AllowsChildren))
				{
					StartNewExtrusion();
				}
				Stop(EStopReason::StopFilter);
				return false;
			}

			// Transition from probing to extruding
			if (State == EExtrusionState::Probing)
			{
				State = EExtrusionState::Extruding;
				bIsExtruding = true;
				SetHead(Head);
				RemainingIterations--;
				return RemainingIterations > 0;
			}
		}
		else
		{
			// No stop filters - immediately start extruding
			if (State == EExtrusionState::Probing)
			{
				State = EExtrusionState::Extruding;
				bIsExtruding = true;
			}
		}

		// Now in extruding state - add point
		State = EExtrusionState::Extruding;
		bIsExtruding = true;

		// Track distance for fuse check
		double DistToLast = 0;
		const double Length = Metrics.Add(Metrics.Last + Sample.DirectionAndSize, DistToLast);
		DistToLastSum += DistToLast;

		// Skip if too close to last point
		if (DistToLastSum < Config.FuseDistance)
		{
			RemainingIterations--;
			return RemainingIterations > 0;
		}
		DistToLastSum = 0;

		// Adjust position if exceeding max length
		if (Length > MaxLength)
		{
			const FVector LastValidPos = ExtrudedPoints.Last().GetLocation();
			ActiveTransform.SetLocation(LastValidPos + ((Metrics.Last - LastValidPos).GetSafeNormal() * (MaxLength - (Length - DistToLast))));
		}

		// Check for collisions (if enabled)
		if (HasFlag(Flags, EExtrusionFlags::CollisionCheck))
		{
			const PCGExMath::FSegment Segment(ExtrudedPoints.Last().GetLocation(), ActiveTransform.GetLocation());

			if (CheckCollisions(Segment))
			{
				return false;
			}
		}

		// Insert the point
		Insert(ActiveTransform);

		// Check termination conditions
		RemainingIterations--;

		if (RemainingIterations <= 0)
		{
			Stop(EStopReason::Iterations);
			return false;
		}

		if (Length >= MaxLength)
		{
			Stop(EStopReason::MaxLength);
			return false;
		}

		if (ExtrudedPoints.Num() >= MaxPointCount)
		{
			Stop(EStopReason::MaxPointCount);
			return false;
		}

		return true;
	}

	void FExtrusion::ApplyRotation(const PCGExTensor::FTensorSample& Sample)
	{
		if (!Config.bTransformRotation) { return; }

		switch (Config.RotationMode)
		{
		case EPCGExTensorTransformMode::Absolute:
			Head.SetRotation(Sample.Rotation);
			break;

		case EPCGExTensorTransformMode::Relative:
			Head.SetRotation(Head.GetRotation() * Sample.Rotation);
			break;

		case EPCGExTensorTransformMode::Align:
			Head.SetRotation(PCGExMath::MakeDirection(Config.AlignAxis, ExtrusionDirection * -1, Head.GetRotation().GetUpVector()));
			break;
		}
	}

	bool FExtrusion::CheckClosedLoop(const FVector& PreviousHeadLocation)
	{
		const FVector Tail = Origin.GetLocation();

		if (FVector::DistSquared(Metrics.Last, Tail) > Config.ClosedLoopSquaredDistance)
		{
			return false;
		}

		const FVector DirectionToTail = (Tail - PreviousHeadLocation).GetSafeNormal();
		return FVector::DotProduct(ExtrusionDirection, DirectionToTail) > Config.ClosedLoopSearchDot;
	}

	bool FExtrusion::CheckStopFilters()
	{
		if (!StopFilters) { return false; }
		return StopFilters->Test(ProxyHead);
	}

	bool FExtrusion::CheckCollisions(const PCGExMath::FSegment& Segment)
	{
		// Check external path intersections first
		FCollisionResult ExternalResult = CheckExternalIntersection(Segment);

		if (ExternalResult)
		{
			// Handle origin intersection ignore
			if (FMath::IsNearlyZero(ExternalResult.Position.SquaredLength() - Origin.GetLocation().SquaredLength()))
			{
				if (Config.bIgnoreIntersectionOnOrigin && ExtrudedPoints.Num() <= 1)
				{
					ExternalResult.bHasCollision = false;
				}
			}

			if (ExternalResult)
			{
				ActiveTransform.SetLocation(ExternalResult.Position);
				Insert(ActiveTransform);
				Stop(EStopReason::ExternalPath);
				return true;
			}
		}

		// Check self path intersections
		PCGExMath::FClosestPosition MergePosition(Segment.Lerp(Config.ProximitySegmentBalance));
		FCollisionResult SelfResult = CheckSelfIntersection(Segment, MergePosition);

		// Create merge collision result if applicable
		FCollisionResult MergeResult;
		if (TryMerge(Segment, MergePosition))
		{
			MergeResult.Set(MergePosition, EStopReason::SelfMerge);
		}

		// Resolve priority between crossing and merge
		FCollisionResult FinalResult = ResolveCollisionPriority(SelfResult, MergeResult);

		if (FinalResult)
		{
			ActiveTransform.SetLocation(FinalResult.Position);
			Insert(ActiveTransform);
			Stop(FinalResult.Reason);
			return true;
		}

		return false;
	}

	FCollisionResult FExtrusion::CheckExternalIntersection(const PCGExMath::FSegment& Segment)
	{
		FCollisionResult Result;

		if (!Config.bDoExternalIntersections || !ExternalPaths || ExternalPaths->IsEmpty()) { return Result; }

		int32 PathIndex = -1;
		PCGExMath::FClosestPosition Intersection = PCGExPaths::Helpers::FindClosestIntersection(
			*ExternalPaths,
			Config.ExternalPathIntersections,
			Segment,
			PathIndex);

		if (Intersection)
		{
			Result.Set(Intersection, EStopReason::ExternalPath, PathIndex);
		}

		return Result;
	}

	FCollisionResult FExtrusion::CheckSelfIntersection(const PCGExMath::FSegment& Segment, PCGExMath::FClosestPosition& OutMerge)
	{
		FCollisionResult Result;

		if (!Config.bDoSelfIntersections || !SolidPaths) { return Result; }

		int32 PathIndex = -1;
		// Note: Original uses ExternalPathIntersections for SolidPaths check, not SelfPathIntersections
		PCGExMath::FClosestPosition Intersection = PCGExPaths::Helpers::FindClosestIntersection(
			*SolidPaths.Get(),
			Config.ExternalPathIntersections,
			Segment,
			PathIndex,
			OutMerge);

		if (Intersection)
		{
			Result.Set(Intersection, EStopReason::SelfIntersection, PathIndex);
		}

		return Result;
	}

	FCollisionResult FExtrusion::ResolveCollisionPriority(const FCollisionResult& Crossing, const FCollisionResult& Merge)
	{
		if (!Crossing && !Merge) { return FCollisionResult(); }
		if (Crossing && !Merge) { return Crossing; }
		if (!Crossing && Merge) { return Merge; }

		// Both exist - use priority setting
		if (Config.IntersectionPriority == EPCGExSelfIntersectionPriority::Crossing)
		{
			return Crossing;
		}
		return Merge;
	}

	bool FExtrusion::TryMerge(const PCGExMath::FSegment& InSegment, const PCGExMath::FClosestPosition& InMerge)
	{
		if (!Config.bMergeOnProximity || !InMerge) { return false; }

		if (Config.MergeDetails.bWantsDotCheck)
		{
			if (!Config.MergeDetails.CheckDot(FMath::Abs(FVector::DotProduct(InMerge.Direction(), InSegment.Direction))))
			{
				return false;
			}
		}

		return InMerge.DistSquared <= Config.MergeDetails.ToleranceSquared;
	}

	PCGExMath::FClosestPosition FExtrusion::FindCrossing(const PCGExMath::FSegment& InSegment, bool& OutIsLastSegment, PCGExMath::FClosestPosition& OutClosestPosition, const int32 TruncateSearch) const
	{
		if (!Bounds.Intersect(InSegment.Bounds)) { return PCGExMath::FClosestPosition(); }

		const int32 MaxSearches = SegmentBounds.Num() - TruncateSearch;
		if (MaxSearches <= 0) { return PCGExMath::FClosestPosition(); }

		PCGExMath::FClosestPosition Crossing(InSegment.A);

		const int32 LastSegment = SegmentBounds.Num() - 1;
		const double SqrTolerance = Config.SelfPathIntersections.ToleranceSquared;
		const uint8 Strictness = Config.SelfPathIntersections.Strictness;

		for (int32 i = 0; i < MaxSearches; i++)
		{
			if (!SegmentBounds[i].Intersect(InSegment.Bounds)) { continue; }

			FVector A = ExtrudedPoints[i].GetLocation();
			FVector B = ExtrudedPoints[i + 1].GetLocation();

			if (Config.SelfPathIntersections.bWantsDotCheck)
			{
				if (!Config.SelfPathIntersections.CheckDot(FMath::Abs(FVector::DotProduct((B - A).GetSafeNormal(), InSegment.Direction))))
				{
					continue;
				}
			}

			FVector OutSelf = FVector::ZeroVector;
			FVector OutOther = FVector::ZeroVector;

			if (!InSegment.FindIntersection(A, B, SqrTolerance, OutSelf, OutOther, Strictness))
			{
				OutClosestPosition.Update(OutOther);
				continue;
			}

			OutClosestPosition.Update(OutOther);
			Crossing.Update(OutOther, i);
		}

		OutIsLastSegment = Crossing.Index == LastSegment;
		return Crossing;
	}

	void FExtrusion::Insert(const FTransform& InPoint)
	{
		bAdvancedOnly = false;

		ExtrudedPoints.Emplace(InPoint);
		LastInsertion = InPoint.GetLocation();

		if (Config.bDoSelfIntersections)
		{
			PCGEX_BOX_TOLERANCE(OEBox, ExtrudedPoints.Last(1).GetLocation(), ExtrudedPoints.Last().GetLocation(), (Config.SelfPathIntersections.ToleranceSquared + 1));
			SegmentBounds.Emplace(OEBox);
			Bounds += OEBox;
		}
	}

	void FExtrusion::StartNewExtrusion()
	{
		if (RemainingIterations > 1 && Processor)
		{
			if (TSharedPtr<FExtrusion> ChildExtrusion = Processor->InitExtrusionFromExtrusion(SharedThis(this)))
			{
				ChildExtrusion->bIsChildExtrusion = true;
				ChildExtrusion->bIsFollowUp = (State == EExtrusionState::Completed);
			}
		}
	}

	void FExtrusion::Stop(EStopReason InReason)
	{
		StopReason |= InReason;
		Complete();
		State = EExtrusionState::Stopped;
	}

	void FExtrusion::Complete()
	{
		if (State == EExtrusionState::Completed || State == EExtrusionState::Stopped)
		{
			return;
		}

		State = EExtrusionState::Completed;

		// Validate path
		const UPCGExExtrudeTensorsSettings* Settings = Processor ?
			static_cast<const UPCGExExtrudeTensorsSettings*>(Processor->GetContext()->GetInputSettings<UPCGExExtrudeTensorsSettings>()) : nullptr;

		const bool bIsValid = Settings ? Settings->PathOutputDetails.Validate(ExtrudedPoints.Num()) : (ExtrudedPoints.Num() >= 2);

		if (!bIsValid)
		{
			PointDataFacade->Source->InitializeOutput(PCGExData::EIOInit::NoInit);
			PointDataFacade->Source->Disable();
			return;
		}

		// Write output data
		UPCGBasePointData* OutPointData = PointDataFacade->GetOut();
		PCGExPaths::Helpers::SetClosedLoop(OutPointData, HasStopReason(EStopReason::ClosedLoop));

		PCGExPointArrayDataHelpers::SetNumPointsAllocated(OutPointData, ExtrudedPoints.Num(), PointDataFacade->GetAllocations());

		TPCGValueRange<FTransform> OutTransforms = OutPointData->GetTransformValueRange();
		for (int32 i = 0; i < ExtrudedPoints.Num(); i++)
		{
			OutTransforms[i] = ExtrudedPoints[i];
		}

		// Apply tags based on stop reason
		if (Settings)
		{
			if (Settings->bTagIfIsStoppedByFilters && HasStopReason(EStopReason::StopFilter))
			{
				PointDataFacade->Source->Tags->AddRaw(Settings->IsStoppedByFiltersTag);
			}

			if (Settings->bTagIfIsStoppedByIntersection && (HasStopReason(EStopReason::ExternalPath) || HasStopReason(EStopReason::SelfIntersection)))
			{
				PointDataFacade->Source->Tags->AddRaw(Settings->IsStoppedByIntersectionTag);
			}

			if (Settings->bTagIfIsStoppedBySelfIntersection && HasStopReason(EStopReason::SelfIntersection))
			{
				PointDataFacade->Source->Tags->AddRaw(Settings->IsStoppedBySelfIntersectionTag);
			}

			if (Settings->bTagIfSelfMerged && HasStopReason(EStopReason::SelfMerge))
			{
				PointDataFacade->Source->Tags->AddRaw(Settings->IsSelfMergedTag);
			}

			if (Settings->bTagIfChildExtrusion && bIsChildExtrusion)
			{
				PointDataFacade->Source->Tags->AddRaw(Settings->IsChildExtrusionTag);
			}

			if (Settings->bTagIfIsFollowUp && bIsFollowUp)
			{
				PointDataFacade->Source->Tags->AddRaw(Settings->IsFollowUpTag);
			}
		}

		PointDataFacade->Source->GetOutKeys(true);
	}

	void FExtrusion::CutOff(const FVector& InCutOff)
	{
		FVector PrevPos = ExtrudedPoints.Last(1).GetLocation();

		ExtrudedPoints.Last().SetLocation(InCutOff);
		StopReason |= EStopReason::SelfIntersection;

		Complete();

		PCGEX_BOX_TOLERANCE(OEBox, PrevPos, ExtrudedPoints.Last().GetLocation(), (Config.SelfPathIntersections.ToleranceSquared + 1));
		if (SegmentBounds.Num() > 0)
		{
			SegmentBounds.Last() = OEBox;
		}

		State = EExtrusionState::Stopped;
	}

	void FExtrusion::Shorten(const FVector& InCutOff)
	{
		const FVector A = ExtrudedPoints.Last(1).GetLocation();
		if (FVector::DistSquared(A, InCutOff) < FVector::DistSquared(A, ExtrudedPoints.Last().GetLocation()))
		{
			ExtrudedPoints.Last().SetLocation(InCutOff);
		}
	}

	void FExtrusion::Cleanup()
	{
		SegmentBounds.Empty();
	}

	//
	// FProcessor Implementation
	//

	FProcessor::~FProcessor()
	{
	}

	void FProcessor::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TProcessor<FPCGExExtrudeTensorsContext, UPCGExExtrudeTensorsSettings>::RegisterBuffersDependencies(FacadePreloader);

		TArray<FPCGExSortRuleConfig> RuleConfigs;
		if (Settings->GetSortingRules(ExecutionContext, RuleConfigs) && !RuleConfigs.IsEmpty())
		{
			Sorter = MakeShared<PCGExSorting::FSorter>(Context, PointDataFacade, RuleConfigs);
			Sorter->SortDirection = Settings->SortDirection;
		}
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExExtrudeTensors::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InTaskManager)) { return false; }

		if (Sorter && !Sorter->Init(Context)) { Sorter.Reset(); }

		StaticPaths = MakeShared<TArray<TSharedPtr<PCGExPaths::FPath>>>();

		// Initialize stop filters if present
		if (!Context->StopFilterFactories.IsEmpty())
		{
			StopFilters = MakeShared<PCGExPointFilter::FManager>(PointDataFacade);
			if (!StopFilters->Init(Context, Context->StopFilterFactories)) { StopFilters.Reset(); }
		}

		// Initialize config with stop filter state
		Context->ExtrusionConfig.InitFromContext(Context, Settings, StopFilters.IsValid());

		// Initialize tensor handler
		TensorsHandler = MakeShared<PCGExTensor::FTensorsHandler>(Settings->TensorHandlerDetails);
		if (!TensorsHandler->Init(Context, Context->TensorFactories, PointDataFacade)) { return false; }

		AttributesToPathTags = Settings->AttributesToPathTags;
		if (!AttributesToPathTags.Init(Context, PointDataFacade)) { return false; }

		// Initialize per-point settings
		PerPointIterations = Settings->GetValueSettingIterations();
		if (!PerPointIterations->Init(PointDataFacade, false, true)) { return false; }
		if (!PerPointIterations->IsConstant())
		{
			if (Settings->bUseMaxFromPoints) { RemainingIterations = FMath::Max(RemainingIterations, PerPointIterations->Max()); }
		}
		else { RemainingIterations = Settings->Iterations; }

		if (Settings->bUseMaxLength)
		{
			MaxLength = Settings->GetValueSettingMaxLength();
			if (!MaxLength->Init(PointDataFacade, false)) { return false; }
		}

		if (Settings->bUseMaxPointsCount)
		{
			MaxPointsCount = Settings->GetValueSettingMaxPointsCount();
			if (!MaxPointsCount->Init(PointDataFacade, false)) { return false; }
		}

		const int32 NumPoints = PointDataFacade->GetNum();
		PCGExArrayHelpers::InitArray(ExtrusionQueue, NumPoints);
		PointFilterCache.Init(true, NumPoints);

		Context->MainPoints->IncreaseReserve(NumPoints);

		StartParallelLoopForPoints(PCGExData::EIOSide::In);

		return true;
	}

	void FProcessor::InitExtrusionFromSeed(const int32 InSeedIndex)
	{
		const int32 Iterations = PerPointIterations->Read(InSeedIndex);
		if (Iterations < 1) { return; }

		bool bIsStopped = false;
		if (StopFilters)
		{
			const PCGExData::FProxyPoint ProxyPoint(PointDataFacade->Source->GetInPoint(InSeedIndex));
			bIsStopped = StopFilters->Test(ProxyPoint);
			if (Settings->bIgnoreStoppedSeeds && bIsStopped) { return; }
		}

		TSharedPtr<FExtrusion> NewExtrusion = CreateExtrusion(InSeedIndex, Iterations);
		if (!NewExtrusion) { return; }

		// If starting stopped, stay in probing state
		if (bIsStopped)
		{
			NewExtrusion->State = EExtrusionState::Probing;
		}

		if (Settings->bUseMaxLength) { NewExtrusion->MaxLength = MaxLength->Read(InSeedIndex); }
		if (Settings->bUseMaxPointsCount) { NewExtrusion->MaxPointCount = MaxPointsCount->Read(InSeedIndex); }

		ExtrusionQueue[InSeedIndex] = NewExtrusion;
	}

	TSharedPtr<FExtrusion> FProcessor::InitExtrusionFromExtrusion(const TSharedRef<FExtrusion>& InExtrusion)
	{
		if (!Settings->bAllowChildExtrusions) { return nullptr; }

		TSharedPtr<FExtrusion> NewExtrusion = CreateExtrusion(InExtrusion->SeedIndex, InExtrusion->RemainingIterations);
		if (!NewExtrusion) { return nullptr; }

		NewExtrusion->SetHead(InExtrusion->Head);
		NewExtrusion->ParentExtrusion = InExtrusion;

		{
			FWriteScopeLock WriteScopeLock(NewExtrusionLock);
			NewExtrusions.Add(NewExtrusion);
		}

		return NewExtrusion;
	}

	TSharedPtr<FExtrusion> FProcessor::CreateExtrusion(const int32 InSeedIndex, const int32 InMaxIterations)
	{
		TSharedPtr<PCGExData::FPointIO> NewIO = Context->MainPoints->Emplace_GetRef(PointDataFacade->Source->GetIn(), PCGExData::EIOInit::NoInit);
		if (!NewIO) { return nullptr; }

		PCGEX_MAKE_SHARED(Facade, PCGExData::FFacade, NewIO.ToSharedRef());
		if (!Facade->Source->InitializeOutput(PCGExData::EIOInit::New)) { return nullptr; }

		TSharedPtr<FExtrusion> NewExtrusion = MakeShared<FExtrusion>(InSeedIndex, Facade.ToSharedRef(), InMaxIterations, Context->ExtrusionConfig);

		if (Settings->bUseMaxLength) { NewExtrusion->MaxLength = MaxLength->Read(InSeedIndex); }
		if (Settings->bUseMaxPointsCount) { NewExtrusion->MaxPointCount = MaxPointsCount->Read(InSeedIndex); }

		NewExtrusion->PointDataFacade->Source->IOIndex = BatchIndex * 1000000 + InSeedIndex;
		AttributesToPathTags.Tag(PointDataFacade->GetInPoint(InSeedIndex), Facade->Source);

		NewExtrusion->Processor = this;
		NewExtrusion->TensorsHandler = TensorsHandler;
		NewExtrusion->StopFilters = StopFilters;
		NewExtrusion->SolidPaths = StaticPaths;
		NewExtrusion->ExternalPaths = &Context->ExternalPaths;

		return NewExtrusion;
	}

	void FProcessor::SortQueue()
	{
		switch (Settings->SelfIntersectionMode)
		{
		case EPCGExSelfIntersectionMode::PathLength:
			if (Sorter)
			{
				if (Settings->SortDirection == EPCGExSortDirection::Ascending)
				{
					ExtrusionQueue.Sort([S = Sorter](const TSharedPtr<FExtrusion>& EA, const TSharedPtr<FExtrusion>& EB)
					{
						if (EA->Metrics.Length == EB->Metrics.Length) { return S->Sort(EA->SeedIndex, EB->SeedIndex); }
						return EA->Metrics.Length > EB->Metrics.Length;
					});
				}
				else
				{
					ExtrusionQueue.Sort([S = Sorter](const TSharedPtr<FExtrusion>& EA, const TSharedPtr<FExtrusion>& EB)
					{
						if (EA->Metrics.Length == EB->Metrics.Length) { return S->Sort(EA->SeedIndex, EB->SeedIndex); }
						return EA->Metrics.Length < EB->Metrics.Length;
					});
				}
			}
			else
			{
				ExtrusionQueue.Sort([](const TSharedPtr<FExtrusion>& EA, const TSharedPtr<FExtrusion>& EB)
				{
					return EA->Metrics.Length > EB->Metrics.Length;
				});
			}
			break;

		case EPCGExSelfIntersectionMode::SortingOnly:
			if (Sorter)
			{
				ExtrusionQueue.Sort([S = Sorter](const TSharedPtr<FExtrusion>& EA, const TSharedPtr<FExtrusion>& EB)
				{
					return S->Sort(EA->SeedIndex, EB->SeedIndex);
				});
			}
			break;
		}
	}

	void FProcessor::PrepareLoopScopesForRanges(const TArray<PCGExMT::FScope>& Loops)
	{
		CompletedExtrusions = MakeShared<PCGExMT::TScopedArray<TSharedPtr<FExtrusion>>>(Loops);
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::ExtrudeTensors::ProcessPoints);

		PointDataFacade->Fetch(Scope);
		PCGEX_SCOPE_LOOP(Index) { InitExtrusionFromSeed(Index); }
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		if (UpdateExtrusionQueue()) { StartParallelLoopForRange(ExtrusionQueue.Num(), 32); }
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		PCGEX_SCOPE_LOOP(Index)
		{
			if (TSharedPtr<FExtrusion> Extrusion = ExtrusionQueue[Index]; Extrusion && !Extrusion->Advance())
			{
				Extrusion->Complete();
				CompletedExtrusions->Get(Scope)->Add(Extrusion);
			}
		}
	}

	void FProcessor::OnRangeProcessingComplete()
	{
		RemainingIterations--;

		if (Settings->bDoSelfPathIntersections)
		{
			const bool bMergeFirst = Settings->SelfIntersectionPriority == EPCGExSelfIntersectionPriority::Merge;
			const int32 NumQueuedExtrusions = ExtrusionQueue.Num();

			SortQueue();

			for (int32 i = 0; i < NumQueuedExtrusions; i++)
			{
				TSharedPtr<FExtrusion> E = ExtrusionQueue[i];

				if (E->bAdvancedOnly || !E->bIsExtruding) { continue; }

				const PCGExMath::FSegment HeadSegment = E->GetHeadSegment();
				PCGExMath::FClosestPosition Crossing(HeadSegment.A);
				PCGExMath::FClosestPosition Merge(HeadSegment.Lerp(Settings->ProximitySegmentBalance));
				PCGExMath::FClosestPosition PreMerge(Merge.Origin);

				for (int32 j = 0; j < ExtrusionQueue.Num(); j++)
				{
					const TSharedPtr<FExtrusion> OE = ExtrusionQueue[j];
					if (!OE->bIsExtruding) { continue; }
					if (!OE->Bounds.Intersect(HeadSegment.Bounds)) { continue; }

					const int32 TruncateSearch = i == j ? 2 : 0;
					bool bIsLastSegment = false;
					if (j > i)
					{
						if (PCGExMath::FClosestPosition LocalCrossing = OE->FindCrossing(HeadSegment, bIsLastSegment, PreMerge, TruncateSearch))
						{
							if (bIsLastSegment)
							{
								// Lower priority path
								// Cut will happen the other way around
								continue;
							}

							Merge.Update(PreMerge);
							Crossing.Update(LocalCrossing, j);
						}
					}
					else
					{
						if (PCGExMath::FClosestPosition LocalCrossing = OE->FindCrossing(HeadSegment, bIsLastSegment, PreMerge, TruncateSearch))
						{
							// Crossing found
							if (bMergeFirst) { Merge.Update(PreMerge); }
							Crossing.Update(LocalCrossing, j);
						}
						else
						{
							// Update merge instead
							Merge.Update(PreMerge);
						}
					}
				}

				if (bMergeFirst)
				{
					if (E->TryMerge(HeadSegment, Merge))
					{
						E->CutOff(Merge);
						CompletedExtrusions->Arrays[0]->Add(E);
					}
					else if (Crossing)
					{
						E->CutOff(Crossing);
						CompletedExtrusions->Arrays[0]->Add(E);
					}
				}
				else
				{
					if (Crossing)
					{
						E->CutOff(Crossing);
						CompletedExtrusions->Arrays[0]->Add(E);
					}
					else if (E->TryMerge(HeadSegment, Merge))
					{
						E->CutOff(Merge);
						CompletedExtrusions->Arrays[0]->Add(E);
					}
				}
			}
		}

		if (UpdateExtrusionQueue()) { StartParallelLoopForRange(ExtrusionQueue.Num(), 32); }
	}

	bool FProcessor::UpdateExtrusionQueue()
	{
		if (RemainingIterations <= 0) { return false; }

		int32 WriteIndex = 0;
		for (int32 i = 0; i < ExtrusionQueue.Num(); i++)
		{
			if (TSharedPtr<FExtrusion> E = ExtrusionQueue[i]; E && E->IsActive())
			{
				ExtrusionQueue[WriteIndex++] = E;
			}
		}

		ExtrusionQueue.SetNum(WriteIndex);

		if (!NewExtrusions.IsEmpty())
		{
			ExtrusionQueue.Reserve(ExtrusionQueue.Num() + NewExtrusions.Num());
			ExtrusionQueue.Append(NewExtrusions);
			NewExtrusions.Reset();
		}

		if (ExtrusionQueue.IsEmpty()) { return false; }

		// Convert completed paths to static collision constraints
		if (Settings->bDoSelfPathIntersections && CompletedExtrusions)
		{
			CompletedExtrusions->ForEach([&](TArray<TSharedPtr<FExtrusion>>& Completed)
			{
				StaticPaths.Get()->Reserve(StaticPaths.Get()->Num() + Completed.Num());
				for (const TSharedPtr<FExtrusion>& E : Completed)
				{
					E->Cleanup();

					if (!E->IsValidPath()) { continue; }

					TSharedPtr<PCGExPaths::FPath> StaticPath = MakeShared<PCGExPaths::FPath>(E->PointDataFacade->GetOut(), Settings->ExternalPathIntersections.Tolerance);
					StaticPath->BuildEdgeOctree();
					StaticPaths.Get()->Add(StaticPath);
				}
			});

			CompletedExtrusions.Reset();
		}

		return true;
	}

	void FProcessor::CompleteWork()
	{
		for (const TSharedPtr<FExtrusion>& E : ExtrusionQueue)
		{
			if (E) { E->Complete(); }
		}
		CompletedExtrusions.Reset();
		ExtrusionQueue.Empty();
		StaticPaths->Empty();
	}

	//
	// FBatch Implementation
	//

	FBatch::FBatch(FPCGExContext* InContext, const TArray<TWeakPtr<PCGExData::FPointIO>>& InPointsCollection)
		: TBatch<FProcessor>(InContext, InPointsCollection)
	{
	}

	void FBatch::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TaskManager = InTaskManager;

		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ExtrudeTensors)

		if (Settings->bDoExternalPathIntersections)
		{
			if (TryGetFacades(Context, PCGExPaths::Labels::SourcePathsLabel, Context->PathsFacades, false, true))
			{
				Context->ExternalPaths.Reserve(Context->PathsFacades.Num());
				for (const TSharedPtr<PCGExData::FFacade>& Facade : Context->PathsFacades)
				{
					TSharedPtr<PCGExPaths::FPath> Path = MakeShared<PCGExPaths::FPath>(Facade->GetIn(), Settings->ExternalPathIntersections.Tolerance);
					Context->ExternalPaths.Add(Path);
					Path->BuildEdgeOctree();
				}
			}
		}

		OnPathsPrepared();
	}

	void FBatch::OnPathsPrepared()
	{
		TBatch<FProcessor>::Process(TaskManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
