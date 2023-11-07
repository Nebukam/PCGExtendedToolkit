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

struct PCGEXTENDEDTOOLKIT_API FPCGExIndexedPointDataIO : public FPCGExPointDataIO
{

public:
	TMap<uint64, int32> Indices; //MetadaEntry::Index, based on Input points (output MetadataEntry will be offset)

	FPCGExIndexedPointDataIO()
	{
		Indices.Empty();
	}

	/**
	 * Copy In.Points to Out.Points and build the Indices map
	 * @param Context 
	 * @return 
	 */
	bool ForwardPointsIndexed(FPCGContext* Context)
	{
		return ForwardPoints(Context, [this](FPCGPoint& Point, const int32 Index)
		{
			Indices.Add(Point.MetadataEntry, Index);
		});
	}

	/**
	 * Copy In.Points to Out.Points and build the Indices map with a callback after each copy
	 * @param Context 
	 * @param PointFunc 
	 * @return 
	 */
	bool ForwardPointsIndexed(FPCGContext* Context, const TFunction<void(FPCGPoint&, const int32)>& PointFunc)
	{
		return ForwardPoints(Context, [&PointFunc, this](FPCGPoint& Point, const int32 Index)
		{
			Indices.Add(Point.MetadataEntry, Index);
			PointFunc(Point, Index);
		});
	}
};

struct PCGEXTENDEDTOOLKIT_API FPCGExRelationalDataIO
{

public:
	FPCGExIndexedPointDataIO* PointIO = nullptr;
	FPCGTaggedData* Source = nullptr; // Source struct
	UPCGExRelationalData* In = nullptr; // Input PointData
	FPCGTaggedData* Output = nullptr; // Source struct
	UPCGExRelationalData* Out = nullptr; // Output PointData

	/**
	 * 
	 * @param Context 
	 * @param bForwardOnly Only initialize output if there is an existing input
	 * @return 
	 */
	bool InitializeOut(FPCGContext* Context, bool bForwardOnly = true)
	{
		if (bForwardOnly && (!In || !Source)) { return false; }
		Out = NewObject<UPCGExRelationalData>();
		if (In && Source) { Out->Initialize(&In); }
		return true;
	}

	/**
	 * Write valid outputs to Context' tagged data
	 * @param Context 
	 */
	void OutputToContext(FPCGContext* Context)
	{
		if (!Out || Output) { return; }
		if (Source) { Output = &(Context->OutputData.TaggedData.Add_GetRef(*Source)); }
		else { Output = &(Context->OutputData.TaggedData.Emplace_GetRef()); }
		Output->Data = Out;
	}

	void SetPointIO(FPCGExIndexedPointDataIO* InPointIO)
	{
		PointIO = InPointIO;
		if (In && PointIO->In) { Out->SetBoundUID(PointIO->In->UID); }
		if (Out && PointIO->Out) { Out->SetBoundUID(PointIO->Out->UID); }
	}
};

USTRUCT()
struct PCGEXTENDEDTOOLKIT_API FPCGExRelationalIOMap
{
	GENERATED_BODY()

	FPCGExRelationalIOMap()
	{
		Pairs.Empty();
		UIDMap.Empty();
	}

	FPCGExRelationalIOMap(UPCGExRelationalParamsData* InParams): FPCGExRelationalIOMap()
	{
		Params = InParams;
	}

	FPCGExRelationalIOMap(FPCGContext* Context, UPCGExRelationalParamsData* InParams, TArray<FPCGTaggedData>& Sources, bool bInitializeOutput = false): FPCGExRelationalIOMap(InParams)
	{
		Initialize(Context, Sources, bInitializeOutput);
	}

	FPCGExRelationalIOMap(FPCGContext* Context, UPCGExRelationalParamsData* InParams, FPCGExPointIOMap<FPCGExIndexedPointDataIO>& PointIOMap): FPCGExRelationalIOMap(InParams)
	{
		Initialize(Context, PointIOMap);
	}

	FPCGExRelationalIOMap(FPCGContext* Context, UPCGExRelationalParamsData* InParams, TArray<FPCGTaggedData>& Sources, FPCGExPointIOMap<FPCGExIndexedPointDataIO>& PointIOMap, bool bInitializeOutput = false): FPCGExRelationalIOMap(InParams)
	{
		Initialize(Context, Sources, PointIOMap, bInitializeOutput);
	}

public:
	UPCGExRelationalParamsData* Params = nullptr;
	TArray<FPCGExRelationalDataIO> Pairs;
	TMap<uint64, int32> UIDMap;

	/**
	 * Initialize from Sources
	 * Useful for elements that aim at only modifying Relational data 
	 * @param Context 
	 * @param Sources RelationalData Sources
	 * @param bInitializeOutput 
	 */
	void Initialize(FPCGContext* Context, TArray<FPCGTaggedData>& Sources, bool bInitializeOutput = false)
	{
		Pairs.Empty(Sources.Num());
		for (FPCGTaggedData& Source : Sources)
		{
			UPCGExRelationalData* RelationalData = Cast<UPCGExRelationalData>(Source.Data);

			if (!RelationalData) { continue; } // Unsupported type
			if (RelationalData->Params != Params) { continue; } // Wrong params

			FPCGExRelationalDataIO& IOPair = Pairs.Emplace_GetRef();

			IOPair.Source = &Source;
			IOPair.In = RelationalData;

			if (bInitializeOutput) { IOPair.InitializeOut(Context, true); }
		}
		UpdateMap();
	}

	/**
	* Initialize from IOMap
	 * Useful for elements that create new relational data
	 * @param Context 
	 * @param PointIOMap Prepared PointIOMap
	 */
	void Initialize(FPCGContext* Context, FPCGExPointIOMap<FPCGExIndexedPointDataIO>& PointIOMap)
	{
		int32 NumPairs = PointIOMap.Pairs.Num();
		Pairs.Empty(NumPairs);

		PointIOMap.UpdateMap(); // Make sure map is up-to-date

		for (int i = 0; i < NumPairs; i++)
		{
			FPCGExRelationalDataIO& RIOPair = Pairs.Emplace_GetRef();
			RIOPair.InitializeOut(Context, false);
			RIOPair.SetPointIO(&PointIOMap.Pairs[i]);
		}
		UpdateMap();
	}

	/**
	* Cross-initialize from Source & IOMap
	 * Useful for elements that do something from relational data.
	 * @param Context 
	 * @param Sources RelationalData Sources
	 * @param PointIOMap Prepared PointIOMap 
	 * @param bInitializeOutput 
	 */
	void Initialize(FPCGContext* Context, TArray<FPCGTaggedData>& Sources, FPCGExPointIOMap<FPCGExIndexedPointDataIO>& PointIOMap, bool bInitializeOutput = false)
	{
		Pairs.Empty(Sources.Num());
		PointIOMap.UpdateMap(); // Make sure map is up-to-date

		for (FPCGTaggedData& Source : Sources)
		{
			UPCGExRelationalData* RelationalData = Cast<UPCGExRelationalData>(Source.Data);
			if (!RelationalData) { continue; } // Unsupported types
			if (RelationalData->Params != Params) { continue; } // Wrong params

			FPCGExIndexedPointDataIO* PointDataIO = PointIOMap.Find(RelationalData->GetBoundUID());
			if (!PointDataIO)
			{
				// Bound Points are missing from the map :(
				continue;
			}

			FPCGExRelationalDataIO& RIOPair = Pairs.Emplace_GetRef();
			RIOPair.Source = &Source;
			RIOPair.In = RelationalData;

			if (bInitializeOutput) { RIOPair.InitializeOut(Context, true); }

			RIOPair.SetPointIO(PointDataIO);
		}
		UpdateMap();
	}

	/**
	 * Write valid outputs to Context' tagged data
	 * @param Context 
	 */
	void OutputToContext(FPCGContext* Context)
	{
		for (FPCGExRelationalDataIO& RIOPair : Pairs) { RIOPair.OutputToContext(Context); }
	}

	void UpdateMap()
	{
		UIDMap.Empty();
		for (int i = 0; i < Pairs.Num(); i++) { MapIOPair(Pairs[i], i); }
	}

	FPCGExRelationalDataIO* Find(uint64 UID)
	{
		if (const int32* Found = UIDMap.Find(UID)) { return &Pairs[*Found]; }
		return nullptr;
	}

	void ForEachPair(
		FPCGContext* Context,
		const TFunction<bool(FPCGExRelationalDataIO*, const int32)>& BodyLoop)
	{
		for (int i = 0; i < Pairs.Num(); i++)
		{
			FPCGExRelationalDataIO* RIOPair = &Pairs[i];
			BodyLoop(RIOPair, i);
			MapIOPair(*RIOPair, i);
		}
	}

protected:
	void MapIOPair(const FPCGExRelationalDataIO& RIOPair, int32 Index)
	{
		if (RIOPair.In) { UIDMap.Add(RIOPair.In->UID, Index); }
		if (RIOPair.Out) { UIDMap.Add(RIOPair.Out->UID, Index); }
	}
};

USTRUCT()
struct PCGEXTENDEDTOOLKIT_API FPCGExPerParamsMappings
{
	GENERATED_BODY()

	FPCGExPerParamsMappings()
	{
		Maps.Empty();
		UIDMap.Empty();
		// This struct contains a list of all FPCGExRelationalIOMap based on ->Params
	}

public:
	UPCGExRelationalParamsData* Params = nullptr;
	TArray<FPCGExRelationalIOMap> Maps;
	TMap<uint64, int32> UIDMap;

};

////////

USTRUCT()
struct PCGEXTENDEDTOOLKIT_API FPCGExRelationalPair
{
	GENERATED_BODY()

public:
	FPCGExIndexedPointDataIO* IO = nullptr;
	UPCGExRelationalData* RelationalData = nullptr;
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
	TArray<FPCGExIndexedPointDataIO>* IOPair = nullptr;
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

USTRUCT()
struct PCGEXTENDEDTOOLKIT_API FPCGExProcessingData
{
	GENERATED_BODY()

	FPCGExProcessingData()
	{
		Indices.Empty();
		Modifiers.Empty();
		Candidates.Empty();
	}

public:
	UPCGExRelationalParamsData* Params = nullptr;
	FPCGExRelationalPair* RelationalPair = nullptr;
	UPCGPointData::PointOctree* Octree = nullptr;
	bool bUseModifiers = false;

protected:
	TMap<int64, int32> Indices;
	TArray<FPCGExSamplingModifier> Modifiers;
	TArray<FPCGExRelationCandidate> Candidates;

public:
	int32 CurrentIndex = -1;
	TMap<int64, int32>& GetIndices() { return Indices; }
	int64 GetIndex(int64 Key) { return *(Indices.Find(Key)); }
	TArray<FPCGExSamplingModifier>& GetModifiers() { return Modifiers; }
	TArray<FPCGExRelationCandidate>& GetCandidates() { return Candidates; }
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
		OutPair.RelationalData->SetBoundUID(OutPair.IO->Out->UID);
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
		OutPair.RelationalData->SetBoundUID(OutPair.IO->Out->UID);
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
		OutPair.IO->In = const_cast<UPCGPointData*>(Cast<UPCGSpatialData>(Source.Data)->ToPointData(Context));

		OutPair.IO->Out = NewObject<UPCGPointData>();
		OutPair.IO->Out->InitializeFromData(OutPair.InPoints);
		Outputs.Add_GetRef(Source).Data = OutPair.OutPoints;

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

	/**
	 * 
	 * @param Point 
	 * @param Data 
	 * @return 
	 */
	static double PrepareCandidatesForPoint(const FPCGPoint& Point, FPCGExProcessingData& Data)
	{
		TArray<FPCGExRelationCandidate>& Candidates = Data.GetCandidates();
		TArray<FPCGExRelationDefinition>& Slots = Data.Params->RelationSlots;
		Candidates.Reset();

		if (Data.Params->bHasVariableMaxDistance && Data.bUseModifiers)
		{
			double GreatestMaxDistance = Data.Params->GreatestStaticMaxDistance;

			for (int i = 0; i < Slots.Num(); i++)
			{
				FPCGExRelationDefinition& Slot = Slots[i];
				FPCGExSamplingModifier* Modifier = &(Data.GetModifiers()[i]);
				FPCGExRelationCandidate Candidate = FPCGExRelationCandidate(Point, Slot);

				double Scale = 1.0;

				auto GetAttributeScaleFactor = [&Modifier, &Point](auto DummyValue) -> double
				{
					using T = decltype(DummyValue);
					FPCGMetadataAttribute<T>* Attribute = FPCGExCommon::GetTypedAttribute<T>(Modifier);
					return GetScaleFactor(Attribute->GetValue(Point.MetadataEntry));
				};

				if (Modifier->bFixed)
				{
					switch (Modifier->Selector.GetSelection())
					{
					case EPCGAttributePropertySelection::Attribute:
						Scale = PCGMetadataAttribute::CallbackWithRightType(Modifier->Attribute->GetTypeId(), GetAttributeScaleFactor);
						break;
#define PCGEX_SCALE_BY_ACCESSOR(_ENUM, _ACCESSOR) case _ENUM: Scale=GetScaleFactor(Point._ACCESSOR); break;
					case EPCGAttributePropertySelection::PointProperty:
						switch (Modifier->Selector.GetPointProperty())
						{
						PCGEX_FOREACH_POINTPROPERTY(PCGEX_SCALE_BY_ACCESSOR)
						default: ;
						}
						break;
					case EPCGAttributePropertySelection::ExtraProperty:
						switch (Modifier->Selector.GetExtraProperty())
						{
						PCGEX_FOREACH_POINTEXTRAPROPERTY(PCGEX_SCALE_BY_ACCESSOR)
						default: ;
						}
						break;
					default: ;
					}
#undef PCGEX_SCALE_BY_ACCESSOR
				}

				Candidate.DistanceScale = Scale;
				GreatestMaxDistance = FMath::Max(GreatestMaxDistance, Candidate.GetScaledDistance());
				Candidates.Add(Candidate);
			}

			return GreatestMaxDistance;
		}
		else
		{
			for (const FPCGExRelationDefinition& Slot : Slots)
			{
				FPCGExRelationCandidate Candidate = FPCGExRelationCandidate(Point, Slot);
				Candidates.Add(Candidate);
			}

			return Data.Params->GreatestStaticMaxDistance;
		}
	}

	template <typename T, typename dummy = void>
	static double GetScaleFactor(const T& Value) { return static_cast<double>(Value); }

	template <typename dummy = void>
	static double GetScaleFactor(const FVector2D& Value) { return Value.Length(); }

	template <typename dummy = void>
	static double GetScaleFactor(const FVector& Value) { return Value.Length(); }

	template <typename dummy = void>
	static double GetScaleFactor(const FVector4& Value) { return FVector(Value).Length(); }

	template <typename dummy = void>
	static double GetScaleFactor(const FRotator& Value) { return 1.0; }

	template <typename dummy = void>
	static double GetScaleFactor(const FQuat& Value) { return 1.0; }

	template <typename dummy = void>
	static double GetScaleFactor(const FName& Value) { return 1.0; }

	template <typename dummy = void>
	static double GetScaleFactor(const FString& Value) { return 1.0; }

	template <typename dummy = void>
	static double GetScaleFactor(const FTransform& Value) { return 1.0; }
};
