// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Math/UnrealMathUtility.h"

#include "PCGExAttributeHelpers.h"
#include "Graph/PCGExGraph.h"

#include "PCGExGraphParamsData.generated.h"

class UPCGPointData;


/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExGraphParamsData : public UPCGPointData
{
	GENERATED_BODY()

public:
	UPCGExGraphParamsData(const FObjectInitializer& ObjectInitializer);

	TArray<FPCGExSocketDescriptor> SocketsDescriptors;
	bool bApplyGlobalOverrides = false;
	FPCGExSocketGlobalOverrides GlobalOverrides;

	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Param; }

	/**
	 * 
	 * @param PointData 
	 * @return 
	 */
	bool HasMatchingGraphData(const UPCGPointData* PointData) const;

	FName GraphIdentifier = "GraphIdentifier";
	double GreatestStaticMaxDistance = 0.0;
	bool bHasVariableMaxDistance = false;
	FName CachedIndexAttributeName;
	uint64 GraphUID = 0;

	virtual void BeginDestroy() override;

protected:
	PCGExGraph::FSocketMapping* SocketMapping = nullptr;

public:
	const PCGExGraph::FSocketMapping* GetSocketMapping() const { return SocketMapping; }

	/**
	 * Initialize this data object from a list of socket descriptors
	 * @param InSockets
	 * @param bApplyOverrides
	 * @param Overrides 
	 */
	virtual void Initialize();

	/**
	 * Prepare socket mapping for working with a given PointData object.
	 * @param Context
	 * @param PointIO
	 * @param bEnsureEdgeType 
	 */
	void PrepareForPointData(const PCGExData::FPointIO& PointIO, const bool bEnsureEdgeType) const;

	/**
		 * Fills an array in order with each' socket metadata registered for a given point.
		 * Make sure to call PrepareForPointData first.
		 * @param PointIndex 
		 * @param OutMetadata 
		 */
	void GetSocketsData(const int32 PointIndex, TArray<PCGExGraph::FSocketMetadata>& OutMetadata) const;

	/**
	 * 
	 * @param InIndex 
	 * @param MetadataEntry 
	 * @param OutEdges 
	 */
	template <typename T>
	void GetEdges(const int32 InIndex, const PCGMetadataEntryKey MetadataEntry, TArray<T>& OutEdges) const
	{
		for (const PCGExGraph::FSocket& Socket : SocketMapping->Sockets)
		{
			T Edge;
			if (Socket.TryGetEdge(MetadataEntry, Edge)) { OutEdges.AddUnique(Edge); }
		}
	}

	/**
	 * 
	 * @param PointIndex 
	 * @param OutEdges
	 * @param EdgeFilter 
	 */
	template <typename T>
	void GetEdges(const int32 PointIndex, TArray<T>& OutEdges, const EPCGExEdgeType& EdgeFilter) const
	{
		for (const PCGExGraph::FSocket& Socket : SocketMapping->Sockets)
		{
			T Edge;
			if (Socket.TryGetEdge(PointIndex, Edge, EdgeFilter)) { OutEdges.AddUnique(Edge); }
		}
	}

	/**
	 * Make sure InMetadata has the same length as the nu
	 * @param PointIndex 
	 * @param InMetadata 
	 */
	void SetSocketsData(const int32 PointIndex, TArray<PCGExGraph::FSocketMetadata>& InMetadata) const;

	void GetSocketsInfos(TArray<PCGExGraph::FSocketInfos>& OutInfos) const;

	void Cleanup();
};

namespace PCGExGraph
{
	struct PCGEXTENDEDTOOLKIT_API FGraphInputs
	{
		FGraphInputs()
		{
			Params.Empty();
			ParamsSources.Empty();
		}

		FGraphInputs(FPCGContext* Context, FName InputLabel): FGraphInputs()
		{
			TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(InputLabel);
			Initialize(Context, Sources);
		}

		FGraphInputs(FPCGContext* Context, TArray<FPCGTaggedData>& Sources): FGraphInputs()
		{
			Initialize(Context, Sources);
		}

	public:
		TArray<UPCGExGraphParamsData*> Params;
		TArray<FPCGTaggedData> ParamsSources;

		/**
		 * Initialize from Sources
		 * @param Context 
		 * @param Sources 
		 * @param bInitializeOutput 
		 */
		void Initialize(FPCGContext* Context, TArray<FPCGTaggedData>& Sources, bool bInitializeOutput = false)
		{
			Params.Empty(Sources.Num());
			TSet<uint64> UniqueParams;
			for (FPCGTaggedData& Source : Sources)
			{
				const UPCGExGraphParamsData* GraphData = Cast<UPCGExGraphParamsData>(Source.Data);
				if (!GraphData) { continue; }
				if (UniqueParams.Contains(GraphData->GraphUID)) { continue; }
				UniqueParams.Add(GraphData->GraphUID);
				Params.Add(const_cast<UPCGExGraphParamsData*>(GraphData));
				//Params.Add(CopyGraph(GraphData));
				ParamsSources.Add(Source);
			}
			UniqueParams.Empty();
		}

		static UPCGExGraphParamsData* CopyGraph(const UPCGExGraphParamsData* InGraph)
		{
			return NewGraph(
				InGraph->GraphUID,
				InGraph->GraphIdentifier,
				InGraph->SocketsDescriptors,
				InGraph->bApplyGlobalOverrides,
				InGraph->GlobalOverrides);
		}

		static UPCGExGraphParamsData* NewGraph(
			uint64 GraphUID,
			FName Identifier,
			TArray<FPCGExSocketDescriptor> Sockets,
			bool ApplyGlobalOverrides,
			FPCGExSocketGlobalOverrides GlobalOverrides)
		{
			UPCGExGraphParamsData* OutParams = NewObject<UPCGExGraphParamsData>();

			OutParams->GraphUID = GraphUID;
			OutParams->GraphIdentifier = Identifier;

			OutParams->SocketsDescriptors.Append(Sockets);
			OutParams->bApplyGlobalOverrides = ApplyGlobalOverrides;
			OutParams->GlobalOverrides = GlobalOverrides;
			OutParams->Initialize();

			return OutParams;
		}

		void ForEach(FPCGContext* Context, const TFunction<void(UPCGExGraphParamsData*, const int32)>& BodyLoop)
		{
			for (int i = 0; i < Params.Num(); i++)
			{
				UPCGExGraphParamsData* ParamsData = Params[i]; //TODO : Create "working copies" so we don't start working with destroyed sockets, as params lie early in the graph
				BodyLoop(ParamsData, i);
			}
		}

		void OutputTo(FPCGContext* Context) const
		{
			for (int i = 0; i < ParamsSources.Num(); i++)
			{
				FPCGTaggedData& OutputRef = Context->OutputData.TaggedData.Add_GetRef(ParamsSources[i]);
				OutputRef.Pin = OutputParamsLabel;
				OutputRef.Data = Params[i];
			}
		}

		bool IsEmpty() const { return Params.IsEmpty(); }

		~FGraphInputs()
		{
			Params.Empty();
			ParamsSources.Empty();
		}
	};
}
