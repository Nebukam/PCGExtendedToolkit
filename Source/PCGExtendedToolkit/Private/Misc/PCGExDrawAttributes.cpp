// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExDrawAttributes.h"

#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "DrawDebugHelpers.h"
#include "Editor.h"
#include "PCGComponent.h"

#define LOCTEXT_NAMESPACE "PCGExDrawAttributes"

PCGEx::EIOInit UPCGExDrawAttributesSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::NoOutput; }

FPCGElementPtr UPCGExDrawAttributesSettings::CreateElement() const
{
	return MakeShared<FPCGExDrawAttributesElement>();
}

TArray<FPCGPinProperties> UPCGExDrawAttributesSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Empty;
	return Empty;
}

void UPCGExDrawAttributesSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
#if  WITH_EDITOR

	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	if (EditorWorld) { FlushPersistentDebugLines(EditorWorld); }

#endif

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void FPCGExDrawAttributesContext::PrepareForPoints(UPCGPointData* PointData)
{
	for (FPCGExAttributeDebugDraw DebugInfos : DebugList)
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

	Context->DebugList = Settings->DebugList;

	return Context;
}

bool FPCGExDrawAttributesElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDrawAttributesElement::Execute);

#if  WITH_EDITOR

	FPCGExDrawAttributesContext* Context = static_cast<FPCGExDrawAttributesContext*>(InContext);

	const UPCGExDrawAttributesSettings* Settings = Context->GetInputSettings<UPCGExDrawAttributesSettings>();
	check(Settings);

	UWorld* World = PCGEx::Common::GetWorld(Context);

	if (Context->IsSetup())
	{
		FlushPersistentDebugLines(World);

		if (!Settings->bDebug)
		{
			Context->Points->OutputTo(Context);
			return true;
		}

		Context->SetState(PCGExMT::EState::ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::EState::ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO())
		{
			Context->SetState(PCGExMT::EState::Done); //No more points
		}
		else
		{
			Context->SetState(PCGExMT::EState::ProcessingPoints);
		}
	}

	auto ProcessPoint = [&Context, &World, &Settings](
		const FPCGPoint& Point, int32 ReadIndex, UPCGExPointIO* IO)
	{
		// FWriteScopeLock ScopeLock(Context->ContextLock);
		const FVector Start = Point.Transform.GetLocation();
		for (FPCGExAttributeDebugDraw& Drawer : Context->DebugList) { Drawer.Draw(World, Start, Point, IO->In); }
	};

	auto Initialize = [&Context](UPCGExPointIO* IO)
	{
		Context->PrepareForPoints(IO->In);
	};

	if (Context->IsState(PCGExMT::EState::ProcessingPoints))
	{
		Initialize(Context->CurrentIO);
		for (int i = 0; i < Context->CurrentIO->NumPoints; i++) { ProcessPoint(Context->CurrentIO->Out->GetPoint(i), i, Context->CurrentIO); }
		Context->SetState(PCGExMT::EState::ReadyForNextParams);
	}

	if (Context->IsState(PCGExMT::EState::Done))
	{
		//Context->OutputPoints();
		return true;
	}

#elif
	return  true;
#endif

	return false;
}

#undef LOCTEXT_NAMESPACE
