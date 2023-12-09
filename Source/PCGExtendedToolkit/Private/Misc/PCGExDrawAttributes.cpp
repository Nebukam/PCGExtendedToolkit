// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExDrawAttributes.h"

#define LOCTEXT_NAMESPACE "PCGExDrawAttributes"

PCGExData::EInit UPCGExDrawAttributesSettings::GetPointOutputInitMode() const { return PCGExData::EInit::NoOutput; }

#if WITH_EDITOR
FString FPCGExAttributeDebugDrawDescriptor::GetDisplayName() const
{
	if (bEnabled) { return FPCGExInputDescriptor::GetDisplayName(); }
	return "(Disabled) " + FPCGExInputDescriptor::GetDisplayName();
}
#endif

bool FPCGExAttributeDebugDraw::Validate(const UPCGPointData* InData)
{
	bValid = false;

	switch (Descriptor->ExpressedAs)
	{
	case EPCGExDebugExpression::Direction:
	case EPCGExDebugExpression::Point:
	case EPCGExDebugExpression::ConnectionToPosition:
		VectorGetter.Descriptor = static_cast<FPCGExInputDescriptor>(*Descriptor);
		VectorGetter.Axis = Descriptor->Axis;
		bValid = VectorGetter.Validate(InData);
		break;
	case EPCGExDebugExpression::ConnectionToIndex:
		IndexGetter.Descriptor = static_cast<FPCGExInputDescriptor>(*Descriptor);
		IndexGetter.Axis = Descriptor->Axis;
		IndexGetter.Field = Descriptor->Field;
		bValid = IndexGetter.Validate(InData);
		break;
	case EPCGExDebugExpression::Label:
		TextGetter.Descriptor = static_cast<FPCGExInputDescriptor>(*Descriptor);
		bValid = TextGetter.Validate(InData);
		break;
	default: ;
	}

	if (bValid)
	{
		SizeGetter.Capture(Descriptor->SizeAttribute);
		SizeGetter.Validate(InData);

		ColorGetter.Descriptor = Descriptor->ColorAttribute;
		ColorGetter.Validate(InData);
	}
	else
	{
		SizeGetter.bValid = false;
		ColorGetter.bValid = false;
	}

	return bValid;
}

double FPCGExAttributeDebugDraw::GetSize(const FPCGPoint& Point) const
{
	double Value = Descriptor->Size;
	if (Descriptor->bSizeFromAttribute && SizeGetter.bValid) { Value = SizeGetter.GetValue(Point) * Descriptor->Size; }
	return Value;
}

FColor FPCGExAttributeDebugDraw::GetColor(const FPCGPoint& Point) const
{
	if (Descriptor->bColorFromAttribute && ColorGetter.bValid)
	{
		const FVector Value = ColorGetter.GetValue(Point);
		return Descriptor->bColorIsLinear ? FColor(Value.X * 255.0f, Value.Y * 255.0f, Value.Z * 255.0f) : FColor(Value.X, Value.Y, Value.Z);
	}
	return Descriptor->Color;
}

FVector FPCGExAttributeDebugDraw::GetVector(const FPCGPoint& Point) const
{
	FVector OutVector = VectorGetter.GetValueSafe(Point, FVector::ZeroVector);
	if (Descriptor->ExpressedAs == EPCGExDebugExpression::Direction && Descriptor->bNormalizeBeforeSizing) { OutVector.Normalize(); }
	return OutVector;
}

FVector FPCGExAttributeDebugDraw::GetIndexedPosition(const FPCGPoint& Point, const UPCGPointData* PointData) const
{
	const int64 OutIndex = IndexGetter.GetValueSafe(Point, -1);
	if (OutIndex != -1) { return PointData->GetPoints()[OutIndex].Transform.GetLocation(); }
	return Point.Transform.GetLocation();
}

void FPCGExAttributeDebugDraw::Draw(const UWorld* World, const FVector& Start, const FPCGPoint& Point, const UPCGPointData* PointData) const
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
	}
}

void FPCGExAttributeDebugDraw::DrawDirection(const UWorld* World, const FVector& Start, const FPCGPoint& Point) const
{
	const FVector Dir = GetVector(Point) * GetSize(Point);
	DrawDebugDirectionalArrow(World, Start, Start + Dir, Dir.Length() * 0.05f, GetColor(Point), true, -1, 0, Descriptor->Thickness);
}

void FPCGExAttributeDebugDraw::DrawConnection(const UWorld* World, const FVector& Start, const FPCGPoint& Point, const FVector& End) const
{
	DrawDebugLine(World, Start, End, GetColor(Point), true, -1, 0, Descriptor->Thickness);
}

void FPCGExAttributeDebugDraw::DrawPoint(const UWorld* World, const FVector& Start, const FPCGPoint& Point) const
{
	const FVector End = GetVector(Point);
	DrawDebugPoint(World, End, GetSize(Point), GetColor(Point), true, -1, 0);
}

void FPCGExAttributeDebugDraw::DrawLabel(const UWorld* World, const FVector& Start, const FPCGPoint& Point) const
{
	FString Text = TextGetter.GetValueSafe(Point, ".");
	DrawDebugString(World, Start, *Text, nullptr, GetColor(Point), 99999.0f, false, GetSize(Point));
}

UPCGExDrawAttributesSettings::UPCGExDrawAttributesSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DebugSettings.PointScale = 0.0f;
	if (DebugList.IsEmpty())
	{
		FPCGExAttributeDebugDrawDescriptor& Forward = DebugList.Emplace_GetRef();
		Forward.Selector.Update(TEXT("$transform"));
		Forward.Axis = EPCGExAxis::Forward;
		Forward.Color = FColor::Red;
		Forward.Size = 50;

		FPCGExAttributeDebugDrawDescriptor& Right = DebugList.Emplace_GetRef();
		Right.Selector.Update(TEXT("$transform"));
		Right.Axis = EPCGExAxis::Right;
		Right.Color = FColor::Green;
		Right.Size = 50;

		FPCGExAttributeDebugDrawDescriptor& Up = DebugList.Emplace_GetRef();
		Up.Selector.Update(TEXT("$transform"));
		Up.Axis = EPCGExAxis::Up;
		Up.Color = FColor::Blue;
		Up.Size = 50;
	}
}

#if WITH_EDITOR
TArray<FPCGPinProperties> UPCGExDrawAttributesSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> None;
	return None;
}

void UPCGExDrawAttributesSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	DebugSettings.PointScale = 0.0f;
	for (FPCGExAttributeDebugDrawDescriptor& Descriptor : DebugList) { Descriptor.UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

FPCGElementPtr UPCGExDrawAttributesSettings::CreateElement() const { return MakeShared<FPCGExDrawAttributesElement>(); }

void FPCGExDrawAttributesContext::PrepareForPoints(const UPCGPointData* PointData)
{
	for (FPCGExAttributeDebugDraw& DebugInfos : DebugList)
	{
		DebugInfos.Validate(PointData);
	}
}

FPCGContext* FPCGExDrawAttributesElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExDrawAttributesContext* Context = new FPCGExDrawAttributesContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	const UPCGExDrawAttributesSettings* Settings = Context->GetInputSettings<UPCGExDrawAttributesSettings>();
	check(Settings);

	//const PCGEx::FPinAttributeInfos* ExtraAttributes = Settings->GetInputAttributeInfos(PCGEx::SourcePointsLabel);

	Context->DebugList.Empty();
	for (const FPCGExAttributeDebugDrawDescriptor& Descriptor : Settings->DebugList)
	{
		FPCGExAttributeDebugDrawDescriptor& MutableDescriptor = (const_cast<FPCGExAttributeDebugDrawDescriptor&>(Descriptor));
		//if (ExtraAttributes) { ExtraAttributes->PushToDescriptor(MutableDescriptor); }

		if (!Descriptor.bEnabled) { continue; }

		FPCGExAttributeDebugDraw& Drawer = Context->DebugList.Emplace_GetRef();
		Drawer.Descriptor = &MutableDescriptor;
	}

	return Context;
}

bool FPCGExDrawAttributesElement::Validate(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Validate(InContext)) { return false; }

	const FPCGExDrawAttributesContext* Context = static_cast<FPCGExDrawAttributesContext*>(InContext);

	if (Context->DebugList.IsEmpty())
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("MissingDebugInfos", "Debug list is empty."));
	}

	if (!PCGExDebug::NotifyExecute(InContext))
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingDebugManager", "Could not find a PCGEx Debug Manager node in your graph."));
		return false;
	}

	return true;
}

bool FPCGExDrawAttributesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDrawAttributesElement::Execute);

#if WITH_EDITOR

	FPCGExDrawAttributesContext* Context = static_cast<FPCGExDrawAttributesContext*>(InContext);

	const UPCGExDrawAttributesSettings* Settings = Context->GetInputSettings<UPCGExDrawAttributesSettings>();
	check(Settings);

	if (Context->IsSetup())
	{
		if (!Settings->bDebug) { return true; }
		if (!Validate(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else { Context->SetState(PCGExMT::State_ProcessingPoints); }
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		auto Initialize = [&](PCGExData::FPointIO& PointIO)
		{
			Context->PrepareForPoints(PointIO.GetIn());
		};

		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			const FPCGPoint& Point = PointIO.GetInPoint(PointIndex);
			const FVector Start = Point.Transform.GetLocation();
			DrawDebugPoint(Context->World, Start, 1.0f, FColor::White, true);
			for (FPCGExAttributeDebugDraw& Drawer : Context->DebugList)
			{
				if (!Drawer.bValid) { continue; }
				Drawer.Draw(Context->World, Start, Point, PointIO.GetIn());
			}
		};

		if (Context->ProcessCurrentPoints(Initialize, ProcessPoint, true))
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
	}

	if (Context->IsDone()) { return true; }

	return false;

#elif
	return  true;
#endif
}

#undef LOCTEXT_NAMESPACE
