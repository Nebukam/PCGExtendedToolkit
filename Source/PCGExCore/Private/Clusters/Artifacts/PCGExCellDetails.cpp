// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Clusters/Artifacts/PCGExCellDetails.h"

#include "Clusters/Artifacts/PCGExCell.h"
#include "Core/PCGExContext.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointElements.h"
#include "Data/PCGExPointIO.h"
#include "Clusters/PCGExCluster.h"
#include "Clusters/PCGExClusterCommon.h"
#include "Paths/PCGExPathsHelpers.h"
#include "Math/PCGExBestFitPlane.h"
#include "Data/PCGPointArrayData.h"

void FPCGExCellSeedMutationDetails::ApplyToPoint(const PCGExClusters::FCell* InCell, PCGExData::FMutablePoint& OutSeedPoint, const UPCGBasePointData* CellPoints) const
{
	switch (Location)
	{
	default: case EPCGExCellSeedLocation::Original: break;
	case EPCGExCellSeedLocation::Centroid: OutSeedPoint.SetLocation(InCell->Data.Centroid);
		break;
	case EPCGExCellSeedLocation::PathBoundsCenter: OutSeedPoint.SetLocation(InCell->Data.Bounds.GetCenter());
		break;
	case EPCGExCellSeedLocation::FirstNode: OutSeedPoint.SetLocation(CellPoints->GetTransform(0).GetLocation());
		break;
	case EPCGExCellSeedLocation::LastNode: OutSeedPoint.SetLocation(CellPoints->GetTransform(CellPoints->GetNumPoints() - 1).GetLocation());
		break;
	}

	if (bResetScale) { OutSeedPoint.SetScale3D(FVector::OneVector); }

	if (bResetRotation) { OutSeedPoint.SetRotation(FQuat::Identity); }

	if (bMatchCellBounds)
	{
		const FVector Offset = OutSeedPoint.GetLocation();
		OutSeedPoint.SetBoundsMin(InCell->Data.Bounds.Min - Offset);
		OutSeedPoint.SetBoundsMax(InCell->Data.Bounds.Max - Offset);
	}

	PCGExClusters::SetPointProperty(OutSeedPoint, InCell->Data.Area, AreaTo);
	PCGExClusters::SetPointProperty(OutSeedPoint, InCell->Data.Perimeter, PerimeterTo);
	PCGExClusters::SetPointProperty(OutSeedPoint, InCell->Data.Compactness, CompactnessTo);
}

bool FPCGExCellArtifactsDetails::WriteAny() const
{
	// Common attributes
	if (bWriteCellHash || bWriteArea || bWriteCompactness) { return true; }

	// Mode-specific attributes
	if (OutputMode == EPCGExCellOutputMode::Paths)
	{
		return bWriteVtxId || bFlagTerminalPoint || bWriteNumRepeat;
	}
	else if (OutputMode == EPCGExCellOutputMode::CellBounds)
	{
		return bWriteNumNodes;
	}

	return false;
}

bool FPCGExCellArtifactsDetails::Init(FPCGExContext* InContext)
{
	// Validate common attributes (used in all modes)
	if (bWriteCellHash) { PCGEX_VALIDATE_NAME_C(InContext, CellHashAttributeName); }
	if (bWriteArea) { PCGEX_VALIDATE_NAME_C(InContext, AreaAttributeName); }
	if (bWriteCompactness) { PCGEX_VALIDATE_NAME_C(InContext, CompactnessAttributeName); }

	// Validate mode-specific attributes
	if (OutputMode == EPCGExCellOutputMode::Paths)
	{
		// Paths-only attributes
		if (bWriteVtxId) { PCGEX_VALIDATE_NAME_C(InContext, VtxIdAttributeName); }
		if (bFlagTerminalPoint) { PCGEX_VALIDATE_NAME_C(InContext, TerminalFlagAttributeName); }
		if (bWriteNumRepeat) { PCGEX_VALIDATE_NAME_C(InContext, NumRepeatAttributeName); }

		// Tagging only relevant for Paths mode
		TagForwarding.bFilterToRemove = true;
		TagForwarding.bPreservePCGExData = false;
		TagForwarding.Init();
	}
	else if (OutputMode == EPCGExCellOutputMode::CellBounds)
	{
		// OBB-only attributes
		if (bWriteNumNodes) { PCGEX_VALIDATE_NAME_C(InContext, NumNodesAttributeName); }
	}

	return true;
}

void FPCGExCellArtifactsDetails::Process(const TSharedPtr<PCGExClusters::FCluster>& InCluster, const TSharedPtr<PCGExData::FFacade>& InDataFacade, const TSharedPtr<PCGExClusters::FCell>& InCell) const
{
	auto FwdTags = [&](const TSet<FString>& SourceTags)
	{
		TArray<FString> Tags = SourceTags.Array();
		for (int i = 0; i < Tags.Num(); i++)
		{
			if (Tags[i].StartsWith(PCGExCommon::PCGExPrefix))
			{
				Tags.RemoveAt(i);
				i--;
			}
		}

		TagForwarding.Prune(Tags);
		InDataFacade->Source->Tags->Append(Tags);
	};

	// Lots of wasted cycles here
	FwdTags(InCluster->VtxIO.Pin()->Tags->Flatten());
	FwdTags(InCluster->EdgesIO.Pin()->Tags->Flatten());

	PCGExPaths::Helpers::SetClosedLoop(InDataFacade->GetOut(), true);

	if (InCell->Data.bIsConvex) { if (bTagConvex) { InDataFacade->Source->Tags->AddRaw(ConvexTag); } }
	else { if (bTagConcave) { InDataFacade->Source->Tags->AddRaw(ConcaveTag); } }

	if (!WriteAny()) { return; }

	const int32 NumNodes = InCell->Nodes.Num();
	const TSharedPtr<PCGExData::TBuffer<bool>> TerminalBuffer = bFlagTerminalPoint ? InDataFacade->GetWritable(TerminalFlagAttributeName, false, true, PCGExData::EBufferInit::New) : nullptr;

	TMap<int32, int32> NumRepeats;
	const TSharedPtr<PCGExData::TBuffer<int32>> RepeatBuffer = bWriteNumRepeat ? InDataFacade->GetWritable(NumRepeatAttributeName, 0, true, PCGExData::EBufferInit::New) : nullptr;


	if (bWriteNumRepeat)
	{
		NumRepeats.Reserve(NumNodes);
		for (int i = 0; i < NumNodes; i++)
		{
			int32 NodeIdx = InCell->Nodes[i];
			NumRepeats.Add(NodeIdx, NumRepeats.FindOrAdd(NodeIdx, 0) + 1);
		}
	}

	if (bWriteCellHash) { InDataFacade->GetWritable<int64>(CellHashAttributeName, static_cast<int64>(InCell->GetCellHash()), true, PCGExData::EBufferInit::New); }
	if (bWriteArea) { InDataFacade->GetWritable<double>(AreaAttributeName, InCell->Data.Area, true, PCGExData::EBufferInit::New); }
	if (bWriteCompactness) { InDataFacade->GetWritable<double>(CompactnessAttributeName, InCell->Data.Compactness, true, PCGExData::EBufferInit::New); }
	if (TerminalBuffer) { for (int i = 0; i < NumNodes; i++) { TerminalBuffer->SetValue(i, InCluster->GetNode(InCell->Nodes[i])->IsLeaf()); } }
	if (RepeatBuffer) { for (int i = 0; i < NumNodes; i++) { RepeatBuffer->SetValue(i, NumRepeats[InCell->Nodes[i]] - 1); } }

	const TSharedPtr<PCGExData::TBuffer<int32>> VtxIDBuffer = bWriteVtxId ? InDataFacade->GetWritable(VtxIdAttributeName, 0, true, PCGExData::EBufferInit::New) : nullptr;
	if (VtxIDBuffer)
	{
		TSharedPtr<PCGExData::FPointIO> VtxIO = InCluster->VtxIO.Pin();
		const FPCGMetadataAttribute<int64>* VtxIDAttr = VtxIO ? VtxIO->FindConstAttribute<int64>(PCGExClusters::Labels::Attr_PCGExVtxIdx) : nullptr;

		if (VtxIO && VtxIDAttr)
		{
			TConstPCGValueRange<int64> MetadataEntries = VtxIO->GetIn()->GetConstMetadataEntryValueRange();
			for (int i = 0; i < NumNodes; i++)
			{
				const int32 PointIndex = InCluster->GetNodePointIndex(InCell->Nodes[i]);
				VtxIDBuffer->SetValue(i, PCGEx::H64A(VtxIDAttr->GetValueFromItemKey(MetadataEntries[PointIndex])));
			}
		}
	}
}


#pragma region OBB Output

namespace PCGExClusters
{
	void ProcessCellsAsOBBPoints(
		const TSharedPtr<FCluster>& InCluster,
		const TArray<TSharedPtr<FCell>>& InCells,
		const TSharedPtr<PCGExData::FFacade>& OutFacade,
		const FPCGExCellArtifactsDetails& ArtifactSettings,
		const TSharedPtr<PCGExMT::FTaskManager>& TaskManager)
	{
		if (!InCluster || InCells.IsEmpty() || !OutFacade)
		{
			if (OutFacade) { OutFacade->Source->Disable(); }
			return;
		}

		const int32 NumCells = InCells.Num();
		UPCGBasePointData* OutPointData = OutFacade->Source->GetOut();

		// Allocate output points
		PCGExPointArrayDataHelpers::SetNumPointsAllocated(OutPointData, NumCells);

		// Get cluster transforms (read-only, thread-safe)
		const TConstPCGValueRange<FTransform> ClusterTransforms = InCluster->VtxPoints->GetConstTransformValueRange();

		// Get value ranges for native properties
		TPCGValueRange<FTransform> OutTransforms = OutPointData->GetTransformValueRange();
		TPCGValueRange<FVector> OutBoundsMin = OutPointData->GetBoundsMinValueRange();
		TPCGValueRange<FVector> OutBoundsMax = OutPointData->GetBoundsMaxValueRange();

		// Create attribute writers (conditional based on ArtifactSettings)
		TSharedPtr<PCGExData::TBuffer<int64>> CellHashWriter = ArtifactSettings.bWriteCellHash ? OutFacade->GetWritable<int64>(ArtifactSettings.CellHashAttributeName, static_cast<int64>(0), true, PCGExData::EBufferInit::New) : nullptr;

		TSharedPtr<PCGExData::TBuffer<double>> AreaWriter = ArtifactSettings.bWriteArea ? OutFacade->GetWritable<double>(ArtifactSettings.AreaAttributeName, 0.0, true, PCGExData::EBufferInit::New) : nullptr;

		TSharedPtr<PCGExData::TBuffer<double>> CompactnessWriter = ArtifactSettings.bWriteCompactness ? OutFacade->GetWritable<double>(ArtifactSettings.CompactnessAttributeName, 0.0, true, PCGExData::EBufferInit::New) : nullptr;

		TSharedPtr<PCGExData::TBuffer<int32>> NumNodesWriter = ArtifactSettings.bWriteNumNodes ? OutFacade->GetWritable<int32>(ArtifactSettings.NumNodesAttributeName, 0, true, PCGExData::EBufferInit::New) : nullptr;

		PCGEX_PARALLEL_FOR(
			NumCells,

			const TSharedPtr<FCell>& Cell = InCells[i];
			if (!Cell) { return;; }

			// Build FBestFitPlane from cell vertices (thread-local, stack-allocated)
			const PCGExMath::FBestFitPlane BFP = PCGExMath::FBestFitPlane(
				Cell->Nodes.Num(),
				[&](int32 j) { return ClusterTransforms[InCluster->GetNodePointIndex(Cell->Nodes[j])].GetLocation(); },
				ArtifactSettings.OBBAttributes.bUseMinBoxFit
			);

			// Get extents reordered to match axis order
			FVector Extents = BFP.GetExtents(ArtifactSettings.OBBAttributes.AxisOrder);
			for (int c = 0; c < 3; c++) {Extents[c] = FMath::Max(Extents[c], ArtifactSettings.OBBAttributes.MinExtent[c]);}

			// Write OBB transform to unique index (thread-safe)
			OutTransforms[i] = BFP.GetTransform(ArtifactSettings.OBBAttributes.AxisOrder);
			OutBoundsMin[i] = -Extents;
			OutBoundsMax[i] = Extents;

			// Write all attributes
			if (CellHashWriter) { CellHashWriter->SetValue(i, static_cast<int64>(Cell->GetCellHash())); }
			if (AreaWriter) { AreaWriter->SetValue(i, Cell->Data.Area); }
			if (CompactnessWriter) { CompactnessWriter->SetValue(i, Cell->Data.Compactness); }
			if (NumNodesWriter) { NumNodesWriter->SetValue(i, Cell->Nodes.Num()); }

		);

		// Commit facade
		OutFacade->WriteFastest(TaskManager);
	}
}

#pragma endregion
