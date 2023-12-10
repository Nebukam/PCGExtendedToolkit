// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExFusePoints.h"

#define LOCTEXT_NAMESPACE "PCGExFusePointsElement"

PCGExData::EInit UPCGExFusePointsSettings::GetPointOutputInitMode() const { return PCGExData::EInit::NewOutput; }

FPCGExFusePointsContext::~FPCGExFusePointsContext()
{
	AttributesBlendingOverrides.Empty();
	FusedPoints.Empty();
	OutPoints = nullptr;
	PCGEX_DELETE(MetadataBlender)
	PCGEX_DELETE(PropertyBlender)
}

UPCGExFusePointsSettings::UPCGExFusePointsSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR
void UPCGExFusePointsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

FPCGElementPtr UPCGExFusePointsSettings::CreateElement() const { return MakeShared<FPCGExFusePointsElement>(); }

FPCGContext* FPCGExFusePointsElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExFusePointsContext* Context = new FPCGExFusePointsContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	const UPCGExFusePointsSettings* Settings = Context->GetInputSettings<UPCGExFusePointsSettings>();
	check(Settings);

	Context->AttributesBlendingOverrides = Settings->BlendingSettings.AttributesOverrides;
	Context->MetadataBlender = new PCGExDataBlending::FMetadataBlender(Settings->BlendingSettings.DefaultBlending);
	Context->PropertyBlender = new PCGExDataBlending::FPropertiesBlender(Settings->BlendingSettings);

	return Context;
}

bool FPCGExFusePointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFusePointsElement::Execute);

	FPCGExFusePointsContext* Context = static_cast<FPCGExFusePointsContext*>(InContext);
	const UPCGExFusePointsSettings* Settings = Context->GetInputSettings<UPCGExFusePointsSettings>();
	check(Settings);

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else { Context->SetState(PCGExFuse::State_FindingFusePoints); }
	}

	if (Context->IsState(PCGExFuse::State_FindingFusePoints))
	{
		auto Initialize = [&](PCGExData::FPointIO& PointIO)
		{
			Context->FusedPoints.Reset(PointIO.GetNum() / 2);
		};

		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			const FPCGPoint& Point = PointIO.GetInPoint(PointIndex);
			const FVector PtPosition = Point.Transform.GetLocation();
			double Distance = 0;
			PCGExFuse::FFusedPoint* FuseTarget = nullptr;

			Context->PointsLock.ReadLock();
			
			if (Settings->bComponentWiseRadius)
			{
				for (PCGExFuse::FFusedPoint& FusedPoint : Context->FusedPoints)
				{
					if (abs(PtPosition.X - FusedPoint.Position.X) <= Settings->Radiuses.X &&
						abs(PtPosition.Y - FusedPoint.Position.Y) <= Settings->Radiuses.Y &&
						abs(PtPosition.Z - FusedPoint.Position.Z) <= Settings->Radiuses.Z)
					{
						Distance = FVector::DistSquared(FusedPoint.Position, PtPosition);
						FuseTarget = &FusedPoint;
						break;
					}
				}
			}
			else
			{
				for (PCGExFuse::FFusedPoint& FusedPoint : Context->FusedPoints)
				{
					Distance = FVector::DistSquared(FusedPoint.Position, PtPosition);
					if (Distance < (Settings->Radius * Settings->Radius))
					{
						FuseTarget = &FusedPoint;
						break;
					}
				}
			}
			
			Context->PointsLock.ReadUnlock();

			if (!FuseTarget)
			{
				FWriteScopeLock WriteLock(Context->PointsLock);
				&Context->FusedPoints.Emplace_GetRef(PointIndex, PtPosition);
			}
			else { FuseTarget->Add(PointIndex, Distance); }
		};

		if (Context->ProcessCurrentPoints(Initialize, ProcessPoint))
		{
			Context->SetState(PCGExFuse::State_MergingPoints);
		}
	}

	if (Context->IsState(PCGExFuse::State_MergingPoints))
	{
		auto Initialize = [&]()
		{
			const TArray<FPCGPoint>& InPoints = Context->GetCurrentIn()->GetPoints();
			TArray<FPCGPoint>& MutablePoints = Context->GetCurrentOut()->GetMutablePoints();
			MutablePoints.SetNumUninitialized(Context->FusedPoints.Num());

			int32 Index = 0;
			for (const PCGExFuse::FFusedPoint& FPoint : Context->FusedPoints)
			{
				MutablePoints[Index] = InPoints[FPoint.Index];
				Index++;
			}

			Context->MetadataBlender->PrepareForData(Context->CurrentIO, Context->AttributesBlendingOverrides);
		};

		auto FusePoint = [&](int32 ReadIndex)
		{
			PCGExFuse::FFusedPoint& FusedPoint = Context->FusedPoints[ReadIndex];
			const int32 NumFused = FusedPoint.Fused.Num();

			if (NumFused == 0) { return; }

			const double AverageDivider = NumFused;

			FPCGPoint& ConsolidatedPoint = Context->CurrentIO->GetMutablePoint(ReadIndex);

			Context->MetadataBlender->PrepareForBlending(ReadIndex);

			PCGExDataBlending::FPropertiesBlender PropertiesBlender = PCGExDataBlending::FPropertiesBlender(*Context->PropertyBlender);
			// Skip prepare, the original point is already data itself.
			//if (PropertiesBlender.bRequiresPrepare) { PropertiesBlender.PrepareBlending(ConsolidatedPoint, ConsolidatedPoint); }

			for (int i = 0; i < NumFused; i++)
			{
				const int32 FusedIndex = FusedPoint.Fused[i];
				const double Weight = 1 - (FusedPoint.Distances[i] / FusedPoint.MaxDistance);
				PropertiesBlender.Blend(ConsolidatedPoint, Context->CurrentIO->GetInPoint(FusedIndex), ConsolidatedPoint, Weight);
				Context->MetadataBlender->Blend(ReadIndex, FusedIndex, ReadIndex, Weight);
			}

			if (PropertiesBlender.bRequiresPrepare) { PropertiesBlender.CompleteBlending(ConsolidatedPoint); }
			Context->MetadataBlender->CompleteBlending(ReadIndex, AverageDivider);
		};

		if (PCGExMT::ParallelForLoop(Context, Context->FusedPoints.Num(), Initialize, FusePoint, Context->ChunkSize))
		{
			Context->CurrentIO->OutputTo(Context);
			Context->CurrentIO->Cleanup();
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
