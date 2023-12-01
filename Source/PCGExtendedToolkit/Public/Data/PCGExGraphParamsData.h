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

	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Param; }

	/**
	 * 
	 * @param PointData 
	 * @return 
	 */
	bool HasMatchingGraphData(const UPCGPointData* PointData);

public:
	UPROPERTY(BlueprintReadOnly)
	FName GraphIdentifier = "GraphIdentifier";

	UPROPERTY(BlueprintReadOnly)
	double GreatestStaticMaxDistance = 0.0;

	UPROPERTY(BlueprintReadOnly)
	bool bHasVariableMaxDistance = false;

	FName CachedIndexAttributeName;

protected:
	PCGExGraph::FSocketMapping SocketMapping;

public:
	const PCGExGraph::FSocketMapping* GetSocketMapping() const { return &SocketMapping; }

	/**
	 * Initialize this data object from a list of socket descriptors
	 * @param InSockets
	 * @param bApplyOverrides
	 * @param Overrides 
	 */
	virtual void Initialize(
		TArray<FPCGExSocketDescriptor>& InSockets,
		const bool bApplyOverrides,
		FPCGExSocketGlobalOverrides& Overrides);

	/**
	 * Prepare socket mapping for working with a given PointData object.
	 * @param Context
	 * @param PointData
	 * @param bEnsureEdgeType 
	 */
	void PrepareForPointData(const UPCGPointData* PointData, const bool bEnsureEdgeType);

	/**
		 * Fills an array in order with each' socket metadata registered for a given point.
		 * Make sure to call PrepareForPointData first.
		 * @param MetadataEntry 
		 * @param OutMetadata 
		 */
	void GetSocketsData(const PCGMetadataEntryKey MetadataEntry, TArray<PCGExGraph::FSocketMetadata>& OutMetadata) const;
	
	/**
	 * 
	 * @param InIndex 
	 * @param MetadataEntry 
	 * @param OutEdges 
	 */
	template <typename T>
	void GetEdges(const int32 InIndex, const PCGMetadataEntryKey MetadataEntry, TArray<T>& OutEdges) const
	{
		for (const PCGExGraph::FSocket& Socket : SocketMapping.Sockets)
		{
			T Edge;
			if (Socket.TryGetEdge(InIndex, MetadataEntry, Edge)) { OutEdges.AddUnique(Edge); }
		}
	}
	
	/**
	 * 
	 * @param InIndex 
	 * @param MetadataEntry 
	 * @param OutEdges
	 * @param EdgeFilter 
	 */
	template <typename T>
	void GetEdges(const int32 InIndex, const PCGMetadataEntryKey MetadataEntry, TArray<T>& OutEdges, const EPCGExEdgeType& EdgeFilter) const
	{
		for (const PCGExGraph::FSocket& Socket : SocketMapping.Sockets)
		{
			T Edge;
			if (Socket.TryGetEdge(InIndex, MetadataEntry, Edge, EdgeFilter)) { OutEdges.AddUnique(Edge); }
		}
	}

	/**
	 * Make sure InMetadata has the same length as the nu
	 * @param MetadataEntry 
	 * @param InMetadata 
	 */
	void SetSocketsData(const PCGMetadataEntryKey MetadataEntry, TArray<PCGExGraph::FSocketMetadata>& InMetadata);

	void GetSocketsInfos(TArray<PCGExGraph::FSocketInfos>& OutInfos);
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
				if (UniqueParams.Contains(GraphData->UID)) { continue; }
				UniqueParams.Add(GraphData->UID);
				Params.Add(const_cast<UPCGExGraphParamsData*>(GraphData));
				ParamsSources.Add(Source);
			}
			UniqueParams.Empty();
		}

		void ForEach(FPCGContext* Context, const TFunction<void(UPCGExGraphParamsData*, const int32)>& BodyLoop)
		{
			for (int i = 0; i < Params.Num(); i++)
			{
				UPCGExGraphParamsData* ParamsData = Params[i];
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
