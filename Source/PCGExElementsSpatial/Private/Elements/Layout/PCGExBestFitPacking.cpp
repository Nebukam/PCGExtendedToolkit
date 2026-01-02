// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/Layout/PCGExBestFitPacking.h"

#include "Data/PCGExAttributeBroadcaster.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExSettingsDetails.h"
#include "Helpers/PCGExArrayHelpers.h"
#include "Elements/Layout/PCGExLayout.h"
#include "Math/PCGExMathBounds.h"
#include "Sorting/PCGExPointSorter.h"
#include "Sorting/PCGExSortingDetails.h"

#define LOCTEXT_NAMESPACE "PCGExBestFitPackingElement"
#define PCGEX_NAMESPACE BestFitPacking

PCGEX_SETTING_VALUE_IMPL(UPCGExBestFitPackingSettings, Padding, FVector, OccupationPaddingInput, OccupationPaddingAttribute, OccupationPadding)

bool UPCGExBestFitPackingSettings::GetSortingRules(FPCGExContext* InContext, TArray<FPCGExSortRuleConfig>& OutRules) const
{
	OutRules.Append(PCGExSorting::GetSortingRules(InContext, PCGExSorting::Labels::SourceSortingRules));
	return !OutRules.IsEmpty();
}

TArray<FPCGPinProperties> UPCGExBestFitPackingSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINTS(PCGExLayout::SourceBinsLabel, "List of bins to fit input points into. Each input collection is expected to have a matching collection of bins.", Required)
	PCGExSorting::DeclareSortingRulesInputs(PinProperties, EPCGPinStatus::Normal);
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExBestFitPackingSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExLayout::OutputBinsLabel, "Input bins, with added statistics.", Required)
	PCGEX_PIN_POINTS(PCGExCommon::Labels::OutputDiscardedLabel, "Discarded points, ones that could not fit into any bin.", Required)
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(BestFitPacking)

PCGExData::EIOInit UPCGExBestFitPackingSettings::GetMainDataInitializationPolicy() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_ELEMENT_BATCH_POINT_IMPL(BestFitPacking)

bool FPCGExBestFitPackingElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BestFitPacking)

	Context->Bins = MakeShared<PCGExData::FPointIOCollection>(InContext, PCGExLayout::SourceBinsLabel, PCGExData::EIOInit::NoInit);
	Context->Bins->OutputPin = PCGExLayout::OutputBinsLabel;

	int32 NumBins = Context->Bins->Num();
	int32 NumInputs = Context->MainPoints->Num();

	Context->ValidIOIndices.Reserve(FMath::Max(NumBins, NumInputs));

	if (NumBins != NumInputs)
	{
		if (NumBins > NumInputs)
		{
			NumBins = NumInputs;
			if (!Settings->bQuietTooManyBinsWarning)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("There are more bins than there are inputs. Extra bins will be ignored."));
			}

			for (int i = 0; i < NumInputs; i++)
			{
				Context->ValidIOIndices.Add(Context->MainPoints->Pairs[i]->IOIndex);
			}
		}
		else if (NumInputs > NumBins)
		{
			NumInputs = NumBins;
			if (!Settings->bQuietTooFewBinsWarning)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("There are more inputs than there are bins. Extra inputs will be ignored."));
			}

			for (int i = 0; i < NumBins; i++)
			{
				Context->ValidIOIndices.Add(Context->MainPoints->Pairs[i]->IOIndex);
			}
		}
	}
	else
	{
		for (int i = 0; i < NumInputs; i++)
		{
			Context->ValidIOIndices.Add(Context->MainPoints->Pairs[i]->IOIndex);
		}
	}

	for (int i = 0; i < NumBins; i++) { Context->Bins->Pairs[i]->OutputPin = Context->Bins->OutputPin; }

	Context->Discarded = MakeShared<PCGExData::FPointIOCollection>(InContext);
	Context->Discarded->OutputPin = PCGExCommon::Labels::OutputDiscardedLabel;

	return true;
}

bool FPCGExBestFitPackingElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBestFitPackingElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BestFitPacking)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints([&](const TSharedPtr<PCGExData::FPointIO>& Entry)
		                                         {
			                                         return Context->ValidIOIndices.Contains(Entry->IOIndex);
		                                         }, [&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
		                                         {
			                                         TArray<FPCGExSortRuleConfig> OutRules;
			                                         Settings->GetSortingRules(Context, OutRules);
			                                         NewBatch->bPrefetchData = !OutRules.IsEmpty() || Settings->bSortByVolume;
		                                         }))
		{
			return Context->CancelExecution(TEXT("Could not find any points to process."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->MainPoints->StageOutputs();
	Context->Bins->StageOutputs();
	Context->Discarded->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExBestFitPacking
{
	//--------------------------------------------------
	// Rotation Helper Implementation
	//--------------------------------------------------
	
	void FRotationHelper::GetRotationsToTest(EPCGExBestFitRotationMode Mode, TArray<FRotator>& OutRotations)
	{
		OutRotations.Empty();
		
		switch (Mode)
		{
		case EPCGExBestFitRotationMode::None:
			OutRotations.Add(FRotator::ZeroRotator);
			break;
			
		case EPCGExBestFitRotationMode::CardinalOnly:
			// 4 rotations around the up axis (most common for 2.5D packing)
			OutRotations.Add(FRotator(0, 0, 0));
			OutRotations.Add(FRotator(0, 90, 0));
			OutRotations.Add(FRotator(0, 180, 0));
			OutRotations.Add(FRotator(0, 270, 0));
			break;
			
		case EPCGExBestFitRotationMode::AllOrthogonal:
			// All 24 unique orthogonal rotations (cube symmetry group)
			// Face pointing +X
			OutRotations.Add(FRotator(0, 0, 0));
			OutRotations.Add(FRotator(0, 0, 90));
			OutRotations.Add(FRotator(0, 0, 180));
			OutRotations.Add(FRotator(0, 0, 270));
			// Face pointing -X
			OutRotations.Add(FRotator(0, 180, 0));
			OutRotations.Add(FRotator(0, 180, 90));
			OutRotations.Add(FRotator(0, 180, 180));
			OutRotations.Add(FRotator(0, 180, 270));
			// Face pointing +Y
			OutRotations.Add(FRotator(0, 90, 0));
			OutRotations.Add(FRotator(0, 90, 90));
			OutRotations.Add(FRotator(0, 90, 180));
			OutRotations.Add(FRotator(0, 90, 270));
			// Face pointing -Y
			OutRotations.Add(FRotator(0, -90, 0));
			OutRotations.Add(FRotator(0, -90, 90));
			OutRotations.Add(FRotator(0, -90, 180));
			OutRotations.Add(FRotator(0, -90, 270));
			// Face pointing +Z
			OutRotations.Add(FRotator(90, 0, 0));
			OutRotations.Add(FRotator(90, 0, 90));
			OutRotations.Add(FRotator(90, 0, 180));
			OutRotations.Add(FRotator(90, 0, 270));
			// Face pointing -Z
			OutRotations.Add(FRotator(-90, 0, 0));
			OutRotations.Add(FRotator(-90, 0, 90));
			OutRotations.Add(FRotator(-90, 0, 180));
			OutRotations.Add(FRotator(-90, 0, 270));
			break;
		}
	}

	FVector FRotationHelper::RotateSize(const FVector& Size, const FRotator& Rotation)
	{
		if (Rotation.IsNearlyZero()) { return Size; }
		
		// Create a box and rotate it, then get the new axis-aligned size
		const FQuat Quat = Rotation.Quaternion();
		
		// Rotate the 8 corners of the unit box and find the new AABB
		const FVector HalfSize = Size * 0.5;
		FVector Min = FVector(MAX_dbl);
		FVector Max = FVector(-MAX_dbl);
		
		for (int32 i = 0; i < 8; i++)
		{
			FVector Corner(
				(i & 1) ? HalfSize.X : -HalfSize.X,
				(i & 2) ? HalfSize.Y : -HalfSize.Y,
				(i & 4) ? HalfSize.Z : -HalfSize.Z
			);
			
			Corner = Quat.RotateVector(Corner);
			Min = Min.ComponentMin(Corner);
			Max = Max.ComponentMax(Corner);
		}
		
		return Max - Min;
	}

	//--------------------------------------------------
	// FBestFitBin Implementation
	//--------------------------------------------------
	
	void FBestFitBin::AddSpace(const FBox& InBox)
	{
		FSpace& NewSpace = Spaces.Emplace_GetRef(InBox, Seed);
		NewSpace.DistanceScore /= MaxDist;
	}

	void FBestFitBin::RemoveSmallSpaces(double MinSize)
	{
		for (int32 i = Spaces.Num() - 1; i >= 0; i--)
		{
			const FVector Size = Spaces[i].Box.GetSize();
			if (Size.X < MinSize || Size.Y < MinSize || Size.Z < MinSize)
			{
				Spaces.RemoveAt(i);
			}
		}
	}

	FBestFitBin::FBestFitBin(int32 InBinIndex, const PCGExData::FConstPoint& InBinPoint, const FVector& InSeed, const TSharedPtr<FBinSplit>& InSplitter)
	{
		BinIndex = InBinIndex;
		Splitter = InSplitter;
		Seed = InSeed;
		Bounds = PCGExMath::GetLocalBounds<EPCGExPointBoundsSource::ScaledBounds>(InBinPoint);

		Transform = InBinPoint.GetTransform();
		Transform.SetScale3D(FVector::OneVector);

		MaxVolume = Bounds.GetVolume();
		UsedVolume = 0;
		
		FVector FurthestLocation = InSeed;
		for (int C = 0; C < 3; C++)
		{
			const double DistToMin = FMath::Abs(Seed[C] - Bounds.Min[C]);
			const double DistToMax = FMath::Abs(Seed[C] - Bounds.Max[C]);
			FurthestLocation[C] = (DistToMin > DistToMax) ? Bounds.Min[C] : Bounds.Max[C];
		}

		MaxDist = FVector::DistSquared(FurthestLocation, Seed);
		if (MaxDist < KINDA_SMALL_NUMBER) { MaxDist = 1.0; }

		AddSpace(Bounds);
	}

	bool FBestFitBin::EvaluatePlacement(
		const FVector& ItemSize,
		int32 SpaceIndex,
		const FRotator& Rotation,
		FPlacementCandidate& OutCandidate) const
	{
		if (SpaceIndex < 0 || SpaceIndex >= Spaces.Num()) { return false; }
		
		const FSpace& Space = Spaces[SpaceIndex];
		const FVector RotatedSize = FRotationHelper::RotateSize(ItemSize, Rotation);
		
		// Check if it fits
		if (!Space.CanFit(RotatedSize)) { return false; }
		
		const double ItemVolume = RotatedSize.X * RotatedSize.Y * RotatedSize.Z;
		const FVector SpaceSize = Space.Box.GetSize();
		
		// Compute tightness score (how well does the item fill the space?)
		// Lower is better - measures the gap on each axis
		const FVector Gaps = SpaceSize - RotatedSize;
		const double TotalGap = Gaps.X + Gaps.Y + Gaps.Z;
		const double MaxPossibleGap = SpaceSize.X + SpaceSize.Y + SpaceSize.Z;
		const double TightnessScore = MaxPossibleGap > 0 ? TotalGap / MaxPossibleGap : 0;
		
		// Compute waste score (how much space is wasted?)
		const double WasteScore = 1.0 - (ItemVolume / Space.Volume);
		
		// Compute proximity score (distance to seed, normalized)
		const double ProximityScore = Space.DistanceScore;
		
		OutCandidate.BinIndex = BinIndex;
		OutCandidate.SpaceIndex = SpaceIndex;
		OutCandidate.Rotation = Rotation;
		OutCandidate.RotatedSize = RotatedSize;
		OutCandidate.TightnessScore = TightnessScore;
		OutCandidate.WasteScore = WasteScore;
		OutCandidate.ProximityScore = ProximityScore;
		
		return true;
	}

	void FBestFitBin::CommitPlacement(const FPlacementCandidate& Candidate, FBestFitItem& InItem)
	{
		if (!Candidate.IsValid()) { return; }
		
		const FSpace& Space = Spaces[Candidate.SpaceIndex];
		const FVector ItemSize = Candidate.RotatedSize;
		
		// Calculate placement position based on anchor mode
		FVector ItemMin = Space.Box.Min;
		
		switch (Settings->PlacementAnchor)
		{
		case EPCGExBestFitPlacementAnchor::Corner:
			// Place at corner closest to seed
			for (int C = 0; C < 3; C++)
			{
				if (Seed[C] < Space.Box.GetCenter()[C])
				{
					ItemMin[C] = Space.Box.Min[C];
				}
				else
				{
					ItemMin[C] = Space.Box.Max[C] - ItemSize[C];
				}
			}
			break;
			
		case EPCGExBestFitPlacementAnchor::Center:
			ItemMin = Space.Box.GetCenter() - ItemSize * 0.5;
			// Clamp to space bounds
			for (int C = 0; C < 3; C++)
			{
				ItemMin[C] = FMath::Clamp(ItemMin[C], Space.Box.Min[C], Space.Box.Max[C] - ItemSize[C]);
			}
			break;
			
		case EPCGExBestFitPlacementAnchor::SeedProximity:
		default:
			for (int C = 0; C < 3; C++)
			{
				ItemMin[C] = FMath::Clamp(Seed[C] - ItemSize[C] * 0.5, Space.Box.Min[C], Space.Box.Max[C] - ItemSize[C]);
			}
			break;
		}
		
		FBox ItemBox = FBox(ItemMin, ItemMin + ItemSize);
		InItem.Box = ItemBox;
		InItem.Rotation = Candidate.Rotation;
		
		// Store the item after setting all properties
		Items.Add(InItem);
		
		// Expand by padding
		Space.Expand(ItemBox, InItem.Padding);
		
		// Optionally inflate to avoid tiny fragments
		if (Settings->bAvoidWastedSpace)
		{
			Space.Inflate(ItemBox, WastedSpaceThresholds);
		}
		
		// Update used volume
		UsedVolume += ItemSize.X * ItemSize.Y * ItemSize.Z;
		
		// Split the space
		TArray<FBox> NewPartitions;
		Splitter->SplitSpace(Space, ItemBox, NewPartitions);
		
		// Remove the used space
		Spaces.RemoveAt(Candidate.SpaceIndex);
		
		// Add new spaces
		Spaces.Reserve(Spaces.Num() + NewPartitions.Num());
		for (const FBox& Partition : NewPartitions)
		{
			AddSpace(Partition);
		}
		
		// Clean up tiny spaces using the bin's stored MinOccupation
		if (Settings->bAvoidWastedSpace && MinOccupation > 0)
		{
			RemoveSmallSpaces(MinOccupation * Settings->WastedSpaceThreshold);
		}
	}

	void FBestFitBin::UpdatePoint(PCGExData::FMutablePoint& InPoint, const FBestFitItem& InItem) const
	{
		const FTransform ItemTransform = FTransform(
			InItem.Rotation.Quaternion(),
			InItem.Box.GetCenter() - InPoint.GetLocalBounds().GetCenter(),
			InPoint.GetScale3D()
		);
		InPoint.SetTransform(ItemTransform * Transform);
	}

	//--------------------------------------------------
	// FProcessor Implementation
	//--------------------------------------------------

	FPlacementCandidate FProcessor::FindBestPlacement(const FBestFitItem& InItem)
	{
		FPlacementCandidate BestCandidate;
		double BestScore = MAX_dbl;
		
		const FVector OriginalSize = InItem.OriginalSize;
		
		if (Settings->bGlobalBestFit)
		{
			// Global best-fit: Check ALL bins and find the absolute best placement
			for (int32 BinIdx = 0; BinIdx < Bins.Num(); BinIdx++)
			{
				const TSharedPtr<FBestFitBin>& Bin = Bins[BinIdx];
				
				// Iterate through spaces in this bin
				for (int32 SpaceIdx = 0; SpaceIdx < Bin->GetSpaceCount(); SpaceIdx++)
				{
					// Try each rotation
					for (int32 RotIdx = 0; RotIdx < RotationsToTest.Num(); RotIdx++)
					{
						FPlacementCandidate Candidate;
						Candidate.RotationIndex = RotIdx;
						
						if (Bin->EvaluatePlacement(OriginalSize, SpaceIdx, RotationsToTest[RotIdx], Candidate))
						{
							Candidate.Score = ComputeFinalScore(Candidate);
							
							if (Candidate.Score < BestScore)
							{
								BestScore = Candidate.Score;
								BestCandidate = Candidate;
							}
						}
					}
				}
			}
		}
		else
		{
			// Sequential best-fit: Try bins in order, use the first bin where item fits best
			for (int32 BinIdx = 0; BinIdx < Bins.Num(); BinIdx++)
			{
				const TSharedPtr<FBestFitBin>& Bin = Bins[BinIdx];
				BestScore = MAX_dbl;
				BestCandidate = FPlacementCandidate(); // Reset for this bin
				
				// Find best placement in this specific bin
				for (int32 SpaceIdx = 0; SpaceIdx < Bin->GetSpaceCount(); SpaceIdx++)
				{
					for (int32 RotIdx = 0; RotIdx < RotationsToTest.Num(); RotIdx++)
					{
						FPlacementCandidate Candidate;
						Candidate.RotationIndex = RotIdx;
						
						if (Bin->EvaluatePlacement(OriginalSize, SpaceIdx, RotationsToTest[RotIdx], Candidate))
						{
							Candidate.Score = ComputeFinalScore(Candidate);
							
							if (Candidate.Score < BestScore)
							{
								BestScore = Candidate.Score;
								BestCandidate = Candidate;
							}
						}
					}
				}
				
				// If we found a valid placement in this bin, use it
				if (BestCandidate.IsValid())
				{
					break;
				}
			}
		}
		
		return BestCandidate;
	}

	double FProcessor::ComputeFinalScore(const FPlacementCandidate& Candidate) const
	{
		switch (Settings->ScoreMode)
		{
		case EPCGExBestFitScoreMode::TightestFit:
			// Prioritize tight fits - lower tightness score is better
			return Candidate.TightnessScore + Candidate.ProximityScore * 0.1;
			
		case EPCGExBestFitScoreMode::SmallestSpace:
			// Just use the waste score (smaller space = less waste)
			return Candidate.WasteScore;
			
		case EPCGExBestFitScoreMode::LeastWaste:
			// Minimize overall waste
			return Candidate.WasteScore + Candidate.TightnessScore * 0.5;
			
		case EPCGExBestFitScoreMode::Balanced:
		default:
			// Weighted combination
			return Settings->TightnessWeight * Candidate.TightnessScore +
			       (1.0 - Settings->TightnessWeight) * Candidate.WasteScore +
			       Candidate.ProximityScore * 0.1;
		}
	}

	void FProcessor::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TProcessor::RegisterBuffersDependencies(FacadePreloader);

		TArray<FPCGExSortRuleConfig> RuleConfigs;
		if (Settings->GetSortingRules(ExecutionContext, RuleConfigs) && !RuleConfigs.IsEmpty())
		{
			Sorter = MakeShared<PCGExSorting::FSorter>(Context, PointDataFacade, RuleConfigs);
			Sorter->SortDirection = Settings->SortDirection;
		}
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBestFitPacking::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InTaskManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)
		PointDataFacade->Source->GetOut()->AllocateProperties(EPCGPointNativeProperties::Transform);

		TSharedPtr<PCGExData::FPointIO> TargetBins = Context->Bins->Pairs[BatchIndex];
		PCGEX_INIT_IO(TargetBins, PCGExData::EIOInit::Duplicate)

		PaddingBuffer = Settings->GetValueSettingPadding();
		if (!PaddingBuffer->Init(PointDataFacade)) { return false; }

		// Initialize rotations to test
		FRotationHelper::GetRotationsToTest(Settings->RotationMode, RotationsToTest);

		// Create the splitter
#define PCGEX_SWITCH_ON_SPLIT_MODE(_DIRECTION)\
		switch (Settings->SplitMode){\
		case EPCGExSpaceSplitMode::Minimal: Splitter = MakeShared<TBinSplit<_DIRECTION, EPCGExSpaceSplitMode::Minimal>>(); break;\
		case EPCGExSpaceSplitMode::MinimalCross: Splitter = MakeShared<TBinSplit<_DIRECTION, EPCGExSpaceSplitMode::MinimalCross>>(); break;\
		case EPCGExSpaceSplitMode::EqualSplit: Splitter = MakeShared<TBinSplit<_DIRECTION, EPCGExSpaceSplitMode::EqualSplit>>(); break;\
		case EPCGExSpaceSplitMode::Cone: Splitter = MakeShared<TBinSplit<_DIRECTION, EPCGExSpaceSplitMode::Cone>>(); break;\
		case EPCGExSpaceSplitMode::ConeCross: Splitter = MakeShared<TBinSplit<_DIRECTION, EPCGExSpaceSplitMode::ConeCross>>(); break;\
		}

#define PCGEX_SWITCH_ON_SPLIT_DIRECTION(_MACRO) \
		switch (Settings->SplitAxis){ \
		case EPCGExAxis::Forward: _MACRO(EPCGExAxis::Forward) break; \
		case EPCGExAxis::Backward: _MACRO(EPCGExAxis::Backward) break; \
		case EPCGExAxis::Right: _MACRO(EPCGExAxis::Right) break; \
		case EPCGExAxis::Left: _MACRO(EPCGExAxis::Left) break; \
		case EPCGExAxis::Up: _MACRO(EPCGExAxis::Up) break; \
		case EPCGExAxis::Down: _MACRO(EPCGExAxis::Down) break; \
		}

		PCGEX_SWITCH_ON_SPLIT_DIRECTION(PCGEX_SWITCH_ON_SPLIT_MODE)

		Fitted.Init(false, PointDataFacade->GetNum());
		Bins.Reserve(TargetBins->GetNum());

		// Setup seed getter
		bool bRelativeSeed = Settings->SeedMode == EPCGExBinSeedMode::UVWConstant;
		TSharedPtr<PCGExData::TAttributeBroadcaster<FVector>> SeedGetter = MakeShared<PCGExData::TAttributeBroadcaster<FVector>>();
		
		if (Settings->SeedMode == EPCGExBinSeedMode::PositionAttribute)
		{
			if (!SeedGetter->Prepare(Settings->SeedPositionAttribute, TargetBins.ToSharedRef()))
			{
				PCGEX_LOG_INVALID_SELECTOR_C(Context, Seed Position, Settings->SeedPositionAttribute)
				return false;
			}
		}
		else if (Settings->SeedMode == EPCGExBinSeedMode::UVWAttribute)
		{
			bRelativeSeed = true;
			if (!SeedGetter->Prepare(Settings->SeedUVWAttribute, TargetBins.ToSharedRef()))
			{
				PCGEX_LOG_INVALID_SELECTOR_C(Context, Seed UVW, Settings->SeedUVWAttribute)
				return false;
			}
		}
		else
		{
			SeedGetter.Reset();
		}

		const int32 NumPoints = PointDataFacade->GetNum();
		PCGExArrayHelpers::ArrayOfIndices(ProcessingOrder, PointDataFacade->GetNum());

		// Compute min occupation for wasted space threshold (always compute for proper initialization)
		MinOccupation = 0;
		if (Settings->bAvoidWastedSpace && NumPoints > 0)
		{
			MinOccupation = MAX_dbl;
			const UPCGBasePointData* InPoints = PointDataFacade->GetIn();
			for (int i = 0; i < InPoints->GetNumPoints(); i++)
			{
				const FVector Size = PCGExMath::GetLocalBounds<EPCGExPointBoundsSource::ScaledBounds>(PCGExData::FConstPoint(InPoints, i)).GetSize();
				MinOccupation = FMath::Min(MinOccupation, FMath::Min3(Size.X, Size.Y, Size.Z));
			}
			
			// Ensure we don't have an invalid value
			if (MinOccupation == MAX_dbl) { MinOccupation = 0; }
		}

		// Sort by volume if enabled (Best-Fit Decreasing approach)
		if (Settings->bSortByVolume)
		{
			const UPCGBasePointData* InPoints = PointDataFacade->GetIn();
			
			// Create volume array
			TArray<double> Volumes;
			Volumes.SetNum(NumPoints);
			
			for (int32 i = 0; i < NumPoints; i++)
			{
				const FVector Size = PCGExMath::GetLocalBounds<EPCGExPointBoundsSource::ScaledBounds>(PCGExData::FConstPoint(InPoints, i)).GetSize();
				Volumes[i] = Size.X * Size.Y * Size.Z;
			}
			
			// Sort indices by volume (descending for BFD)
			if (Settings->SortDirection == EPCGExSortDirection::Descending)
			{
				ProcessingOrder.Sort([&Volumes](const int32 A, const int32 B) { return Volumes[A] > Volumes[B]; });
			}
			else
			{
				ProcessingOrder.Sort([&Volumes](const int32 A, const int32 B) { return Volumes[A] < Volumes[B]; });
			}
		}
		else if (Sorter && Sorter->Init(Context))
		{
			// Use custom sorting rules
			if (TSharedPtr<PCGExSorting::FSortCache> Cache = Sorter->BuildCache(NumPoints))
			{
				ProcessingOrder.Sort([&](const int32 A, const int32 B) { return Cache->Compare(A, B); });
			}
			else
			{
				ProcessingOrder.Sort([&](const int32 A, const int32 B) { return Sorter->Sort(A, B); });
			}
		}

		// Create bins
		for (int i = 0; i < TargetBins->GetNum(); i++)
		{
			const PCGExData::FConstPoint BinPoint = TargetBins->GetInPoint(i);

			FVector Seed = FVector::ZeroVector;
			if (bRelativeSeed)
			{
				FBox Box = PCGExMath::GetLocalBounds<EPCGExPointBoundsSource::ScaledBounds>(BinPoint);
				Seed = Box.GetCenter() + (SeedGetter ? SeedGetter->FetchSingle(BinPoint, FVector::ZeroVector) : Settings->SeedUVW) * Box.GetExtent();
			}
			else
			{
				Seed = BinPoint.GetTransform().InverseTransformPositionNoScale(SeedGetter ? SeedGetter->FetchSingle(BinPoint, FVector::ZeroVector) : Settings->SeedPosition);
			}

			PCGEX_MAKE_SHARED(NewBin, FBestFitBin, i, BinPoint, Seed, Splitter)

			NewBin->Settings = Settings;
			NewBin->SetMinOccupation(MinOccupation);
			NewBin->WastedSpaceThresholds = FVector(MinOccupation * Settings->WastedSpaceThreshold);

			Bins.Add(NewBin);
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::BestFitPacking::ProcessPoints);

		PointDataFacade->Fetch(Scope);

		UPCGBasePointData* OutPointData = PointDataFacade->GetOut();

		PCGEX_SCOPE_LOOP(Index)
		{
			const int32 PointIndex = ProcessingOrder[Index];
			PCGExData::FMutablePoint Point(OutPointData, PointIndex);

			const FVector PointSize = PCGExMath::GetLocalBounds<EPCGExPointBoundsSource::ScaledBounds>(Point).GetSize();

			FBestFitItem Item;
			Item.Index = PointIndex;
			Item.Box = FBox(FVector::ZeroVector, PointSize);
			Item.OriginalSize = PointSize;
			Item.Padding = PaddingBuffer->Read(PointIndex);

			// Find the globally best placement
			FPlacementCandidate BestPlacement = FindBestPlacement(Item);

			bool bPlaced = false;
			if (BestPlacement.IsValid())
			{
				TSharedPtr<FBestFitBin>& Bin = Bins[BestPlacement.BinIndex];
				Bin->CommitPlacement(BestPlacement, Item);
				Bin->UpdatePoint(Point, Item);
				bPlaced = true;
			}

			Fitted[PointIndex] = bPlaced;
			if (!bPlaced) { bHasUnfitted = true; }
		}
	}

	void FProcessor::CompleteWork()
	{
		if (bHasUnfitted)
		{
			(void)PointDataFacade->Source->Gather(Fitted);

			if (const TSharedPtr<PCGExData::FPointIO> Discarded = Context->Discarded->Emplace_GetRef(PointDataFacade->GetIn(), PCGExData::EIOInit::New))
			{
				(void)Discarded->InheritPoints(Fitted, true);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE