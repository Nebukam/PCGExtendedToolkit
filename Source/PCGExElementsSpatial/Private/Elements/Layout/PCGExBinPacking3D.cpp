// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/Layout/PCGExBinPacking3D.h"

#include "Data/PCGExAttributeBroadcaster.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExSettingsDetails.h"
#include "Helpers/PCGExArrayHelpers.h"
#include "Elements/Layout/PCGExLayout.h"
#include "Math/PCGExMathBounds.h"
#include "Sorting/PCGExPointSorter.h"
#include "Sorting/PCGExSortingDetails.h"

#define LOCTEXT_NAMESPACE "PCGExBinPacking3DElement"
#define PCGEX_NAMESPACE BinPacking3D

PCGEX_SETTING_VALUE_IMPL(UPCGExBinPacking3DSettings, Padding, FVector, OccupationPaddingInput, OccupationPaddingAttribute, OccupationPadding)
PCGEX_SETTING_VALUE_IMPL(UPCGExBinPacking3DSettings, ItemWeight, double, ItemWeightInput, ItemWeightAttribute, ItemWeight)

#pragma region UPCGExBinPacking3DSettings

bool UPCGExBinPacking3DSettings::GetSortingRules(FPCGExContext* InContext, TArray<FPCGExSortRuleConfig>& OutRules) const
{
	OutRules.Append(PCGExSorting::GetSortingRules(InContext, PCGExSorting::Labels::SourceSortingRules));
	return !OutRules.IsEmpty();
}

TArray<FPCGPinProperties> UPCGExBinPacking3DSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINTS(PCGExLayout::SourceBinsLabel, "List of bins to fit input points into. Each input collection is expected to have a matching collection of bins.", Required)
	PCGExSorting::DeclareSortingRulesInputs(PinProperties, EPCGPinStatus::Normal);
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExBinPacking3DSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExLayout::OutputBinsLabel, "Input bins, with added statistics.", Required)
	PCGEX_PIN_POINTS(PCGExCommon::Labels::OutputDiscardedLabel, "Discarded points, ones that could not fit into any bin.", Required)
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(BinPacking3D)

PCGExData::EIOInit UPCGExBinPacking3DSettings::GetMainDataInitializationPolicy() const { return PCGExData::EIOInit::Duplicate; }

#pragma endregion

#pragma region FPCGExBinPacking3DElement

PCGEX_ELEMENT_BATCH_POINT_IMPL(BinPacking3D)

bool FPCGExBinPacking3DElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BinPacking3D)

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

bool FPCGExBinPacking3DElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBinPacking3DElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BinPacking3D)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
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

#pragma endregion

namespace PCGExBinPacking3D
{
#pragma region FBP3DRotationHelper

	void FBP3DRotationHelper::GetPaper6Rotations(const FVector& ItemSize, TArray<FRotator>& OutRotations)
	{
		OutRotations.Empty();

		const double L = ItemSize.X;
		const double W = ItemSize.Y;
		const double H = ItemSize.Z;

		const bool bLW = FMath::IsNearlyEqual(L, W, KINDA_SMALL_NUMBER);
		const bool bLH = FMath::IsNearlyEqual(L, H, KINDA_SMALL_NUMBER);
		const bool bWH = FMath::IsNearlyEqual(W, H, KINDA_SMALL_NUMBER);

		// The 6 axis permutations map dimensions (L,W,H) to axes (X,Y,Z):
		// Orientation 1: (L,W,H) -> identity
		// Orientation 2: (L,H,W) -> swap Y,Z
		// Orientation 3: (W,L,H) -> swap X,Y
		// Orientation 4: (W,H,L) -> cycle
		// Orientation 5: (H,L,W) -> cycle
		// Orientation 6: (H,W,L) -> swap X,Z

		// Always include identity
		OutRotations.Add(FRotator(0, 0, 0)); // (L,W,H)

		if (bLW && bLH)
		{
			// Cube: all dimensions equal, only 1 unique orientation
			return;
		}

		if (bLW)
		{
			// L==W, H different: square prism along Z
			// (L,W,H) = (W,L,H) -> skip orientation 3
			// (L,H,W) and (W,H,L) -> equivalent -> keep one
			// (H,L,W) and (H,W,L) -> equivalent -> keep one
			OutRotations.Add(FRotator(90, 0, 0));  // (L,H,W) - pitch 90
			OutRotations.Add(FRotator(0, 0, 90));  // (H,L,W) - roll 90
			return;
		}

		if (bLH)
		{
			// L==H, W different: square prism along Y
			// (L,W,H) = (H,W,L) -> skip orientation 6
			// (L,H,W) and (H,L,W) -> equivalent -> keep one
			// (W,L,H) and (W,H,L) -> equivalent -> keep one
			OutRotations.Add(FRotator(90, 0, 0));  // (L,H,W)
			OutRotations.Add(FRotator(0, 90, 0));  // (W,L,H) - yaw 90
			return;
		}

		if (bWH)
		{
			// W==H, L different: square prism along X
			// (L,W,H) = (L,H,W) -> skip orientation 2
			// (W,L,H) and (H,L,W) -> equivalent -> keep one
			// (W,H,L) and (H,W,L) -> equivalent -> keep one
			OutRotations.Add(FRotator(0, 90, 0));   // (W,L,H)
			OutRotations.Add(FRotator(0, 90, 90));   // (W,H,L)
			return;
		}

		// All dimensions different: 6 unique orientations
		OutRotations.Add(FRotator(90, 0, 0));    // (L,H,W)
		OutRotations.Add(FRotator(0, 90, 0));    // (W,L,H)
		OutRotations.Add(FRotator(0, 90, 90));   // (W,H,L)
		OutRotations.Add(FRotator(90, 90, 0));   // (H,L,W)
		OutRotations.Add(FRotator(0, 0, 90));    // (H,W,L)
	}

	void FBP3DRotationHelper::GetRotationsToTest(EPCGExBP3DRotationMode Mode, TArray<FRotator>& OutRotations)
	{
		OutRotations.Empty();

		switch (Mode)
		{
		case EPCGExBP3DRotationMode::None:
			OutRotations.Add(FRotator::ZeroRotator);
			break;

		case EPCGExBP3DRotationMode::CardinalOnly:
			OutRotations.Add(FRotator(0, 0, 0));
			OutRotations.Add(FRotator(0, 90, 0));
			OutRotations.Add(FRotator(0, 180, 0));
			OutRotations.Add(FRotator(0, 270, 0));
			break;

		case EPCGExBP3DRotationMode::Paper6:
			// For Paper6 mode, we generate per-item (see FindBestPlacement).
			// Fallback to identity here.
			OutRotations.Add(FRotator::ZeroRotator);
			break;

		case EPCGExBP3DRotationMode::AllOrthogonal:
			// All 24 unique orthogonal rotations
			OutRotations.Add(FRotator(0, 0, 0));
			OutRotations.Add(FRotator(0, 0, 90));
			OutRotations.Add(FRotator(0, 0, 180));
			OutRotations.Add(FRotator(0, 0, 270));
			OutRotations.Add(FRotator(0, 180, 0));
			OutRotations.Add(FRotator(0, 180, 90));
			OutRotations.Add(FRotator(0, 180, 180));
			OutRotations.Add(FRotator(0, 180, 270));
			OutRotations.Add(FRotator(0, 90, 0));
			OutRotations.Add(FRotator(0, 90, 90));
			OutRotations.Add(FRotator(0, 90, 180));
			OutRotations.Add(FRotator(0, 90, 270));
			OutRotations.Add(FRotator(0, -90, 0));
			OutRotations.Add(FRotator(0, -90, 90));
			OutRotations.Add(FRotator(0, -90, 180));
			OutRotations.Add(FRotator(0, -90, 270));
			OutRotations.Add(FRotator(90, 0, 0));
			OutRotations.Add(FRotator(90, 0, 90));
			OutRotations.Add(FRotator(90, 0, 180));
			OutRotations.Add(FRotator(90, 0, 270));
			OutRotations.Add(FRotator(-90, 0, 0));
			OutRotations.Add(FRotator(-90, 0, 90));
			OutRotations.Add(FRotator(-90, 0, 180));
			OutRotations.Add(FRotator(-90, 0, 270));
			break;
		}
	}

	FVector FBP3DRotationHelper::RotateSize(const FVector& Size, const FRotator& Rotation)
	{
		if (Rotation.IsNearlyZero()) { return Size; }

		const FQuat Quat = Rotation.Quaternion();
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

#pragma endregion

#pragma region FBP3DBin

	void FBP3DBin::AddSpace(const FBox& InBox)
	{
		FSpace& NewSpace = Spaces.Emplace_GetRef(InBox, Seed);
		NewSpace.DistanceScore /= MaxDist;
	}

	void FBP3DBin::RemoveSmallSpaces(double MinSize)
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

	FBP3DBin::FBP3DBin(int32 InBinIndex, const PCGExData::FConstPoint& InBinPoint, const FVector& InSeed, const TSharedPtr<FBinSplit>& InSplitter)
	{
		BinIndex = InBinIndex;
		Splitter = InSplitter;
		Seed = InSeed;
		Bounds = PCGExMath::GetLocalBounds<EPCGExPointBoundsSource::ScaledBounds>(InBinPoint);

		Transform = InBinPoint.GetTransform();
		Transform.SetScale3D(FVector::OneVector);

		MaxVolume = Bounds.GetVolume();
		UsedVolume = 0;
		CurrentWeight = 0;

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

	bool FBP3DBin::EvaluatePlacement(
		const FVector& ItemSize,
		int32 SpaceIndex,
		const FRotator& Rotation,
		FBP3DPlacementCandidate& OutCandidate) const
	{
		if (SpaceIndex < 0 || SpaceIndex >= Spaces.Num()) { return false; }

		const FSpace& Space = Spaces[SpaceIndex];
		const FVector RotatedSize = FBP3DRotationHelper::RotateSize(ItemSize, Rotation);

		if (!Space.CanFit(RotatedSize)) { return false; }

		const double ItemVolume = RotatedSize.X * RotatedSize.Y * RotatedSize.Z;
		const FVector SpaceSize = Space.Box.GetSize();

		// Tightness score: how well does the item fill the space? (lower is better)
		const FVector Gaps = SpaceSize - RotatedSize;
		const double TotalGap = Gaps.X + Gaps.Y + Gaps.Z;
		const double MaxPossibleGap = SpaceSize.X + SpaceSize.Y + SpaceSize.Z;
		const double TightnessScore = MaxPossibleGap > 0 ? TotalGap / MaxPossibleGap : 0;

		// Waste score
		const double WasteScore = 1.0 - (ItemVolume / Space.Volume);

		// Proximity score (distance to seed, normalized)
		const double ProximityScore = Space.DistanceScore;

		// Placement position (corner closest to seed)
		FVector ItemMin = Space.Box.Min;
		for (int C = 0; C < 3; C++)
		{
			if (Seed[C] < Space.Box.GetCenter()[C])
			{
				ItemMin[C] = Space.Box.Min[C];
			}
			else
			{
				ItemMin[C] = Space.Box.Max[C] - RotatedSize[C];
			}
		}

		const FVector BinSize = Bounds.GetSize();

		// Paper objective o2: Height score (normalized Z position, lower is better for floor-up)
		const double NormalizedZ = BinSize.Z > KINDA_SMALL_NUMBER
			                           ? (ItemMin.Z - Bounds.Min.Z) / BinSize.Z
			                           : 0.0;

		// Paper objective o1: Bin usage score (higher fill ratio is better, so invert for minimization)
		const double CurrentFillRatio = MaxVolume > 0 ? (UsedVolume + ItemVolume) / MaxVolume : 0;
		const double BinUsageScore = 1.0 - CurrentFillRatio;

		// Paper objective o3: Load balance score (Manhattan distance to target CoM = bin center, normalized)
		const FVector ItemCenter = ItemMin + RotatedSize * 0.5;
		const FVector BinCenter = Bounds.GetCenter();
		const FVector Diff = (ItemCenter - BinCenter).GetAbs();
		const FVector BinExtent = Bounds.GetExtent();
		const double MaxManhattan = BinExtent.X + BinExtent.Y + BinExtent.Z;
		const double ManhattanDist = Diff.X + Diff.Y + Diff.Z;
		const double LoadBalanceScore = MaxManhattan > KINDA_SMALL_NUMBER ? ManhattanDist / MaxManhattan : 0.0;

		OutCandidate.BinIndex = BinIndex;
		OutCandidate.SpaceIndex = SpaceIndex;
		OutCandidate.Rotation = Rotation;
		OutCandidate.RotatedSize = RotatedSize;
		OutCandidate.TightnessScore = TightnessScore;
		OutCandidate.WasteScore = WasteScore;
		OutCandidate.ProximityScore = ProximityScore;
		OutCandidate.BinUsageScore = BinUsageScore;
		OutCandidate.HeightScore = NormalizedZ;
		OutCandidate.LoadBalanceScore = LoadBalanceScore;

		return true;
	}

	bool FBP3DBin::CheckLoadBearing(const FBP3DPlacementCandidate& Candidate, double ItemWeight, double Threshold) const
	{
		if (Items.IsEmpty()) { return true; }

		// Compute the placement box for the candidate
		const FSpace& Space = Spaces[Candidate.SpaceIndex];
		FVector ItemMin = Space.Box.Min;
		const FVector& RotatedSize = Candidate.RotatedSize;

		for (int C = 0; C < 3; C++)
		{
			if (Seed[C] < Space.Box.GetCenter()[C])
			{
				ItemMin[C] = Space.Box.Min[C];
			}
			else
			{
				ItemMin[C] = Space.Box.Max[C] - RotatedSize[C];
			}
		}

		const FBox CandidateBox(ItemMin, ItemMin + RotatedSize);

		// Check: if this item is placed ABOVE an existing item, its weight must be <= threshold * supporting item weight
		for (const FBP3DItem& Existing : Items)
		{
			// Check if candidate is above existing: candidate Z min >= existing Z max (or near it)
			// and they overlap in XY
			const bool bAbove = CandidateBox.Min.Z >= Existing.Box.Max.Z - KINDA_SMALL_NUMBER;

			if (!bAbove) { continue; }

			// Check XY overlap
			const bool bXOverlap = CandidateBox.Min.X < Existing.Box.Max.X && CandidateBox.Max.X > Existing.Box.Min.X;
			const bool bYOverlap = CandidateBox.Min.Y < Existing.Box.Max.Y && CandidateBox.Max.Y > Existing.Box.Min.Y;

			if (bXOverlap && bYOverlap)
			{
				// This existing item supports the candidate
				if (ItemWeight > Threshold * Existing.Weight)
				{
					return false; // Too heavy for the support
				}
			}
		}

		return true;
	}

	void FBP3DBin::CommitPlacement(const FBP3DPlacementCandidate& Candidate, FBP3DItem& InItem)
	{
		if (!Candidate.IsValid()) { return; }

		const FSpace& Space = Spaces[Candidate.SpaceIndex];
		const FVector ItemSize = Candidate.RotatedSize;

		// Calculate placement position (corner closest to seed)
		FVector ItemMin = Space.Box.Min;
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

		FBox ItemBox = FBox(ItemMin, ItemMin + ItemSize);
		InItem.Box = ItemBox;
		InItem.Rotation = Candidate.Rotation;

		// Update weight and category tracking
		CurrentWeight += InItem.Weight;
		if (InItem.Category >= 0)
		{
			PresentCategories.Add(InItem.Category);
		}

		Items.Add(InItem);

		// Expand by padding
		Space.Expand(ItemBox, InItem.Padding);

		// Optionally inflate to avoid tiny fragments
		if (Settings->bAvoidWastedSpace)
		{
			Space.Inflate(ItemBox, WastedSpaceThresholds);
		}

		UsedVolume += ItemSize.X * ItemSize.Y * ItemSize.Z;

		// Split the space
		TArray<FBox> NewPartitions;
		Splitter->SplitSpace(Space, ItemBox, NewPartitions);

		Spaces.RemoveAt(Candidate.SpaceIndex);

		Spaces.Reserve(Spaces.Num() + NewPartitions.Num());
		for (const FBox& Partition : NewPartitions)
		{
			AddSpace(Partition);
		}

		if (Settings->bAvoidWastedSpace && MinOccupation > 0)
		{
			RemoveSmallSpaces(MinOccupation * Settings->WastedSpaceThreshold);
		}
	}

	void FBP3DBin::UpdatePoint(PCGExData::FMutablePoint& InPoint, const FBP3DItem& InItem) const
	{
		const FTransform ItemTransform = FTransform(
			InItem.Rotation.Quaternion(),
			InItem.Box.GetCenter() - InPoint.GetLocalBounds().GetCenter(),
			InPoint.GetScale3D()
		);
		InPoint.SetTransform(ItemTransform * Transform);
	}

#pragma endregion

#pragma region FProcessor

	void FProcessor::BuildAffinityLookups()
	{
		NegativeAffinityPairs.Empty();
		PositiveAffinityGroup.Empty();

		if (!Settings->bEnableAffinities || Settings->AffinityRules.IsEmpty()) { return; }

		// Union-find parent map for positive affinities
		TMap<int32, int32> Parent;

		auto FindRoot = [&Parent](int32 X) -> int32
		{
			while (Parent.Contains(X) && Parent[X] != X)
			{
				Parent[X] = Parent[Parent[X]]; // Path compression
				X = Parent[X];
			}
			return X;
		};

		auto Union = [&Parent, &FindRoot](int32 A, int32 B)
		{
			const int32 RootA = FindRoot(A);
			const int32 RootB = FindRoot(B);
			if (RootA != RootB)
			{
				Parent.Add(RootA, RootB);
			}
		};

		for (const FPCGExBP3DAffinityRule& Rule : Settings->AffinityRules)
		{
			if (Rule.Type == EPCGExBP3DAffinityType::Negative)
			{
				NegativeAffinityPairs.Add(MakeAffinityKey(Rule.CategoryA, Rule.CategoryB));
			}
			else
			{
				// Positive affinity: union the two categories
				if (!Parent.Contains(Rule.CategoryA)) { Parent.Add(Rule.CategoryA, Rule.CategoryA); }
				if (!Parent.Contains(Rule.CategoryB)) { Parent.Add(Rule.CategoryB, Rule.CategoryB); }
				Union(Rule.CategoryA, Rule.CategoryB);
			}
		}

		// Flatten union-find into group map
		for (auto& Pair : Parent)
		{
			PositiveAffinityGroup.Add(Pair.Key, FindRoot(Pair.Key));
		}
	}

	uint64 FProcessor::MakeAffinityKey(int32 A, int32 B)
	{
		// Canonical order so (A,B) == (B,A)
		if (A > B) { Swap(A, B); }
		return (static_cast<uint64>(static_cast<uint32>(A)) << 32) | static_cast<uint64>(static_cast<uint32>(B));
	}

	bool FProcessor::HasNegativeAffinity(int32 CatA, int32 CatB) const
	{
		return NegativeAffinityPairs.Contains(MakeAffinityKey(CatA, CatB));
	}

	int32 FProcessor::FindPositiveGroup(int32 Category) const
	{
		const int32* Group = PositiveAffinityGroup.Find(Category);
		return Group ? *Group : -1;
	}

	bool FProcessor::IsCategoryCompatibleWithBin(int32 ItemCategory, const FBP3DBin& Bin) const
	{
		if (ItemCategory < 0) { return true; }

		// Check negative affinities: item category must not conflict with any present category
		for (const int32 PresentCat : Bin.PresentCategories)
		{
			if (HasNegativeAffinity(ItemCategory, PresentCat))
			{
				return false;
			}
		}

		return true;
	}

	int32 FProcessor::FindRequiredBinForPositiveAffinity(int32 ItemCategory) const
	{
		if (ItemCategory < 0) { return -1; }

		const int32 ItemGroup = FindPositiveGroup(ItemCategory);
		if (ItemGroup < 0) { return -1; }

		// Search bins for one that already contains an item from the same positive affinity group
		for (int32 BinIdx = 0; BinIdx < Bins.Num(); BinIdx++)
		{
			const TSharedPtr<FBP3DBin>& Bin = Bins[BinIdx];
			for (const int32 PresentCat : Bin->PresentCategories)
			{
				if (FindPositiveGroup(PresentCat) == ItemGroup)
				{
					return BinIdx;
				}
			}
		}

		return -1; // No bin has items from this group yet
	}

	FBP3DPlacementCandidate FProcessor::FindBestPlacement(const FBP3DItem& InItem)
	{
		FBP3DPlacementCandidate BestCandidate;
		double BestScore = MAX_dbl;

		const FVector OriginalSize = InItem.OriginalSize;

		// Generate rotations for this item
		TArray<FRotator> RotationsToTest;
		if (Settings->RotationMode == EPCGExBP3DRotationMode::Paper6)
		{
			FBP3DRotationHelper::GetPaper6Rotations(OriginalSize, RotationsToTest);
		}
		else
		{
			FBP3DRotationHelper::GetRotationsToTest(Settings->RotationMode, RotationsToTest);
		}

		// Positive affinity: if item belongs to a group that's already placed, restrict to that bin
		const int32 RequiredBin = Settings->bEnableAffinities ? FindRequiredBinForPositiveAffinity(InItem.Category) : -1;

		auto EvaluateBin = [&](int32 BinIdx)
		{
			const TSharedPtr<FBP3DBin>& Bin = Bins[BinIdx];

			// Weight constraint pre-check
			if (Settings->bEnableWeightConstraint)
			{
				if (Bin->CurrentWeight + InItem.Weight > Bin->MaxWeight)
				{
					return;
				}
			}

			// Negative affinity pre-check
			if (Settings->bEnableAffinities && InItem.Category >= 0)
			{
				if (!IsCategoryCompatibleWithBin(InItem.Category, *Bin))
				{
					return;
				}
			}

			for (int32 SpaceIdx = 0; SpaceIdx < Bin->GetSpaceCount(); SpaceIdx++)
			{
				for (int32 RotIdx = 0; RotIdx < RotationsToTest.Num(); RotIdx++)
				{
					FBP3DPlacementCandidate Candidate;
					Candidate.RotationIndex = RotIdx;

					if (Bin->EvaluatePlacement(OriginalSize, SpaceIdx, RotationsToTest[RotIdx], Candidate))
					{
						// Load bearing post-check
						if (Settings->bEnableLoadBearing)
						{
							if (!Bin->CheckLoadBearing(Candidate, InItem.Weight, Settings->LoadBearingThreshold))
							{
								continue;
							}
						}

						Candidate.Score = ComputeFinalScore(Candidate);

						if (Candidate.Score < BestScore)
						{
							BestScore = Candidate.Score;
							BestCandidate = Candidate;
						}
					}
				}
			}
		};

		if (Settings->bGlobalBestFit)
		{
			if (RequiredBin >= 0)
			{
				// Positive affinity restricts to one bin
				EvaluateBin(RequiredBin);
			}
			else
			{
				for (int32 BinIdx = 0; BinIdx < Bins.Num(); BinIdx++)
				{
					EvaluateBin(BinIdx);
				}
			}
		}
		else
		{
			// Sequential: try bins in order, use first that works
			if (RequiredBin >= 0)
			{
				EvaluateBin(RequiredBin);
			}
			else
			{
				for (int32 BinIdx = 0; BinIdx < Bins.Num(); BinIdx++)
				{
					BestScore = MAX_dbl;
					BestCandidate = FBP3DPlacementCandidate();

					EvaluateBin(BinIdx);

					if (BestCandidate.IsValid())
					{
						break;
					}
				}
			}
		}

		return BestCandidate;
	}

	double FProcessor::ComputeFinalScore(const FBP3DPlacementCandidate& Candidate) const
	{
		// Geometric quality score (same as BestFitPacking TightestFit + proximity)
		const double GeometricScore = Candidate.TightnessScore + Candidate.ProximityScore * 0.1;

		// Paper multi-objective score
		const double PaperScore =
			Settings->ObjectiveWeightBinUsage * Candidate.BinUsageScore +
			Settings->ObjectiveWeightHeight * Candidate.HeightScore +
			Settings->ObjectiveWeightLoadBalance * Candidate.LoadBalanceScore;

		// Blend geometric and paper scores
		return GeometricScore * 0.5 + PaperScore * 0.5;
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
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBinPacking3D::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InTaskManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)
		PointDataFacade->Source->GetOut()->AllocateProperties(EPCGPointNativeProperties::Transform);

		TSharedPtr<PCGExData::FPointIO> TargetBins = Context->Bins->Pairs[BatchIndex];
		PCGEX_INIT_IO(TargetBins, PCGExData::EIOInit::Duplicate)

		PaddingBuffer = Settings->GetValueSettingPadding();
		if (!PaddingBuffer->Init(PointDataFacade)) { return false; }

		// Initialize item weight buffer
		if (Settings->bEnableWeightConstraint)
		{
			ItemWeightBuffer = Settings->GetValueSettingItemWeight();
			if (!ItemWeightBuffer->Init(PointDataFacade)) { return false; }
		}

		// Build affinity lookups
		if (Settings->bEnableAffinities)
		{
			BuildAffinityLookups();
		}

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

		const int32 NumPoints = PointDataFacade->GetNum();

		Fitted.Init(false, NumPoints);
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

		// Setup bin max weight getter
		TSharedPtr<PCGExData::TAttributeBroadcaster<double>> BinMaxWeightGetter;
		if (Settings->bEnableWeightConstraint && Settings->BinMaxWeightInput == EPCGExInputValueType::Attribute)
		{
			BinMaxWeightGetter = MakeShared<PCGExData::TAttributeBroadcaster<double>>();
			if (!BinMaxWeightGetter->Prepare(Settings->BinMaxWeightAttribute, TargetBins.ToSharedRef()))
			{
				PCGEX_LOG_INVALID_SELECTOR_C(Context, Bin Max Weight, Settings->BinMaxWeightAttribute)
				return false;
			}
		}

		// Setup category getter
		TSharedPtr<PCGExData::TAttributeBroadcaster<int32>> CategoryGetter;
		if (Settings->bEnableAffinities)
		{
			CategoryGetter = MakeShared<PCGExData::TAttributeBroadcaster<int32>>();
			if (!CategoryGetter->Prepare(Settings->CategoryAttribute, PointDataFacade->Source))
			{
				PCGEX_LOG_INVALID_SELECTOR_C(Context, Category, Settings->CategoryAttribute)
				return false;
			}
		}

		PCGExArrayHelpers::ArrayOfIndices(ProcessingOrder, NumPoints);

		// Compute min occupation for wasted space threshold
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

			if (MinOccupation == MAX_dbl) { MinOccupation = 0; }
		}

		// Pre-read per-item data
		{
			const UPCGBasePointData* InPoints = PointDataFacade->GetIn();

			// Item weights
			ItemWeights.SetNum(NumPoints);
			if (Settings->bEnableWeightConstraint && ItemWeightBuffer)
			{
				for (int32 i = 0; i < NumPoints; i++)
				{
					ItemWeights[i] = ItemWeightBuffer->Read(i);
				}
			}
			else
			{
				for (int32 i = 0; i < NumPoints; i++)
				{
					ItemWeights[i] = 0.0;
				}
			}

			// Item categories
			ItemCategories.SetNum(NumPoints);
			if (Settings->bEnableAffinities && CategoryGetter)
			{
				for (int32 i = 0; i < NumPoints; i++)
				{
					ItemCategories[i] = CategoryGetter->FetchSingle(PCGExData::FConstPoint(InPoints, i), -1);
				}
			}
			else
			{
				for (int32 i = 0; i < NumPoints; i++)
				{
					ItemCategories[i] = -1;
				}
			}
		}

		// Sort by volume if enabled (Best-Fit Decreasing approach)
		if (Settings->bSortByVolume)
		{
			const UPCGBasePointData* InPoints = PointDataFacade->GetIn();

			TArray<double> Volumes;
			Volumes.SetNum(NumPoints);

			for (int32 i = 0; i < NumPoints; i++)
			{
				const FVector Size = PCGExMath::GetLocalBounds<EPCGExPointBoundsSource::ScaledBounds>(PCGExData::FConstPoint(InPoints, i)).GetSize();
				Volumes[i] = Size.X * Size.Y * Size.Z;
			}

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
		BinMaxWeights.SetNum(TargetBins->GetNum());
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

			PCGEX_MAKE_SHARED(NewBin, FBP3DBin, i, BinPoint, Seed, Splitter)

			NewBin->Settings = Settings;
			NewBin->SetMinOccupation(MinOccupation);
			NewBin->WastedSpaceThresholds = FVector(MinOccupation * Settings->WastedSpaceThreshold);

			// Set bin max weight
			if (Settings->bEnableWeightConstraint)
			{
				if (BinMaxWeightGetter)
				{
					NewBin->MaxWeight = BinMaxWeightGetter->FetchSingle(BinPoint, Settings->BinMaxWeight);
				}
				else
				{
					NewBin->MaxWeight = Settings->BinMaxWeight;
				}
			}
			else
			{
				NewBin->MaxWeight = MAX_dbl;
			}

			BinMaxWeights[i] = NewBin->MaxWeight;
			Bins.Add(NewBin);
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::BinPacking3D::ProcessPoints);

		PointDataFacade->Fetch(Scope);

		UPCGBasePointData* OutPointData = PointDataFacade->GetOut();

		PCGEX_SCOPE_LOOP(Index)
		{
			const int32 PointIndex = ProcessingOrder[Index];
			PCGExData::FMutablePoint Point(OutPointData, PointIndex);

			const FVector PointSize = PCGExMath::GetLocalBounds<EPCGExPointBoundsSource::ScaledBounds>(Point).GetSize();

			FBP3DItem Item;
			Item.Index = PointIndex;
			Item.Box = FBox(FVector::ZeroVector, PointSize);
			Item.OriginalSize = PointSize;
			Item.Padding = PaddingBuffer->Read(PointIndex);
			Item.Weight = ItemWeights[PointIndex];
			Item.Category = ItemCategories[PointIndex];

			FBP3DPlacementCandidate BestPlacement = FindBestPlacement(Item);

			bool bPlaced = false;
			if (BestPlacement.IsValid())
			{
				TSharedPtr<FBP3DBin>& Bin = Bins[BestPlacement.BinIndex];
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

#pragma endregion
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
