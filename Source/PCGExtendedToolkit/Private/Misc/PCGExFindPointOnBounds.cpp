﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExFindPointOnBounds.h"

#include "Data/PCGExData.h"
#include "Details/PCGExDetailsSettings.h"
#include "Geometry/PCGExGeo.h"


#define LOCTEXT_NAMESPACE "PCGExFindPointOnBoundsElement"
#define PCGEX_NAMESPACE FindPointOnBounds

PCGEX_SETTING_DATA_VALUE_IMPL(UPCGExFindPointOnBoundsSettings, UVW, FVector, UVWInput, LocalUVW, UVW)

PCGEX_INITIALIZE_ELEMENT(FindPointOnBounds)
PCGEX_ELEMENT_BATCH_POINT_IMPL(FindPointOnBounds)

bool FPCGExFindPointOnBoundsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FindPointOnBounds)

	PCGEX_FWD(CarryOverDetails)
	Context->CarryOverDetails.Init();

	if (Settings->OutputMode == EPCGExPointOnBoundsOutputMode::Merged)
	{
		TSet<FName> AttributeMismatches;

		Context->BestIndices.Init(-1, Context->MainPoints->Num());

		Context->MergedOut = PCGExData::NewPointIO(Context, Settings->GetMainOutputPin(), 0);
		Context->MergedAttributesInfos = PCGEx::FAttributesInfos::Get(Context->MainPoints, AttributeMismatches);

		Context->CarryOverDetails.Attributes.Prune(*Context->MergedAttributesInfos);
		Context->CarryOverDetails.Attributes.Prune(AttributeMismatches);

		Context->MergedOut->InitializeOutput(PCGExData::EIOInit::New);
		PCGEx::SetNumPointsAllocated(Context->MergedOut->GetOut(), Context->MainPoints->Num());
		Context->MergedOut->GetOutKeys(true);

		if (!AttributeMismatches.IsEmpty() && !Settings->bQuietAttributeMismatchWarning)
		{
			PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("Some attributes on incoming data share the same name but not the same type. Whatever type was discovered first will be used."));
		}
	}

	return true;
}

bool FPCGExFindPointOnBoundsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindPointOnBoundsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FindPointOnBounds)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
				//NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	if (Settings->OutputMode == EPCGExPointOnBoundsOutputMode::Merged)
	{
		PCGExFindPointOnBounds::MergeBestCandidatesAttributes(
			Context->MergedOut,
			Context->MainPoints->Pairs,
			Context->BestIndices,
			*Context->MergedAttributesInfos);

		(void)Context->MergedOut->StageOutput(Context);
	}
	else
	{
		Context->MainPoints->StageOutputs();
	}

	return Context->TryComplete();
}

void PCGExFindPointOnBounds::MergeBestCandidatesAttributes(
	const TSharedPtr<PCGExData::FPointIO>& Target,
	const TArray<TSharedPtr<PCGExData::FPointIO>>& Collections,
	const TArray<int32>& BestIndices,
	const PCGEx::FAttributesInfos& InAttributesInfos)
{
	UPCGMetadata* OutMetadata = Target->GetOut()->Metadata;

	for (int i = 0; i < BestIndices.Num(); i++)
	{
		const TSharedPtr<PCGExData::FPointIO> IO = Collections[i];

		if (BestIndices[i] == -1 || !IO) { continue; }

		PCGMetadataEntryKey InKey = IO->GetIn()->GetMetadataEntry(BestIndices[i]);
		PCGMetadataEntryKey OutKey = Target->GetOut()->GetMetadataEntry(i);
		UPCGMetadata* InMetadata = IO->GetIn()->Metadata;

		for (const PCGEx::FAttributeIdentity& Identity : InAttributesInfos.Identities)
		{
			PCGEx::ExecuteWithRightType(
				Identity.GetTypeId(), [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					const FPCGMetadataAttribute<T>* InAttribute = InMetadata->GetConstTypedAttribute<T>(Identity.Identifier);
					FPCGMetadataAttribute<T>* OutAttribute = PCGEx::TryGetMutableAttribute<T>(OutMetadata, Identity.Identifier);

					if (!OutAttribute)
					{
						OutAttribute = Target->FindOrCreateAttribute<T>(
							Identity.Identifier,
							InAttribute->GetValueFromItemKey(PCGDefaultValueKey),
							InAttribute->AllowsInterpolation());
					}

					if (!OutAttribute) { return; }

					OutAttribute->SetValue(OutKey, InAttribute->GetValueFromItemKey(InKey));
				});
		}
	}
}

namespace PCGExFindPointOnBounds
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFindPointOnBounds::Process);

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		FBox Bounds = FBox(ForceInit);
		FVector UVW = Settings->GetValueSettingUVW(Context, PointDataFacade->GetIn())->Read(0);

		if (Settings->bBestFitBounds)
		{
			PCGExGeo::FBestFitPlane BestFitPlane(PointDataFacade->GetIn()->GetConstTransformValueRange());

			FTransform T = BestFitPlane.GetTransform(Settings->AxisOrder);
			UVW = T.TransformVector(UVW);
			Bounds = FBox(BestFitPlane.Centroid - BestFitPlane.Extents, BestFitPlane.Centroid + BestFitPlane.Extents).TransformBy(T);
		}
		else
		{
			Bounds = PointDataFacade->Source->GetIn()->GetBounds();
		}

		SearchPosition = Bounds.GetCenter() + Bounds.GetExtent() * UVW;

		StartParallelLoopForPoints(PCGExData::EIOSide::In);

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::FindPointOnBounds::ProcessPoints);

		TConstPCGValueRange<FTransform> InTransforms = PointDataFacade->GetIn()->GetConstTransformValueRange();

		PCGEX_SCOPE_LOOP(Index)
		{
			const double Dist = FVector::Dist(InTransforms[Index].GetLocation(), SearchPosition);

			{
				FWriteScopeLock WriteLock(BestIndexLock);
				if (Dist > BestDistance) { continue; }
			}

			{
				FWriteScopeLock WriteLock(BestIndexLock);

				if (Dist > BestDistance) { continue; }

				BestPosition = InTransforms[Index].GetLocation();
				BestIndex = Index;
				BestDistance = Dist;
			}
		}
	}

	void FProcessor::CompleteWork()
	{
		const FVector Offset = (BestPosition - PointDataFacade->Source->GetIn()->GetBounds().GetCenter()).GetSafeNormal() * Settings->Offset;

		if (Settings->OutputMode == EPCGExPointOnBoundsOutputMode::Merged)
		{
			const int32 TargetIndex = PointDataFacade->Source->IOIndex;

			TPCGValueRange<FTransform> OutTransforms = Context->MergedOut->GetOut()->GetTransformValueRange(false);
			TPCGValueRange<int64> OutMetadataEntry = Context->MergedOut->GetOut()->GetMetadataEntryValueRange(false);
			const PCGMetadataEntryKey OriginalKey = OutMetadataEntry[TargetIndex];

			PointDataFacade->Source->GetIn()->CopyPointsTo(Context->MergedOut->GetOut(), BestIndex, TargetIndex, 1);

			OutTransforms[TargetIndex].AddToTranslation(Offset);
			OutMetadataEntry[TargetIndex] = OriginalKey;
		}
		else
		{
			PCGEX_INIT_IO_VOID(PointDataFacade->Source, PCGExData::EIOInit::New)
			PCGEx::SetNumPointsAllocated(PointDataFacade->GetOut(), 1);

			TPCGValueRange<FTransform> OutTransforms = PointDataFacade->GetOut()->GetTransformValueRange(false);
			TPCGValueRange<int64> OutMetadataEntry = PointDataFacade->GetOut()->GetMetadataEntryValueRange(false);

			PointDataFacade->Source->GetOut()->Metadata->InitializeOnSet(OutMetadataEntry[0]);
			OutTransforms[0].AddToTranslation(Offset);
		}
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
