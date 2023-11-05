// Fill out your copyright notice in the Description page of Project Settings.

#include "Relational/PCGExFindPaths.h"

#include "Relational/PCGExRelationalData.h"
#include "Helpers/PCGAsync.h"
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

	const UPCGExRelationalData* RelationalData = GetFirstRelationalData(Context);
	if (!RelationalData) { return true; }

	const UPCGExFindPathsSettings* Settings = Context->GetInputSettings<UPCGExFindPathsSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExRelational::SourceLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	for (const FPCGTaggedData& Source : Sources)
	{
		const UPCGSpatialData* InSpatialData = Cast<UPCGSpatialData>(Source.Data);

		if (!InSpatialData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidInputData", "Invalid input data"));
			continue;
		}

		const UPCGPointData* InPointData = InSpatialData->ToPointData(Context);
		if (!InPointData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("CannotConvertToPointData", "Cannot convert input Spatial data to Point data"));
			continue;
		}

		FPCGMetadataAttribute<FPCGExRelationData>* RelationAttribute = FindRelationalAttribute<FPCGExRelationData>(RelationalData, InPointData);
		if (!RelationAttribute)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("CannotFindRelationalAttribute", "Cannot find relational data. Make sure to compute it first (i.e CaptureNeighbors)."));
			continue;
		}
		
		// Initialize output dataset
		//UPCGPointData* OutputData = NewObject<UPCGPointData>();
		//OutputData->InitializeFromData(InPointData);
		//Outputs.Add_GetRef(Source).Data = OutputData;

		return  true;
		
		
	}


	return true;
}

#undef LOCTEXT_NAMESPACE
