// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "..\..\Public\Transforms\PCGExSampleNearestPoint.h"
#include "Data/PCGSpatialData.h"
#include "PCGContext.h"
#include "PCGExCommon.h"
#include "PCGPin.h"
#include <algorithm>

#define LOCTEXT_NAMESPACE "PCGExSampleNearestPointElement"

UPCGExSampleNearestPointSettings::UPCGExSampleNearestPointSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (NormalSource.Selector.GetName() == FName("@Last"))
	{
		NormalSource.Selector.Update(TEXT("$Transform"));
	}

	if (!WeightOverDistance)
	{
		WeightOverDistance = PCGEx::WeightDistributionLinear;
	}
}

TArray<FPCGPinProperties> UPCGExSampleNearestPointSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	FPCGPinProperties& PinPropertySourceTargets = PinProperties.Emplace_GetRef(PCGEx::SourceTargetPointsLabel, EPCGDataType::Point, false, false);

#if WITH_EDITOR
	PinPropertySourceTargets.Tooltip = LOCTEXT("PCGExSourceTargetsPointsPinTooltip", "The point data set to check against.");
#endif // WITH_EDITOR

	return PinProperties;
}

PCGEx::EIOInit UPCGExSampleNearestPointSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::DuplicateInput; }

int32 UPCGExSampleNearestPointSettings::GetPreferredChunkSize() const { return 32; }

FPCGElementPtr UPCGExSampleNearestPointSettings::CreateElement() const { return MakeShared<FPCGExSampleNearestPointElement>(); }

FPCGContext* FPCGExSampleNearestPointElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExSampleNearestPointContext* Context = new FPCGExSampleNearestPointContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	const UPCGExSampleNearestPointSettings* Settings = Context->GetInputSettings<UPCGExSampleNearestPointSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Targets = InputData.GetInputsByPin(PCGEx::SourceTargetPointsLabel);
	if (!Targets.IsEmpty())
	{
		const FPCGTaggedData& Target = Targets[0];
		if (const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Target.Data))
		{
			if (const UPCGPointData* PointData = SpatialData->ToPointData(Context))
			{
				Context->Targets = const_cast<UPCGPointData*>(PointData);
				Context->NumTargets = Context->Targets->GetPoints().Num();
			}
		}
	}

	Context->WeightCurve = Settings->WeightOverDistance.LoadSynchronous();

	Context->RangeMin = Settings->MaxDistance;
	Context->bUseOctree = Settings->MaxDistance <= 0;

	PCGEX_FORWARD_OUT_ATTRIBUTE(Location)
	PCGEX_FORWARD_OUT_ATTRIBUTE(Direction)
	PCGEX_FORWARD_OUT_ATTRIBUTE(Normal)
	PCGEX_FORWARD_OUT_ATTRIBUTE(Distance)

	return Context;
}

bool FPCGExSampleNearestPointElement::Validate(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Validate(InContext)) { return false; }

	FPCGExSampleNearestPointContext* Context = static_cast<FPCGExSampleNearestPointContext*>(InContext);
	const UPCGExSampleNearestPointSettings* Settings = Context->GetInputSettings<UPCGExSampleNearestPointSettings>();
	check(Settings);

	if (!Context->Targets || Context->NumTargets < 1)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingTargets", "No targets (either no input or empty dataset)"));
		return false;
	}

	if (!Context->WeightCurve)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidWeightCurve", "Weight Curve asset could not be loaded."));
		return false;
	}

	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Location)
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Direction)
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Normal)
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Distance)

	Context->RangeMin = Settings->RangeMin;
	Context->bLocalRangeMin = Settings->bUseLocalRangeMin;
	Context->RangeMinInput.Capture(Settings->LocalRangeMin);

	Context->RangeMax = Settings->RangeMax;
	Context->bLocalRangeMax = Settings->bUseLocalRangeMax;
	Context->RangeMaxInput.Capture(Settings->LocalRangeMax);

	Context->SampleMethod = Settings->SampleMethod;
	Context->WeightMethod = Settings->WeightMethod;

	if (Context->bWriteNormal)
	{
		Context->NormalInput.Capture(Settings->NormalSource);
		if (!Context->NormalInput.Validate(Context->Targets))
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("InvalidNormalSource", "Normal source is invalid."));
		}
	}

	return true;
}

bool FPCGExSampleNearestPointElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNearestPointElement::Execute);

	FPCGExSampleNearestPointContext* Context = static_cast<FPCGExSampleNearestPointContext*>(InContext);

	if (Context->IsState(PCGExMT::EState::Setup))
	{
		if (!Validate(Context)) { return true; }

		Context->Octree = Context->bUseOctree ? const_cast<UPCGPointData::PointOctree*>(&(Context->Targets->GetOctree())) : nullptr;
		const TArray<FPCGPoint>& TargetPoints = Context->Targets->GetPoints();
		const int32 NumTargets = TargetPoints.Num();
		Context->TargetIndices.Reserve(NumTargets);
		for (int i = 0; i < NumTargets; i++) { Context->TargetIndices.Add(TargetPoints[i].MetadataEntry, i); }
		Context->SetState(PCGExMT::EState::ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::EState::ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO())
		{
			Context->SetState(PCGExMT::EState::Done);
		}
		else
		{
			Context->SetState(PCGExMT::EState::ProcessingPoints);
		}
	}

	auto InitializeForIO = [&Context, this](UPCGExPointIO* IO)
	{
		IO->BuildMetadataEntries();

		if (Context->bLocalRangeMin)
		{
			if (Context->RangeMinInput.Validate(IO->Out))
			{
				PCGE_LOG(Warning, GraphAndLog, LOCTEXT("InvalidLocalRangeMin", "RangeMin metadata missing"));
			}
		}

		if (Context->bLocalRangeMax)
		{
			if (Context->RangeMaxInput.Validate(IO->Out))
			{
				PCGE_LOG(Warning, GraphAndLog, LOCTEXT("InvalidLocalRangeMax", "RangeMax metadata missing"));
			}
		}

		PCGEX_INIT_ATTRIBUTE_OUT(Location, FVector)
		PCGEX_INIT_ATTRIBUTE_OUT(Direction, FVector)
		PCGEX_INIT_ATTRIBUTE_OUT(Normal, FVector)
		PCGEX_INIT_ATTRIBUTE_OUT(Distance, double)
	};

	auto ProcessPoint = [&Context](const FPCGPoint& Point, const int32 ReadIndex, const UPCGExPointIO* IO)
	{
		double RangeMin = FMath::Pow(Context->RangeMinInput.GetValueSafe(Point, Context->RangeMin), 2);
		double RangeMax = FMath::Pow(Context->RangeMaxInput.GetValueSafe(Point, Context->RangeMax), 2);

		if (RangeMin > RangeMax) { std::swap(RangeMin, RangeMax); }

		TArray<PCGExNearestPoint::FTargetInfos> TargetsInfos;
		TargetsInfos.Reserve(Context->NumTargets);

		PCGExNearestPoint::FTargetsCompoundInfos TargetsCompoundInfos;

		FVector Origin = Point.Transform.GetLocation();
		auto ProcessTarget = [&Context, &Origin, &ReadIndex, &RangeMin, &RangeMax, &TargetsInfos, &TargetsCompoundInfos](const FPCGPoint& TargetPoint)
		{
			const double dist = FVector::DistSquared(Origin, TargetPoint.Transform.GetLocation());

			if (Context->SampleMethod != EPCGExSampleMethod::AllTargets)
			{
				if (dist < RangeMin) { return; }
				if (dist > RangeMax) { return; }
			}

			if (Context->SampleMethod == EPCGExSampleMethod::ClosestTarget ||
				Context->SampleMethod == EPCGExSampleMethod::FarthestTarget)
			{
				FReadScopeLock ScopeLock(Context->IndicesLock);
				TargetsCompoundInfos.UpdateCompound(PCGExNearestPoint::FTargetInfos(*Context->TargetIndices.Find(TargetPoint.MetadataEntry), dist));
			}
			else
			{
				FReadScopeLock ScopeLock(Context->IndicesLock);
				const PCGExNearestPoint::FTargetInfos& Infos = TargetsInfos.Emplace_GetRef(*Context->TargetIndices.Find(TargetPoint.MetadataEntry), dist);
				TargetsCompoundInfos.UpdateCompound(Infos);
			}
		};

		// First: Sample all possible targets
		if (Context->Octree)
		{
			const FBoxCenterAndExtent Box = FBoxCenterAndExtent(Point.Transform.GetLocation(), FVector(Context->RangeMin));
			Context->Octree->FindElementsWithBoundsTest(
				Box,
				[&ProcessTarget](const FPCGPointRef& OtherPointRef)
				{
					const FPCGPoint* OtherPoint = OtherPointRef.Point;
					ProcessTarget(*OtherPoint);
				});
		}
		else
		{
			const TArray<FPCGPoint>& Targets = Context->Targets->GetPoints();
			for (const FPCGPoint& OtherPoint : Targets) { ProcessTarget(OtherPoint); }
		}

		// Compute individual target weight
		if (Context->WeightMethod == EPCGExWeightMethod::FullRange)
		{
			// Reset compounded infos to full range
			TargetsCompoundInfos.RangeMin = RangeMin;
			TargetsCompoundInfos.RangeMax = RangeMax;
		}

		FVector WeightedLocation = FVector::Zero();
		FVector WeightedDirection = FVector::Zero();
		FVector WeightedNormal = FVector::Zero();
		double WeightedDistance = 0;
		double TotalWeight = 0;


		auto ProcessTargetInfos = [&Context, &Origin, &WeightedLocation, &WeightedDirection, &WeightedNormal, &TotalWeight]
			(PCGExNearestPoint::FTargetInfos& TargetInfos, double Weight)
		{
			const FPCGPoint TargetPoint = Context->Targets->GetPoint(TargetInfos.Index);
			const FVector TargetLocationOffset = TargetPoint.Transform.GetLocation() - Origin;
			WeightedLocation += (TargetLocationOffset * Weight); // Relative to origin
			WeightedDirection += (TargetLocationOffset.GetSafeNormal()) * Weight;
			WeightedNormal += Context->NormalInput.GetValue(TargetPoint) * Weight;

			TotalWeight += Weight;
		};

		if (Context->SampleMethod == EPCGExSampleMethod::ClosestTarget ||
			Context->SampleMethod == EPCGExSampleMethod::FarthestTarget)
		{
			PCGExNearestPoint::FTargetInfos& TargetInfos = Context->SampleMethod == EPCGExSampleMethod::ClosestTarget ? TargetsCompoundInfos.Closest : TargetsCompoundInfos.Farthest;
			const double Weight = Context->WeightCurve->GetFloatValue(TargetsCompoundInfos.GetRangeRatio(TargetInfos.Distance));
			ProcessTargetInfos(TargetInfos, Weight);
		}
		else
		{
			for (PCGExNearestPoint::FTargetInfos& TargetInfos : TargetsInfos)
			{
				const double Weight = Context->WeightCurve->GetFloatValue(TargetsCompoundInfos.GetRangeRatio(TargetInfos.Distance));
				if (Weight == 0) { continue; }
				ProcessTargetInfos(TargetInfos, Weight);
			}
		}

		if (TotalWeight != 0) // Dodge NaN
		{
			WeightedLocation /= TotalWeight;
			WeightedDirection /= TotalWeight;
		}

		WeightedDirection.Normalize();
		WeightedNormal.Normalize();

		const PCGMetadataEntryKey Key = Point.MetadataEntry;
		PCGEX_SET_OUT_ATTRIBUTE(Location, Key, Origin + WeightedLocation)
		PCGEX_SET_OUT_ATTRIBUTE(Direction, Key, WeightedDirection)
		PCGEX_SET_OUT_ATTRIBUTE(Normal, Key, WeightedNormal)

	};

	if (Context->IsState(PCGExMT::EState::ProcessingPoints))
	{
		if (Context->CurrentIO->OutputParallelProcessing(Context, InitializeForIO, ProcessPoint, Context->ChunkSize))
		{
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
		}
	}

	if (Context->IsDone())
	{
		Context->TargetIndices.Empty();
		Context->OutputPoints();
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
