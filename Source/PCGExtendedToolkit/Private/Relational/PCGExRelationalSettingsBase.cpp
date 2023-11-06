// Fill out your copyright notice in the Description page of Project Settings.

#include "Relational\PCGExRelationalSettingsBase.h"

#include "Data/PCGExRelationalData.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGExRelationalDataHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExRelationalSettings"

#pragma region UPCGSettings interface

namespace PCGExRelational
{
	const FName SourceLabel = TEXT("Source");
	const FName SourceRelationalParamsLabel = TEXT("RelationalParams");
	const FName SourceRelationalDataLabel = TEXT("RelationalData");
	const FName OutputPointsLabel = TEXT("Points");
	const FName OutputRelationalDataLabel = TEXT("RelationalData");
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

#if WITH_EDITOR
	PinPropertySource.Tooltip = LOCTEXT("PCGExSourcePinTooltip", "For each of the source points, their index position in the data will be written to an attribute.");
#endif // WITH_EDITOR

	if (GetRequiresRelationalParams())
	{
		FPCGPinProperties& PinPropertyParams = PinProperties.Emplace_GetRef(PCGExRelational::SourceRelationalParamsLabel, EPCGDataType::Param, false, false);
#if WITH_EDITOR
		PinPropertyParams.Tooltip = LOCTEXT("PCGExRelationalParamsPinTooltip", "Relational Params.");
#endif // WITH_EDITOR
	}

	if (GetRequiresRelationalData())
	{
		FPCGPinProperties& PinPropertyData = PinProperties.Emplace_GetRef(PCGExRelational::SourceRelationalDataLabel, EPCGDataType::Param, false, true);
#if WITH_EDITOR
		PinPropertyData.Tooltip = LOCTEXT("PCGExRelationalDataPinTooltip", "Relational Datas.");
#endif // WITH_EDITOR
	}

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExRelationalSettingsBase::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPointsOutput = PinProperties.Emplace_GetRef(PCGExRelational::OutputPointsLabel, EPCGDataType::Point);
	FPCGPinProperties& PinRelationalDatasOutput = PinProperties.Emplace_GetRef(PCGExRelational::OutputRelationalDataLabel, EPCGDataType::Param);

#if WITH_EDITOR
	PinPointsOutput.Tooltip = LOCTEXT("PCGExOutputPointsPinTooltip", "The source points.");
	PinRelationalDatasOutput.Tooltip = LOCTEXT("PCGExOutputDataPinTooltip", "A RelationalData object for each source inputs");
#endif // WITH_EDITOR

	return PinProperties;
}

#pragma endregion

bool FPCGExRelationalProcessingElementBase::ExecuteInternal(FPCGContext* Context) const
{
	// if require params input
	/*
	for each(params)
	{
		for each(points :: relation data)
		{
		
		}
	}
	*/
}

void FPCGExRelationalProcessingElementBase::ExecuteForEachParamsInput(FPCGContext* Context, const TFunction<void(const UPCGExRelationalParamsData*)>& ParamsFunc)
{
	TArray<FPCGTaggedData> ParamSources = Context->InputData.GetInputsByPin(PCGExRelational::SourceRelationalParamsLabel);
	TArray<UPCGExRelationalParamsData*> Params;

	if (!FPCGExRelationalDataHelpers::FindRelationalParams(ParamSources, Params))
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoParams", "No RelationalParams found."));
		return;
	}

	for (const UPCGExRelationalParamsData* ParamsData : Params) { ParamsFunc(ParamsData); }
}

void FPCGExRelationalProcessingElementBase::ExecuteForEachRelationalPairsInput(FPCGContext* Context, const TFunction<void(const UPCGExRelationalParamsData*)>& ParamsFunc)
{
	TArray<FPCGTaggedData> PointSources = Context->InputData.GetInputsByPin(PCGExRelational::SourceLabel);
	TArray<FPCGTaggedData> RelationalSources = Context->InputData.GetInputsByPin(PCGExRelational::SourceRelationalDataLabel);

	FPCGExDataMapping DataMapping = FPCGExDataMapping{};
	FPCGExRelationalDataHelpers::BuildRelationalMapping(RelationalSources, PointSources, DataMapping);

	for (const UPCGExRelationalParamsData* ParamsData : Params)
	{
		ParamsFunc(ParamsData);
	}
}

void FPCGExRelationalProcessingElementBase::ExecuteForEachPairs(FPCGContext* Context, const TFunction<void(const UPCGExRelationalParamsData*)>& ParamsFunc)
{
	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExRelational::SourceRelationalParamsLabel);
	TArray<UPCGExRelationalParamsData*> Params;

	if (!FPCGExRelationalDataHelpers::FindRelationalParams(Sources, Params))
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoParams", "No RelationalParams found."));
		return;
	}

	for (const UPCGExRelationalParamsData* ParamsData : Params) { ParamsFunc(ParamsData); }
}

/**
 * 
 * @tparam T 
 * @param RelationalData 
 * @param PointData 
 * @return 
 */
template <RelationalDataStruct T>
FPCGMetadataAttribute<T>* FPCGExRelationalProcessingElementBase::PrepareDataAttributes_UNSUPPORTED(const UPCGExRelationalData* RelationalData, UPCGPointData* PointData)
{
	const int NumSlot = RelationalData->Params->RelationSlots.Num();

	FPCGExRelationData Default = {};
	Default.Details.Reserve(NumSlot);
	for (int i = 0; i < NumSlot; i++)
	{
		Default.Details.Add(FPCGExRelationDetails{});
	}

	FPCGMetadataAttribute<T>* Attribute = PointData->Metadata->FindOrCreateAttribute(RelationalData->Params->RelationalIdentifier, Default, false, true, true);
	return Attribute;
}

/**
 * 
 * @tparam T 
 * @param RelationalData 
 * @param PointData 
 * @return 
 */
template <RelationalDataStruct T>
FPCGMetadataAttribute<T>* FPCGExRelationalProcessingElementBase::FindRelationalAttribute_UNSUPPORTED(const UPCGExRelationalData* RelationalData, const UPCGPointData* PointData)
{
	return PointData->Metadata->GetMutableTypedAttribute<T>(RelationalData->Params->RelationalIdentifier);
}

/**
 * 
 * @param Context 
 * @return 
 */
const bool FPCGExRelationalProcessingElementBase::CheckRelationalParams(const FPCGContext* Context) const
{
	TArray<FPCGTaggedData> RelationalParamsSources = Context->InputData.GetInputsByPin(PCGExRelational::SourceRelationalParamsLabel);
	if (RelationalParamsSources.Num() <= 0)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingRelationalData", "Requires Relational Data input."));
		return false;
	}

	if (const UPCGExRelationalParamsData* Params = Cast<UPCGExRelationalParamsData>(RelationalParamsSources[0].Data))
	{
		return true;
	}

	PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoRelationalParams", "RelationalData Input does not contains any RelationalData."));
	return false;
}

/**
 * Unsafe as-is, make sure to use CheckRelationalParams first.
 * @param Context 
 * @return 
 */
UPCGExRelationalParamsData* FPCGExRelationalProcessingElementBase::GetRelationalParams(const FPCGContext* Context) const
{
	TArray<FPCGTaggedData> RelationalParamsSources = Context->InputData.GetInputsByPin(PCGExRelational::SourceRelationalParamsLabel);
	const UPCGExRelationalParamsData* Params = Cast<UPCGExRelationalParamsData>(RelationalParamsSources[0].Data);
	if(!Params){return nullptr;}
	return const_cast<UPCGExRelationalParamsData*>(Params);
}

template <typename T>
const T* FPCGExRelationalProcessingElementBase::GetSettings(FPCGContext* Context) const
{
	const T* Settings = Context->GetInputSettings<T>();
	check(Settings);

	return Settings;
}

/**
 * Attempts to find matching relational data with specific params
 * @param PointData 
 * @param OutRelationalData 
 * @return 
 */
bool FPCGExRelationalProcessingElementBase::TryGetRelationalData(const FPCGContext* Context, const UPCGExRelationalParamsData* InParams, const UPCGPointData* PointData, const UPCGExRelationalData*& OutRelationalData) const
{
	TArray<FPCGTaggedData> RelationalDatasSources = Context->InputData.GetInputsByPin(PCGExRelational::SourceRelationalDataLabel);
	for (const FPCGTaggedData& Source : RelationalDatasSources)
	{
		const UPCGExRelationalData* InRelationalData = Cast<UPCGExRelationalData>(Source.Data);
		// TODO: Find a more secure way to bind data
		if (InRelationalData->Params == InParams && PointData->Metadata->HasAttribute(InParams->RelationalIdentifier))
		{
			OutRelationalData = InRelationalData;
			return true;
		}
	}
	return false;
}

/**
 * Attempts to find matching relational data
 * @param PointData 
 * @param OutRelationalData 
 * @return 
 */
bool FPCGExRelationalProcessingElementBase::TryGetRelationalData(const FPCGContext* Context, const UPCGPointData* PointData, const UPCGExRelationalData*& OutRelationalData) const
{
	TArray<FPCGTaggedData> RelationalDatasSources = Context->InputData.GetInputsByPin(PCGExRelational::SourceRelationalDataLabel);
	for (const FPCGTaggedData& Source : RelationalDatasSources)
	{
		const UPCGExRelationalData* InRelationalData = Cast<UPCGExRelationalData>(Source.Data);
		// TODO: Find a more secure way to bind data
		if (PointData->Metadata->HasAttribute(InRelationalData->Params->RelationalIdentifier))
		{
			OutRelationalData = InRelationalData;
			return true;
		}
	}
	return false;
}

/**
 * Create a new relational data in context outputs for the provided PointData
 * This requires GetRequiresRelationalParams to return true
 * @param Context 
 * @param PointData 
 * @return 
 */
const UPCGExRelationalData* FPCGExRelationalProcessingElementBase::CreateRelationalData(FPCGContext* Context, const UPCGPointData* PointData) const
{
	const FPCGExRelationalProcessingElementBase* Settings = Context->GetInputSettings<FPCGExRelationalProcessingElementBase>();
	check(Settings);

	const UPCGExRelationalData* RelationalData = GetRelationalParams(Context);

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	UPCGExRelationalData* NewRelationalDataData = NewObject<UPCGExRelationalData>();
	Outputs.Emplace_GetRef().Data = NewRelationalDataData;

	return NewRelationalDataData;
}


#undef LOCTEXT_NAMESPACE
