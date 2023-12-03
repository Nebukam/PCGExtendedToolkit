// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExFusePoints.h"

#define LOCTEXT_NAMESPACE "PCGExFusePointsElement"

#define PCGEX_FUSE_FOREACH_POINTPROPERTYNAME(MACRO)\
MACRO(Density) \
MACRO(Extents) \
MACRO(Color) \
MACRO(Position) \
MACRO(Rotation)\
MACRO(Scale) \
MACRO(Steepness) \
MACRO(Seed)

// TYPE, NAME, ACCESSOR
#define PCGEX_FUSE_FOREACH_POINTPROPERTY(MACRO)\
MACRO(float, Density, Density, 0) \
MACRO(FVector, Extents, GetExtents(), FVector::Zero()) \
MACRO(FVector4, Color, Color, FVector4::Zero()) \
MACRO(FVector, Position, Transform.GetLocation(), FVector::Zero()) \
MACRO(FRotator, Rotation, Transform.Rotator(), FRotator::ZeroRotator)\
MACRO(FVector, Scale, Transform.GetScale3D(), FVector::Zero()) \
MACRO(float, Steepness, Steepness, 0) \
MACRO(int32, Seed, Seed, 0)

#define PCGEX_FUSE_FOREACH_POINTPROPERTY_ASSIGN(MACRO)\
MACRO(Density, Density) \
MACRO(Color, Color) \
MACRO(Transform, Transform) \
MACRO(Steepness, Steepness) \
MACRO(Seed, Seed)

#define PCGEX_FUSE_IGNORE(...) // Ignore

PCGExPointIO::EInit UPCGExFusePointsSettings::GetPointOutputInitMode() const { return PCGExPointIO::EInit::NewOutput; }

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

	Context->Radius = FMath::Pow(Settings->Radius, 2);
	Context->bComponentWiseRadius = Settings->bComponentWiseRadius;
	Context->Radiuses = Settings->Radiuses;

	Context->BlendingOverrides = Settings->AttributeBlendingOverrides;
	Context->Blender = NewObject<UPCGExMetadataBlender>();
	Context->Blender->DefaultOperation = Settings->Blending;

#define PCGEX_FUSE_TRANSFERT(_NAME) Context->_NAME##Blending = Settings->PropertyBlendingOverrides.bOverride##_NAME ? Settings->PropertyBlendingOverrides._NAME##Blending : Settings->Blending;
	PCGEX_FUSE_FOREACH_POINTPROPERTYNAME(PCGEX_FUSE_TRANSFERT)
#undef PCGEX_FUSE_TRANSFERT

	return Context;
}

bool FPCGExFusePointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFusePointsElement::Execute);

	FPCGExFusePointsContext* Context = static_cast<FPCGExFusePointsContext*>(InContext);

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			Context->CurrentIndex = 0;
			Context->SetState(PCGExFuse::State_FindingRootPoints);
		}
	}

	if (Context->IsState(PCGExFuse::State_FindingRootPoints))
	{
		auto Initialize = [&](UPCGExPointIO* PointIO)
		{
			Context->FusedPoints.Reset(PointIO->NumInPoints);
			Context->Blender->PrepareForData(PointIO->Out, nullptr, Context->BlendingOverrides);
		};

		auto ProcessPoint = [&](const int32 PointIndex, const UPCGExPointIO* PointIO)
		{
			const FPCGPoint& Point = PointIO->GetInPoint(PointIndex);
			const FVector PtPosition = Point.Transform.GetLocation();
			double Distance = 0;
			PCGExFuse::FFusedPoint* FuseTarget = nullptr;

			Context->PointsLock.ReadLock();
			if (Context->bComponentWiseRadius)
			{
				for (PCGExFuse::FFusedPoint& FusedPoint : Context->FusedPoints)
				{
					if (abs(PtPosition.X - FusedPoint.Position.X) <= Context->Radiuses.X &&
						abs(PtPosition.Y - FusedPoint.Position.Y) <= Context->Radiuses.Y &&
						abs(PtPosition.Z - FusedPoint.Position.Z) <= Context->Radiuses.Z)
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
					if (Distance < Context->Radius)
					{
						FuseTarget = &FusedPoint;
						break;
					}
				}
			}
			Context->PointsLock.ReadUnlock();

			FWriteScopeLock WriteLock(Context->PointsLock);
			if (!FuseTarget)
			{
				FuseTarget = &Context->FusedPoints.Emplace_GetRef();
				FuseTarget->MainIndex = PointIndex;
				FuseTarget->Position = PtPosition;
			}

			FuseTarget->Add(PointIndex, Distance);
		};

		if (Context->ProcessCurrentPoints(Initialize, ProcessPoint))
		{
			Context->SetState(PCGExFuse::State_MergingPoints);
		}
	}

	if (Context->IsState(PCGExFuse::State_MergingPoints))
	{
		auto FusePoint = [&](int32 ReadIndex)
		{
			PCGExFuse::FFusedPoint& FusedPointData = Context->FusedPoints[ReadIndex];
			int32 NumFused = static_cast<double>(FusedPointData.Fused.Num());
			double AverageDivider = NumFused;

			const FPCGPoint& RootPoint = Context->CurrentIO->GetInPoint(FusedPointData.MainIndex);
			FPCGPoint NewPoint = Context->CurrentIO->CopyPoint(RootPoint);
			PCGMetadataEntryKey NewPointKey = NewPoint.MetadataEntry;
			Context->Blender->PrepareForOperations(NewPointKey);

			FTransform& OutTransform = NewPoint.Transform;
#define PCGEX_FUSE_DECLARE(_TYPE, _NAME, _ACCESSOR, _DEFAULT_VALUE, ...) _TYPE Out##_NAME = Context->_NAME##Blending != EPCGExDataBlendingType::Average ? RootPoint._ACCESSOR : _DEFAULT_VALUE;
			PCGEX_FUSE_FOREACH_POINTPROPERTY(PCGEX_FUSE_DECLARE)
#undef PCGEX_FUSE_DECLARE

			for (int i = 0; i < NumFused; i++)
			{
				const double Weight = 1 - (FusedPointData.Distances[i] / FusedPointData.MaxDistance);
				FPCGPoint Point = Context->CurrentIO->GetInPoint(FusedPointData.Fused[i]);

#define PCGEX_FUSE_FUSE(_TYPE, _NAME, _ACCESSOR, ...) switch (Context->_NAME##Blending){\
case EPCGExDataBlendingType::Average: Out##_NAME += Point._ACCESSOR; break;\
case EPCGExDataBlendingType::Min: Out##_NAME = PCGExMath::CWMin(Out##_NAME, Point._ACCESSOR); break;\
case EPCGExDataBlendingType::Max: Out##_NAME = PCGExMath::CWMax(Out##_NAME, Point._ACCESSOR); break;\
case EPCGExDataBlendingType::Weight: Out##_NAME = PCGExMath::Lerp(Out##_NAME, Point._ACCESSOR, Weight); break;\
}
				PCGEX_FUSE_FOREACH_POINTPROPERTY(PCGEX_FUSE_FUSE)
#undef PCGEX_FUSE_FUSE

				Context->Blender->DoOperations(NewPointKey, Point.MetadataEntry, NewPointKey, Weight);
			}

			Context->Blender->FinalizeOperations(NewPointKey, AverageDivider);

#define PCGEX_FUSE_POST(_TYPE, _NAME, _ACCESSOR, ...)\
if(Context->_NAME##Blending == EPCGExDataBlendingType::Average){ Out##_NAME = PCGExMath::CWDivide(Out##_NAME, AverageDivider); }
			PCGEX_FUSE_FOREACH_POINTPROPERTY(PCGEX_FUSE_POST)
#undef PCGEX_FUSE_POST

			OutTransform.SetLocation(OutPosition);
			OutTransform.SetRotation(OutRotation.Quaternion());
			OutTransform.SetScale3D(OutScale);

#define PCGEX_FUSE_ASSIGN(_NAME, _ACCESSOR, ...) NewPoint._ACCESSOR = Out##_NAME;
			PCGEX_FUSE_FOREACH_POINTPROPERTY_ASSIGN(PCGEX_FUSE_ASSIGN)
#undef PCGEX_FUSE_ASSIGN

			NewPoint.SetExtents(OutExtents);
		};

		if (PCGExMT::ParallelForLoop(
			Context, Context->FusedPoints.Num(), []()
			{
			}, FusePoint, Context->ChunkSize))
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
	}

	if (Context->IsDone())
	{
		Context->FusedPoints.Empty();
		Context->OutputPoints();
		return true;
	}

	return false;
}

#undef PCGEX_FUSE_FOREACH_POINTPROPERTY
#undef PCGEX_FUSE_FOREACH_POINTPROPERTY_ASSIGN
#undef PCGEX_FUSE_IGNORE

#undef LOCTEXT_NAMESPACE
