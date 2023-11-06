// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGExCommon.h"
#include "Data/PCGExRelationalData.h"
#include "Data/PCGExRelationalParamsData.h"
#include "PCGExRelationalDataHelpers.generated.h"

class UPCGPointData;

template <typename T>
concept TBindableData = std::is_base_of_v<UPCGPointData, T>;

template <typename T>
concept TBindableRelationalData = std::is_base_of_v<UPCGExRelationalData, T>;

USTRUCT()
struct PCGEXTENDEDTOOLKIT_API FPCGExRelationalPair
{
	GENERATED_BODY()

public:
	UPCGPointData* SourcePointData;
	UPCGPointData* PointData;
	UPCGExRelationalData* RelationalData = nullptr;

	void Capture(const FPCGExPointDataPair& DataPair){
		SourcePointData = DataPair.SourcePointData;
		PointData = DataPair.OutputPointData;
	}
	
};

USTRUCT()
struct PCGEXTENDEDTOOLKIT_API FPCGExParamDataMapping
{
	GENERATED_BODY()

	FPCGExParamDataMapping()
	{
		RelationalDatasMap.Empty();
		RelationalPairs.Empty();
	}

	FPCGExParamDataMapping(UPCGExRelationalParamsData* InParams)
	{
		RelationalDatasMap.Empty();
		RelationalPairs.Empty();
		Params = InParams;
	}

public:
	UPCGExRelationalParamsData* Params = nullptr;
	TMap<uint64, UPCGExRelationalData*> RelationalDatasMap; // UID:RelationalMap
	TArray<FPCGExRelationalPair> RelationalPairs;

	UPCGExRelationalData* GetRelationalData(UPCGPointData* PointData)
	{
		UPCGExRelationalData** Found = RelationalDatasMap.Find(PointData->UID);
		return Found ? *Found : nullptr;
	}
};

USTRUCT()
struct PCGEXTENDEDTOOLKIT_API FPCGExDataMapping
{
	GENERATED_BODY()

	FPCGExDataMapping()
	{
		RelationalDatas.Empty();
		RelationalDatasMap.Empty();
	}

public:
	TArray<UPCGExRelationalData*> RelationalDatas; //All relational data
	TArray<FPCGExParamDataMapping*> RelationalMappings; //Per Param mapping
	TMap<UPCGExRelationalParamsData*, FPCGExParamDataMapping> RelationalDatasMap; // Per Param quick access

	void Add(UPCGExRelationalData* RelationalData)
	{
		RelationalDatas.AddUnique(RelationalData);
		FPCGExParamDataMapping* Mapping = RelationalDatasMap.Find(RelationalData->Params);

		if (!Mapping)
		{
			RelationalDatasMap.Add(RelationalData->Params, FPCGExParamDataMapping{RelationalData->Params});
			Mapping = RelationalDatasMap.Find(RelationalData->Params);
			RelationalMappings.AddUnique(Mapping);
		}

		Mapping->RelationalDatasMap.Add(RelationalData->GetBoundUID(), RelationalData);
	}

	void Add(FPCGExRelationalPair Pair)
	{
		Add(Pair.RelationalData);
	}
};

// Detail stored in a attribute array
class PCGEXTENDEDTOOLKIT_API FPCGExRelationalDataHelpers
{
public:
	//TODO: Helper methods to build maps, create new RelationalData, find existing ones etc
	//TODO: Map RelationalData to UPCGData.UID. Note that each time a RelationalData is transferred, a new SOCKET_UID must be referenced.

	static bool BuildRelationalMapping(TArray<FPCGTaggedData>& RelationalCandidates, TArray<FPCGTaggedData> PointsCandidates, FPCGExDataMapping& OutMapping)
	{
		bool bFoundAny = false;

		for (FPCGTaggedData& PossibleRelationData : RelationalCandidates)
		{
			UPCGExRelationalData* RelationalData = Cast<UPCGExRelationalData>(PossibleRelationData.Data);
			if (!RelationalData) { continue; }
			OutMapping.Add(RelationalData);
			bFoundAny = true;
		}

		return bFoundAny;
	}

	template <typename TBindableRelationalData>
	static bool FindBoundRelations(TArray<FPCGTaggedData>& RelationalCandidates, TMap<uint64, TBindableRelationalData*>& OutMap)
	{
		bool bFoundAny = false;

		for (FPCGTaggedData& PossibleRelationData : RelationalCandidates)
		{
			TBindableRelationalData* RelationalData = Cast<TBindableRelationalData>(PossibleRelationData.Data);
			if (!RelationalData) { continue; }
			OutMap.Add(RelationalData->BoundUID, RelationalData);
			bFoundAny = true;
		}

		return bFoundAny;
	}

	template <typename TPointData, typename TRelationalData>
	static bool FindBoundInputs(FPCGContext* Context, TArray<FPCGTaggedData>& RelationalCandidates, TArray<FPCGTaggedData> PointsCandidates, TMap<uint64, TRelationalData*>& OutMap)
	{
		bool bFoundAny = false;
		FindBoundRelations(RelationalCandidates, OutMap);
		for (FPCGTaggedData& PossiblePointData : PointsCandidates)
		{
			const TPointData* PointData = Cast<TPointData>(PossiblePointData.Data);
			if (!PointData) { continue; }

			TRelationalData* RelationalData = OutMap.Find(PointData.UID);
			if (!RelationalData) { continue; }

			bFoundAny = true;
		}

		return bFoundAny;
	}

	template <typename KEY, typename VALUE>
	static void FindBoundInputs(FPCGContext* Context, FName RelationalsInputLabel, FName PointsInputLabel, TMap<KEY, VALUE> OutMap)
	{
		TArray<FPCGTaggedData> RelationalCandidates = Context->InputData.GetInputsByPin(RelationalsInputLabel);
		TArray<FPCGTaggedData> PointsCandidates = Context->InputData.GetInputsByPin(PointsInputLabel);
	}

	/**
	 * 
	 * @param Context 
	 * @param Params 
	 * @param OutPair 
	 */
	static void CreateRelationalDataOutput(FPCGContext* Context, UPCGExRelationalParamsData* Params, FPCGExRelationalPair& OutPair)
	{
		TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
		OutPair.RelationalData = NewObject<UPCGExRelationalData>();
		OutPair.RelationalData->Initialize(&Params);
		OutPair.RelationalData->SetBoundUID(OutPair.PointData->UID);
		Outputs.Emplace_GetRef().Data = OutPair.RelationalData;
	}

	/**
	 * 
	 * @param Context 
	 * @param Source 
	 * @param OutPair 
	 */
	static void CreateRelationalDataOutput(FPCGContext* Context, FPCGTaggedData& Source, FPCGExRelationalPair& OutPair)
	{
		TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
		UPCGExRelationalData* RelationalData = Cast<UPCGExRelationalData>(Source.Data);
		OutPair.RelationalData = NewObject<UPCGExRelationalData>();
		OutPair.RelationalData->Initialize(&RelationalData);
		OutPair.RelationalData->SetBoundUID(OutPair.PointData->UID);
		Outputs.Emplace_GetRef().Data = OutPair.RelationalData;
	}

	/**
	 * 
	 * @param Context 
	 * @param Params 
	 * @param Source 
	 * @param OutPair 
	 */
	static void CreateRelationalPairOutput(FPCGContext* Context, UPCGExRelationalParamsData* Params, FPCGTaggedData& Source, FPCGExRelationalPair& OutPair)
	{
		TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
		OutPair.SourcePointData = const_cast<UPCGPointData*>(Cast<UPCGSpatialData>(Source.Data)->ToPointData(Context));

		OutPair.PointData = NewObject<UPCGPointData>();
		OutPair.PointData->InitializeFromData(OutPair.SourcePointData);
		Outputs.Add_GetRef(Source).Data = OutPair.PointData;

		CreateRelationalDataOutput(Context, Params, OutPair);
	}

	static bool FindRelationalParams(TArray<FPCGTaggedData>& Sources, TArray<UPCGExRelationalParamsData*>& OutParams)
	{
		OutParams.Reset();
		bool bFoundAny = false;
		for (const FPCGTaggedData& TaggedData : Sources)
		{
			UPCGExRelationalParamsData* Params = Cast<UPCGExRelationalParamsData>(TaggedData.Data);
			if (!Params) { continue; }
			OutParams.Add(Params);
			bFoundAny = true;
		}

		return bFoundAny;
	}

};
