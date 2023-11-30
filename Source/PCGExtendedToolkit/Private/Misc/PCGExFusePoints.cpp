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

PCGExIO::EInitMode UPCGExFusePointsSettings::GetPointOutputInitMode() const { return PCGExIO::EInitMode::NewOutput; }

UPCGExFusePointsSettings::UPCGExFusePointsSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RefreshFuseMethodHiddenNames();
}

#if WITH_EDITOR
void UPCGExFusePointsSettings::RefreshFuseMethodHiddenNames()
{
	if (!FuseMethodOverrides.IsEmpty())
	{
		for (FPCGExInputDescriptorWithFuseMethod& Descriptor : FuseMethodOverrides) { Descriptor.PrintDisplayName(); }

#define PCGEX_FUSE_UPDATE(_NAME) if(!bOverride##_NAME){_NAME##FuseMethod = FuseMethod;}
		PCGEX_FUSE_FOREACH_POINTPROPERTYNAME(PCGEX_FUSE_UPDATE)
#undef PCGEX_FUSE_UPDATE
	}
}

void UPCGExFusePointsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	RefreshFuseMethodHiddenNames();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

FPCGElementPtr UPCGExFusePointsSettings::CreateElement() const { return MakeShared<FPCGExFusePointsElement>(); }

void FPCGExFusePointsContext::PrepareForPoints(const UPCGExPointIO* InData)
{
	TArray<FName> Names;
	TArray<EPCGMetadataTypes> Types;
	InData->In->Metadata->GetAttributes(Names, Types);

	Attributes.Reset(Names.Num());

	const UPCGExFusePointsSettings* Settings = GetInputSettings<UPCGExFusePointsSettings>();
	check(Settings);

	TMap<FName, EPCGExFuseMethod> DesiredMethods;
	for (const FPCGExInputDescriptorWithFuseMethod& Descriptor : Settings->FuseMethodOverrides)
	{
		FPCGAttributePropertyInputSelector Selector = Descriptor.Selector.CopyAndFixLast(InData->In);
		if (Selector.GetSelection() == EPCGAttributePropertySelection::Attribute &&
			Selector.IsValid())
		{
			DesiredMethods.Add(Selector.GetAttributeName(), Descriptor.FuseMethod);
		}
	}

	for (FName Name : Names)
	{
		PCGExFuse::FAttribute& Attribute = Attributes.Emplace_GetRef();
		Attribute.Attribute = InData->Out->Metadata->GetMutableAttribute(Name);
		Attribute.FuseMethod = FuseMethod;

		if (EPCGExFuseMethod* FMethod = DesiredMethods.Find(Name)) { Attribute.FuseMethod = *FMethod; }
	}
}

FPCGContext* FPCGExFusePointsElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExFusePointsContext* Context = new FPCGExFusePointsContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	const UPCGExFusePointsSettings* Settings = Context->GetInputSettings<UPCGExFusePointsSettings>();
	check(Settings);

	Context->FuseMethod = Settings->FuseMethod;
	Context->Radius = FMath::Pow(Settings->Radius, 2);
	Context->bComponentWiseRadius = Settings->bComponentWiseRadius;
	Context->Radiuses = Settings->Radiuses;

#define PCGEX_FUSE_TRANSFERT(_NAME) Context->_NAME##FuseMethod = Settings->bOverride##_NAME ? Context->_NAME##FuseMethod : Settings->FuseMethod;
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
			Context->CurrentIndex = 0;
			Context->SetState(PCGExMT::EState::ProcessingPoints);
		}
	}

	auto Initialize = [&](UPCGExPointIO* PointIO)
	{
		Context->FusedPoints.Reset(PointIO->NumInPoints);
		Context->PrepareForPoints(PointIO);
		Context->InputAttributeMap.PrepareForPoints(PointIO->Out);
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

	if (Context->IsState(PCGExMT::EState::ProcessingPoints))
	{
		if (Context->AsyncProcessingCurrentPoints(Initialize, ProcessPoint))
		{
			Context->SetState(PCGExMT::EState::ProcessingGraph2ndPass);
		}
	}


	auto InitializeReconcile = [&]()
	{
		Context->OutPoints = &Context->CurrentIO->Out->GetMutablePoints();
	};

	auto FusePoints = [&](int32 ReadIndex)
	{
		FPCGPoint NewPoint;

		{
			FReadScopeLock ReadLock(Context->PointsLock);
			PCGExFuse::FFusedPoint& FusedPointData = Context->FusedPoints[ReadIndex];
			int32 NumFused = static_cast<double>(FusedPointData.Fused.Num());
			double AverageDivider = NumFused;

			FPCGPoint RootPoint = Context->CurrentIO->In->GetPoint((FusedPointData.MainIndex));
			Context->CurrentIO->Out->Metadata->InitializeOnSet(NewPoint.MetadataEntry, RootPoint.MetadataEntry, Context->CurrentIO->In->Metadata);

			FTransform& OutTransform = NewPoint.Transform;
#define PCGEX_FUSE_DECLARE(_TYPE, _NAME, _ACCESSOR, _DEFAULT_VALUE, ...) _TYPE Out##_NAME = Context->_NAME##FuseMethod == EPCGExFuseMethod::Skip ? RootPoint._ACCESSOR : _DEFAULT_VALUE;
			PCGEX_FUSE_FOREACH_POINTPROPERTY(PCGEX_FUSE_DECLARE)
#undef PCGEX_FUSE_DECLARE

			for (int i = 0; i < NumFused; i++)
			{
				const double Weight = 1 - (FusedPointData.Distances[i] / FusedPointData.MaxDistance);
				FPCGPoint Point = Context->CurrentIO->In->GetPoint(FusedPointData.Fused[i]);

#define PCGEX_FUSE_FUSE(_TYPE, _NAME, _ACCESSOR, ...) switch (Context->_NAME##FuseMethod){\
case EPCGExFuseMethod::Average: Out##_NAME += Point._ACCESSOR; break;\
case EPCGExFuseMethod::Min: Out##_NAME = PCGExMath::CWMin(Out##_NAME, Point._ACCESSOR); break;\
case EPCGExFuseMethod::Max: Out##_NAME = PCGExMath::CWMax(Out##_NAME, Point._ACCESSOR); break;\
case EPCGExFuseMethod::Weight: Out##_NAME = PCGExMath::Lerp(Out##_NAME, Point._ACCESSOR, Weight); break;\
}
				PCGEX_FUSE_FOREACH_POINTPROPERTY(PCGEX_FUSE_FUSE)
#undef PCGEX_FUSE_FUSE

				for (const PCGEx::FAttributeIdentity& Identity : Context->InputAttributeMap.Identities)
				{
					EPCGExFuseMethod AttributeFuseMethod = Context->FuseMethod;
					switch (Identity.UnderlyingType)
					{
#define PCGEX_FUSE_ATT(_TYPE, _NAME) case EPCGMetadataTypes::_NAME: switch (AttributeFuseMethod){\
case EPCGExFuseMethod::Average: Context->InputAttributeMap.SetAdd<_TYPE>(Identity.Name, Point.MetadataEntry, NewPoint.MetadataEntry); break;\
case EPCGExFuseMethod::Min: Context->InputAttributeMap.SetCWMin<_TYPE>(Identity.Name, Point.MetadataEntry, NewPoint.MetadataEntry); break;\
case EPCGExFuseMethod::Max: Context->InputAttributeMap.SetCWMax<_TYPE>(Identity.Name, Point.MetadataEntry, NewPoint.MetadataEntry); break;\
case EPCGExFuseMethod::Weight: Context->InputAttributeMap.SetLerp<_TYPE>(Identity.Name, NewPoint.MetadataEntry, Point.MetadataEntry, NewPoint.MetadataEntry, Weight); break;\
} break;
					PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_FUSE_ATT)
#undef PCGEX_FUSE_ATT
					}
				}
			}

#define PCGEX_FUSE_POST(_TYPE, _NAME, _ACCESSOR, ...)\
if(Context->_NAME##FuseMethod == EPCGExFuseMethod::Average){ PCGExMath::CWDivide(Out##_NAME, AverageDivider); }
			PCGEX_FUSE_FOREACH_POINTPROPERTY(PCGEX_FUSE_POST)
#undef PCGEX_FUSE_POST

			OutTransform.SetLocation(OutPosition);
			OutTransform.SetRotation(OutRotation.Quaternion());
			OutTransform.SetScale3D(OutScale);

#define PCGEX_FUSE_ASSIGN(_NAME, _ACCESSOR, ...) NewPoint._ACCESSOR = Out##_NAME;
			PCGEX_FUSE_FOREACH_POINTPROPERTY_ASSIGN(PCGEX_FUSE_ASSIGN)
#undef PCGEX_FUSE_ASSIGN

			NewPoint.SetExtents(OutExtents);
		}
		{
			FWriteScopeLock WriteLock(Context->PointsLock);
			Context->OutPoints->Add(NewPoint);
		}
	};

	if (Context->IsState(PCGExMT::EState::ProcessingGraph2ndPass))
	{
		if (PCGExMT::ParallelForLoop(Context, Context->FusedPoints.Num(), InitializeReconcile, FusePoints, Context->ChunkSize))
		{
			Context->OutPoints = nullptr;
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
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
