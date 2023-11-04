// Fill out your copyright notice in the Description page of Project Settings.

#include "Relational/PCGExRelationalSettings.h"

#include "Relational/PCGExRelationalData.h"
#include "Helpers/PCGAsync.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExRelationalSettings"

namespace PCGExRelational
{
	const FName SourceLabel = TEXT("Source");
	const FName SourceRelationalLabel = TEXT("RelationalParams");
}

#if WITH_EDITOR
FText UPCGExRelationalSettingsBase::GetNodeTooltipText() const
{
	return LOCTEXT("PCGDirectionalRelationshipsTooltip", "Write the current point index to an attribute.");
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGExRelationalSettingsBase::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	FPCGPinProperties& PinPropertySource = PinProperties.Emplace_GetRef(PCGExRelational::SourceLabel, EPCGDataType::Point);

	FPCGPinProperties& PinPropertyParams = PinProperties.Emplace_GetRef(PCGExRelational::SourceRelationalLabel, EPCGDataType::Param, false, false);

#if WITH_EDITOR
	PinPropertySource.Tooltip = LOCTEXT("PCGExSourcePinTooltip", "For each of the source points, their index position in the data will be written to an attribute.");

	PinPropertyParams.Tooltip = LOCTEXT("PCGExRelationalParamsPinTooltip", "Relational Params.");
#endif // WITH_EDITOR

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExRelationalSettingsBase::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertyOutput = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPropertyOutput.Tooltip = LOCTEXT("PCGExOutputPinTooltip", "The source points will be output with the newly added attribute.");
#endif // WITH_EDITOR

	return PinProperties;
}

template <RelationalDataStruct T>
FPCGMetadataAttribute<T>* FPCGExRelationalProcessingElementBase::PrepareData(const UPCGExRelationalData* RelationalData, UPCGPointData* PointData)
{
	int NumSlot = RelationalData->Slots.Num(); //Number of slots

	FPCGExRelationAttributeData Default = {};
	Default.Indices.Reserve(NumSlot);
	for (int i = 0; i < NumSlot; i++)
	{
		Default.Indices.Add(-1);
	}

	FPCGMetadataAttribute<T>* Attribute = PointData->Metadata->FindOrCreateAttribute(RelationalData->RelationalIdentifier, Default, false, true, true);
	return Attribute;
}

template <RelationalDataStruct T>
FPCGMetadataAttribute<T>* FPCGExRelationalProcessingElementBase::FindRelationalAttribute(const UPCGExRelationalData* RelationalData, const UPCGPointData* PointData)
{
	return PointData->Metadata->GetMutableTypedAttribute<T>(RelationalData->RelationalIdentifier);
}

const UPCGExRelationalData* FPCGExRelationalProcessingElementBase::GetFirstRelationalData(FPCGContext* Context) const
{
	TArray<FPCGTaggedData> RelationalDataSources = Context->InputData.GetInputsByPin(PCGExRelational::SourceRelationalLabel);
	if (RelationalDataSources.Num() <= 0)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingRelationalData", "Requires Relational Data input."));
		return nullptr;
	}

	for (FPCGTaggedData Source : RelationalDataSources)
	{
		if (const UPCGExRelationalData* SourceData = Cast<UPCGExRelationalData>(Source.Data))
		{
			return SourceData;
		}
	}

	PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoRelationalData", "RelationalData Input does not contains any RelationalData."));
	return nullptr;
}


#undef LOCTEXT_NAMESPACE
