// Fill out your copyright notice in the Description page of Project Settings.

#include "Relational/PCGExFindPaths.h"

#include "Data/PCGExRelationalData.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExFindPaths"

#if WITH_EDITOR
FText UPCGExFindPathsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("PCGDirectionalRelationshipsTooltip", "Write the current point index to an attribute.");
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGExFindPathsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	return PinProperties;
}

FPCGElementPtr UPCGExFindPathsSettings::CreateElement() const
{
	return MakeShared<FPCGExFindPathsElement>();
}

bool FPCGExFindPathsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindPathsElement::Execute);

	UPCGExRelationalParamsData* Params = GetRelationalParams(Context);
	if (!Params) { return true; }

	const UPCGExFindPathsSettings* Settings = Context->GetInputSettings<UPCGExFindPathsSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExRelational::SourceLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	return true;
}

#undef LOCTEXT_NAMESPACE
