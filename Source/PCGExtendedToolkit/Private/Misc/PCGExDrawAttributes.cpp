// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExDrawAttributes.h"

#define LOCTEXT_NAMESPACE "PCGExDrawAttributes"
#define PCGEX_NAMESPACE DrawAttributes

PCGExData::EInit UPCGExDrawAttributesSettings::GetMainOutputInitMode() const { return PCGExData::EInit::Forward; }

FPCGExDrawAttributesContext::~FPCGExDrawAttributesContext()
{
	DebugList.Empty();
}

#if WITH_EDITOR
FString FPCGExAttributeDebugDrawDescriptor::GetDisplayName() const
{
	if (bEnabled) { return FPCGExInputDescriptor::GetDisplayName(); }
	return "(Disabled) " + FPCGExInputDescriptor::GetDisplayName();
}
#endif

bool FPCGExAttributeDebugDraw::Bind(const PCGExData::FPointIO& PointIO)
{
	bValid = false;

	switch (Descriptor->ExpressedAs)
	{
	case EPCGExDebugExpression::Direction:
	case EPCGExDebugExpression::Point:
	case EPCGExDebugExpression::ConnectionToPosition:
		VectorGetter.Capture(*Descriptor);
		bValid = VectorGetter.Grab(PointIO);
		break;
	case EPCGExDebugExpression::ConnectionToIndex:
		IndexGetter.Capture(*Descriptor);
		bValid = IndexGetter.Grab(PointIO);
		break;
	case EPCGExDebugExpression::Label:
		//TextGetter.Capture(*Descriptor);
		//bValid = TextGetter.Grab(PointIO);
		bValid = false;
		break;
	case EPCGExDebugExpression::Boolean:
		SingleGetter.Capture(*Descriptor);
		bValid = SingleGetter.Grab(PointIO);
		break;
	default: ;
	}

	if (bValid)
	{
		if (Descriptor->bSizeFromAttribute)
		{
			SizeGetter.Capture(Descriptor->LocalSizeAttribute);
			SizeGetter.Grab(PointIO);
		}
		else
		{
			SizeGetter.bValid = false;
		}

		if (Descriptor->bColorFromAttribute)
		{
			ColorGetter.Capture(Descriptor->LocalColorAttribute);
			ColorGetter.Grab(PointIO);
		}
		else
		{
			ColorGetter.bValid = false;
		}
	}
	else
	{
		SizeGetter.bValid = false;
		ColorGetter.bValid = false;
	}

	return bValid;
}

double FPCGExAttributeDebugDraw::GetSize(const PCGEx::FPointRef& Point) const
{
	double Value = Descriptor->Size;
	if (Descriptor->bSizeFromAttribute && SizeGetter.bValid) { Value = SizeGetter[Point.Index] * Descriptor->Size; }
	return Value;
}

FColor FPCGExAttributeDebugDraw::GetColor(const PCGEx::FPointRef& Point) const
{
	if (Descriptor->bColorFromAttribute && ColorGetter.bValid)
	{
		const FVector Value = ColorGetter[Point.Index];
		return Descriptor->bColorIsLinear ? FColor(Value.X * 255.0f, Value.Y * 255.0f, Value.Z * 255.0f) : FColor(Value.X, Value.Y, Value.Z);
	}
	return Descriptor->Color;
}

double FPCGExAttributeDebugDraw::GetSingle(const PCGEx::FPointRef& Point) const
{
	return SingleGetter[Point.Index];
}

FVector FPCGExAttributeDebugDraw::GetVector(const PCGEx::FPointRef& Point) const
{
	FVector OutVector = VectorGetter[Point.Index];
	if (Descriptor->ExpressedAs == EPCGExDebugExpression::Direction && Descriptor->bNormalizeBeforeSizing) { OutVector.Normalize(); }
	return OutVector;
}

FVector FPCGExAttributeDebugDraw::GetIndexedPosition(const PCGEx::FPointRef& Point, const UPCGPointData* PointData) const
{
	const TArray<FPCGPoint> Points = PointData->GetPoints();
	const int64 OutIndex = IndexGetter.SafeGet(Point.Index, -1);
	if (OutIndex != -1) { return Points[PCGExMath::Tile<int32>(OutIndex, 0, Points.Num() - 1)].Transform.GetLocation(); }
	return Point.Point->Transform.GetLocation();
}

void FPCGExAttributeDebugDraw::Draw(const UWorld* World, const FVector& Start, const PCGEx::FPointRef& Point, const UPCGPointData* PointData) const
{
	switch (Descriptor->ExpressedAs)
	{
	case EPCGExDebugExpression::Direction:
		DrawDirection(World, Start, Point);
		break;
	case EPCGExDebugExpression::ConnectionToIndex:
		DrawConnection(World, Start, Point, GetIndexedPosition(Point, PointData));
		break;
	case EPCGExDebugExpression::ConnectionToPosition:
		DrawConnection(World, Start, Point, GetVector(Point));
		break;
	case EPCGExDebugExpression::Point:
		DrawPoint(World, Start, Point);
		break;
	case EPCGExDebugExpression::Label:
		DrawLabel(World, Start, Point);
		break;
	case EPCGExDebugExpression::Boolean:
		DrawSingle(World, Start, Point);
		break;
	default: ;
	}
}

void FPCGExAttributeDebugDraw::DrawDirection(const UWorld* World, const FVector& Start, const PCGEx::FPointRef& Point) const
{
#if WITH_EDITOR
	const FVector Dir = GetVector(Point) * GetSize(Point);
	DrawDebugDirectionalArrow(World, Start, Start + Dir, Dir.Length() * 0.05f, GetColor(Point), true, -1, Descriptor->DepthPriority, Descriptor->Thickness);
#endif
}

void FPCGExAttributeDebugDraw::DrawConnection(const UWorld* World, const FVector& Start, const PCGEx::FPointRef& Point, const FVector& End) const
{
#if WITH_EDITOR
	DrawDebugLine(World, Start, Descriptor->bAsOffset ? Start + End : End, GetColor(Point), true, -1, Descriptor->DepthPriority, Descriptor->Thickness);
#endif
}

void FPCGExAttributeDebugDraw::DrawPoint(const UWorld* World, const FVector& Start, const PCGEx::FPointRef& Point) const
{
#if WITH_EDITOR
	const FVector End = GetVector(Point);
	DrawDebugPoint(World, Descriptor->bAsOffset ? Start + End : End, GetSize(Point), GetColor(Point), true, -1, Descriptor->DepthPriority);
#endif
}

void FPCGExAttributeDebugDraw::DrawSingle(const UWorld* World, const FVector& Start, const PCGEx::FPointRef& Point) const
{
#if WITH_EDITOR
	const double End = GetSingle(Point);
	DrawDebugPoint(World, Start, GetSize(Point), End <= 0 ? Descriptor->SecondaryColor : GetColor(Point), true, -1, Descriptor->DepthPriority);
#endif
}

void FPCGExAttributeDebugDraw::DrawLabel(const UWorld* World, const FVector& Start, const PCGEx::FPointRef& Point) const
{
#if WITH_EDITOR
	const FString Text = TextGetter.SafeGet(Point.Index, ".");
	DrawDebugString(World, Start, *Text, nullptr, GetColor(Point), 99999.0f, false, GetSize(Point));
#endif
}

UPCGExDrawAttributesSettings::UPCGExDrawAttributesSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	DebugSettings.PointScale = 0.0f;
	if (DebugList.IsEmpty())
	{
		FPCGExAttributeDebugDrawDescriptor& Forward = DebugList.Emplace_GetRef();
		Forward.Selector.Update(TEXT("$transform.Forward"));
		Forward.Color = FColor::Red;
		Forward.Size = 50;

		FPCGExAttributeDebugDrawDescriptor& Right = DebugList.Emplace_GetRef();
		Right.Selector.Update(TEXT("$transform.Right"));
		Right.Color = FColor::Green;
		Right.Size = 50;

		FPCGExAttributeDebugDrawDescriptor& Up = DebugList.Emplace_GetRef();
		Up.Selector.Update(TEXT("$transform.Up"));
		Up.Color = FColor::Blue;
		Up.Size = 50;
	}
#endif
}

#if WITH_EDITOR
void UPCGExDrawAttributesSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	DebugSettings.PointScale = 0.0f;
	for (FPCGExAttributeDebugDrawDescriptor& Descriptor : DebugList) { Descriptor.UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGEX_INITIALIZE_ELEMENT(DrawAttributes)

bool FPCGExDrawAttributesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

#if WITH_EDITOR
	PCGEX_CONTEXT_AND_SETTINGS(DrawAttributes)

	if (!Settings->bPCGExDebug) { return false; }

	Context->DebugList.Empty();
	for (const FPCGExAttributeDebugDrawDescriptor& Descriptor : Settings->DebugList)
	{
		FPCGExAttributeDebugDrawDescriptor& MutableDescriptor = (const_cast<FPCGExAttributeDebugDrawDescriptor&>(Descriptor));
		//if (ExtraAttributes) { ExtraAttributes->PushToDescriptor(MutableDescriptor); }

		if (!Descriptor.bEnabled) { continue; }

		FPCGExAttributeDebugDraw& Drawer = Context->DebugList.Emplace_GetRef();
		Drawer.Descriptor = &MutableDescriptor;
	}

	if (Context->DebugList.IsEmpty())
	{
		PCGE_LOG(Warning, GraphAndLog, FTEXT("Debug list is empty."));
	}

#endif

	return true;
}

bool FPCGExDrawAttributesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDrawAttributesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(DrawAttributes)

#if WITH_EDITOR

	if (Context->IsSetup())
	{
		if (!Boot(Context))
		{
			DisabledPassThroughData(Context);
			return true;
		}
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
		return false;
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else { Context->SetState(PCGExMT::State_ProcessingPoints); }
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		Context->CurrentIO->CreateInKeys();
		for (FPCGExAttributeDebugDraw& DebugInfos : Context->DebugList) { DebugInfos.Bind(*Context->CurrentIO); }

		for (int PointIndex = 0; PointIndex < Context->CurrentIO->GetNum(); PointIndex++)
		{
			const PCGEx::FPointRef& Point = Context->CurrentIO->GetInPointRef(PointIndex);
			const FVector Start = Point.Point->Transform.GetLocation();
			DrawDebugPoint(Context->World, Start, 1.0f, FColor::White, true);
			for (FPCGExAttributeDebugDraw& Drawer : Context->DebugList)
			{
				if (!Drawer.bValid) { continue; }
				Drawer.Draw(Context->World, Start, Point, Context->CurrentIO->GetIn());
			}
		}

		Context->CurrentIO->CleanupKeys();
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone()) { DisabledPassThroughData(Context); }

	return Context->IsDone();

#endif

	DisabledPassThroughData(Context);
	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
