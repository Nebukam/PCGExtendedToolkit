// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Math/UnrealMathUtility.h"

#include "PCGExAttributeHelpers.h"
#include "PCGParamData.h"
#include "Graph/PCGExGraph.h"

#include "PCGExGraphParamsData.generated.h"

class UPCGPointData;

struct FPCGExPointsProcessorContext;
ENUM_CLASS_FLAGS(EPCGExEdgeType)

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true"))
enum class EPCGExTangentType : uint8
{
	Custom      = 0 UMETA(DisplayName = "Custom", Tooltip="Custom Attributes."),
	Extrapolate = 0 UMETA(DisplayName = "Extrapolate", Tooltip="Extrapolate from neighbors position and direction"),
};

ENUM_CLASS_FLAGS(EPCGExTangentType)

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true"))
enum class EPCGExSocketType : uint8
{
	None   = 0 UMETA(DisplayName = "None", Tooltip="This socket has no particular type."),
	Output = 1 << 0 UMETA(DisplayName = "Output", Tooltip="This socket in an output socket. It can only connect to Input sockets."),
	Input  = 1 << 1 UMETA(DisplayName = "Input", Tooltip="This socket in an input socket. It can only connect to Output sockets"),
	Any    = Output | Input UMETA(DisplayName = "Any", Tooltip="This socket is considered both an Output and and Input."),
};

ENUM_CLASS_FLAGS(EPCGExSocketType)

#pragma region Descriptors

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSocketDescriptor
{
	GENERATED_BODY()

	FPCGExSocketDescriptor()
	{
	}

	explicit FPCGExSocketDescriptor(const FName InName):
		SocketName(InName)
	{
	}

	FPCGExSocketDescriptor(
		const FName InName,
		const FVector& InDirection,
		const EPCGExSocketType InType,
		const FColor InDebugColor,
		const double InAngle = 45):
		SocketName(InName),
		SocketType(InType),
		DebugColor(InDebugColor)
	{
		Direction = InDirection;
		Angle = InAngle;
	}

	FPCGExSocketDescriptor(
		const FName InName,
		const FVector& InDirection,
		const FName InMatchingSlot,
		const EPCGExSocketType InType,
		const FColor InDebugColor,
		const double InAngle = 45):
		SocketName(InName),
		SocketType(InType),
		DebugColor(InDebugColor)
	{
		Direction = InDirection;
		Angle = InAngle;
		MatchingSlots.Add(InMatchingSlot);
	}

public:
	/** Name of the attribute to write neighbor index to. */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta=(PCG_Overridable))
	FName SocketName = TEXT("SocketName");

	/** Type of socket. */
	//UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta=(HideInDetailPanel)) // TODO: Implement
	EPCGExSocketType SocketType = EPCGExSocketType::Any;

	/** Exclusive sockets can only connect to other socket matching */
	//UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta=(HideInDetailPanel)) // TODO: Implement
	bool bExclusiveBehavior = false;

	/** Whether the orientation of the direction is relative to the point transform or not. */
	UPROPERTY(BlueprintReadWrite, Category = "Settings|Probing", EditAnywhere, meta=(PCG_Overridable))
	bool bRelativeOrientation = true;

	/// Bounds

	/** Slot 'look-at' direction. Used along with DotTolerance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Probing", meta=(PCG_Overridable))
	FVector Direction = FVector::UpVector;

	/** If true, the direction vector of the socket will be read from a local attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Probing", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalDirection = false;

	/** Local property or attribute to read Direction from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Probing", meta = (PCG_Overridable, EditCondition="bUseLocalDirection"))
	FPCGExInputDescriptor LocalDirection;

	//

	/** Angular threshold. Used along with the direction of the slot when looking for the closest candidate. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Probing", meta=(PCG_Overridable, Units="Degrees", ClampMin=0, ClampMax=180))
	double Angle = 45.0; // 0.707f dot
	double DotThreshold = 0.707f;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Probing", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalAngle = false;

	/** Local property or attribute to read Angle from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Probing", meta = (PCG_Overridable, EditCondition="bUseLocalAngle"))
	FPCGExInputDescriptor LocalAngle;

	/** Enable if the local angle should be read as degrees instead of radians. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Probing", meta = (PCG_Overridable, EditCondition="bUseLocalAngle"))
	bool bLocalAngleIsDegrees = true;


	//

	/** Maximum search radius. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Probing", meta=(PCG_Overridable, ClampMin=0.0001))
	double Radius = 1000.0f;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Probing", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalRadius = false;

	/** Local property or attribute to read Radius from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Probing", meta = (PCG_Overridable, EditCondition="bUseLocalRadius"))
	FPCGExInputDescriptor LocalRadius;

	/** Offset socket origin  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Probing", meta = (PCG_Overridable))
	EPCGExDistance ProbeOrigin = EPCGExDistance::Center;

	/** The balance over distance to prioritize closer distance or better alignment. Curve X is normalized distance; Y = 0 means narrower dot wins, Y = 1 means closer distance wins */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Probing")
	TSoftObjectPtr<UCurveFloat> DotOverDistance = TSoftObjectPtr<UCurveFloat>(PCGEx::DefaultDotOverDistanceCurve);

	TObjectPtr<UCurveFloat> DotOverDistanceCurve = nullptr;

	void LoadCurve()
	{
		if (DotOverDistance.IsNull()) { DotOverDistanceCurve = TSoftObjectPtr<UCurveFloat>(PCGEx::DefaultDotOverDistanceCurve).LoadSynchronous(); }
		else { DotOverDistanceCurve = DotOverDistance.LoadSynchronous(); }
	}

	/// Relationships

	/** Sibling slots names that are to be considered as a match. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Relationships")
	TArray<FName> MatchingSlots;

	/** QoL. Inject this slot as a match to slots referenced in the Matching Slots list. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Relationships")
	bool bMirrorMatchingSockets = true;

	/// Advanced

	/** Enable/disable this socket. Disabled sockets are omitted during processing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, AdvancedDisplay)
	bool bEnabled = true;

	/** Debug color for arrows. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, AdvancedDisplay)
	FColor DebugColor = FColor::Red;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSocketGlobalOverrides
{
	GENERATED_BODY()

	FPCGExSocketGlobalOverrides()
	{
	}

public:
	/** Enables override */
	bool bEnabled = false;

	UPROPERTY(BlueprintReadWrite, Category = "Probing", EditAnywhere)
	bool bRelativeOrientation = false;

	/// Bounds

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probing", meta=(PCG_Overridable))
	bool bDirection = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probing", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalDirection = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probing", meta = (PCG_Overridable, EditCondition="bUseLocalDirection"))
	bool bLocalDirection = false;

	//

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probing", meta=(PCG_Overridable))
	bool bAngle = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probing", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalAngle = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probing", meta = (PCG_Overridable, EditCondition="bUseLocalAngle"))
	bool bLocalAngle = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probing", meta = (PCG_Overridable, EditCondition="bUseLocalAngle"))
	bool bLocalAngleIsDegrees = false;

	//

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probing", meta=(PCG_Overridable))
	bool bRadius = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probing", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalRadius = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probing", meta = (PCG_Overridable, EditCondition="bUseLocalRadius"))
	bool bLocalRadius = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Probing")
	bool bDotOverDistance = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probing", meta = (PCG_Overridable))
	bool bOffsetOrigin = false;

	/// Relationships

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Relationships")
	bool bMirrorMatchingSockets = false;
};

#pragma endregion

namespace PCGExGraph
{
#pragma region Sockets

	struct PCGEXTENDEDTOOLKIT_API FSocketMetadata
	{
		FSocketMetadata()
		{
		}

		FSocketMetadata(const int32 InIndex, const EPCGExEdgeType InEdgeType):
			Index(InIndex),
			EdgeType(InEdgeType)
		{
		}

	public:
		int32 Index = -1;
		EPCGExEdgeType EdgeType = EPCGExEdgeType::Unknown;

		bool operator==(const FSocketMetadata& Other) const
		{
			return Index == Other.Index && EdgeType == Other.EdgeType;
		}
	};

	const FName SocketPropertyNameIndex = FName("Target");
	const FName SocketPropertyNameEdgeType = FName("EdgeType");

	struct PCGEXTENDEDTOOLKIT_API FSocket
	{
		friend struct FSocketMapping;

		FSocket()
		{
			MatchingSockets.Empty();
		}

		explicit FSocket(const FPCGExSocketDescriptor& InDescriptor): FSocket()
		{
			Descriptor = InDescriptor;
			Descriptor.DotThreshold = FMath::Cos(Descriptor.Angle * (PI / 180.0)); //Degrees to dot product
		}

		~FSocket();

		FPCGExSocketDescriptor Descriptor;

		int32 SocketIndex = -1;
		TSet<int32> MatchingSockets;

	protected:
		bool bReadOnly = false;

		PCGEx::FLocalVectorGetter* LocalDirectionGetter = nullptr;
		PCGEx::FLocalSingleFieldGetter* LocalAngleGetter = nullptr;
		PCGEx::FLocalSingleFieldGetter* LocalRadiusGetter = nullptr;

		PCGEx::TFAttributeWriter<int32>* TargetIndexWriter = nullptr;
		PCGEx::TFAttributeWriter<int32>* EdgeTypeWriter = nullptr;
		PCGEx::TFAttributeReader<int32>* TargetIndexReader = nullptr;
		PCGEx::TFAttributeReader<int32>* EdgeTypeReader = nullptr;
		FName AttributeNameBase = NAME_None;

		void Cleanup();

	public:
		FName GetName() const { return AttributeNameBase; }
		EPCGExSocketType GetSocketType() const { return Descriptor.SocketType; }
		//bool IsExclusive() const { return Descriptor.bExclusiveBehavior; }
		bool Matches(const FSocket* OtherSocket) const { return MatchingSockets.Contains(OtherSocket->SocketIndex); }
		void DeleteFrom(const UPCGPointData* PointData) const;
		void Write(bool bDoCleanup = true);

		void PrepareForPointData(const PCGExData::FPointIO& PointIO, const bool ReadOnly = true);

		void GetDirection(const int32 PointIndex, FVector& OutDirection) const
		{
			OutDirection = LocalDirectionGetter ? LocalDirectionGetter->SafeGet(PointIndex, Descriptor.Direction) : Descriptor.Direction;
		}

		void GetDotThreshold(const int32 PointIndex, double& OutAngle) const
		{
			OutAngle = LocalAngleGetter ? LocalAngleGetter->SafeGet(PointIndex, Descriptor.DotThreshold) : Descriptor.DotThreshold;
		}

		void GetRadius(const int32 PointIndex, double& OutRadius) const
		{
			OutRadius = LocalRadiusGetter ? LocalRadiusGetter->SafeGet(PointIndex, Descriptor.Radius) : Descriptor.Radius;
		}

		void SetTargetIndex(const int32 PointIndex, int32 InValue) const;
		int32 GetTargetIndex(const int32 PointIndex) const;

		void SetEdgeType(const int32 PointIndex, EPCGExEdgeType InEdgeType) const;
		EPCGExEdgeType GetEdgeType(const int32 PointIndex) const;

		FSocketMetadata GetData(const int32 PointIndex) const;

		template <typename T>
		bool TryGetEdge(const int32 PointIndex, T& OutEdge) const
		{
			const int64 End = GetTargetIndex(PointIndex);
			if (End == -1) { return false; }
			OutEdge = T(PointIndex, End, GetEdgeType(PointIndex));
			return true;
		}

		template <typename T>
		bool TryGetEdge(const int32 PointIndex, T& OutEdge, const EPCGExEdgeType& EdgeFilter) const
		{
			const int32 End = GetTargetIndex(PointIndex);
			if (End == -1) { return false; }

			EPCGExEdgeType EdgeType = GetEdgeType(PointIndex);
			if (static_cast<uint8>((EdgeType & EdgeFilter)) == 0) { return false; }

			OutEdge = T(PointIndex, End, EdgeType);
			return true;
		}

		FName GetSocketPropertyName(FName PropertyName) const;

		PCGEx::TFAttributeWriter<int32>& GetTargetIndexWriter() const { return *TargetIndexWriter; }
		PCGEx::TFAttributeWriter<int32>& GetEdgeTypeWriter() const { return *EdgeTypeWriter; }
		PCGEx::TFAttributeReader<int32>& GetTargetIndexReader() const { return *TargetIndexReader; }
		PCGEx::TFAttributeReader<int32>& GetEdgeTypeReader() const { return *EdgeTypeReader; }
	};

	struct PCGEXTENDEDTOOLKIT_API FSocketInfos
	{
		FSocket* Socket = nullptr;

		bool Matches(const FSocketInfos& Other) const { return Socket->Matches(Other.Socket); }

		explicit FSocketInfos(FSocket* InSocket):
			Socket(InSocket)
		{
		}

		~FSocketInfos()
		{
			Socket = nullptr;
		}
	};

	const FName ParamPropertyNameIndex = FName("EdgeIndex");

	struct PCGEXTENDEDTOOLKIT_API FSocketMapping
	{
		FSocketMapping()
		{
			Reset();
		}

		~FSocketMapping()
		{
			Reset();
		}

	public:
		FName Identifier = NAME_None;
		TArray<FSocket> Sockets;
		TMap<FName, int32> NameToIndexMap;
		TMap<int32, int32> IndexRemap;
		int32 NumSockets = 0;

		void Initialize(
			const FName InIdentifier,
			TArray<FPCGExSocketDescriptor>& InSockets,
			const FPCGExSocketGlobalOverrides& Overrides,
			const FPCGExSocketDescriptor& OverrideSocket);

		FName GetCompoundName(FName SecondaryIdentifier) const;

		/**
		 * Prepare socket mapping for working with a given PointData object.
		 * Each socket will cache Attribute & accessors
		 * @param PointIO
		 * @param bReadOnly 
		 */
		void PrepareForPointData(const PCGExData::FPointIO& PointIO, const bool bReadOnly = true);

		const TArray<FSocket>& GetSockets() const { return Sockets; }

		void GetSocketsInfos(TArray<FSocketInfos>& OutInfos);
		void Cleanup();
		void Reset();

		FName GetParamPropertyName(const FName PropertyName) const;

	private:
		/**
		 * Build matching set
		 */
		void PostProcessSockets();
	};

#pragma endregion

#pragma region Edges

	/**
	 * Assume the edge already is neither None nor Unique, since another socket has been found.
	 * @param StartSocket 
	 * @param EndSocket 
	 * @return 
	 */
	static EPCGExEdgeType GetEdgeType(const FSocketInfos& StartSocket, const FSocketInfos& EndSocket)
	{
		if (StartSocket.Matches(EndSocket))
		{
			if (EndSocket.Matches(StartSocket))
			{
				return EPCGExEdgeType::Complete;
			}
			return EPCGExEdgeType::Match;
		}
		if (StartSocket.Socket->SocketIndex == EndSocket.Socket->SocketIndex)
		{
			// We check for mirror AFTER checking for shared/match, since Mirror can be considered a legal match by design
			// in which case we don't want to flag this as Mirrored.
			return EPCGExEdgeType::Mirror;
		}
		return EPCGExEdgeType::Shared;
	}

	static void ComputeEdgeType(const TArray<FSocketInfos>& SocketInfos, const int32 PointIndex)
	{
		for (const FSocketInfos& CurrentSocketInfos : SocketInfos)
		{
			EPCGExEdgeType Type = EPCGExEdgeType::Unknown;
			const int64 RelationIndex = CurrentSocketInfos.Socket->GetTargetIndex(PointIndex);

			if (RelationIndex != -1)
			{
				for (const FSocketInfos& OtherSocketInfos : SocketInfos)
				{
					if (OtherSocketInfos.Socket->GetTargetIndex(RelationIndex) == PointIndex)
					{
						Type = GetEdgeType(CurrentSocketInfos, OtherSocketInfos);
					}
				}

				if (Type == EPCGExEdgeType::Unknown) { Type = EPCGExEdgeType::Roaming; }
			}


			CurrentSocketInfos.Socket->SetEdgeType(PointIndex, Type);
		}
	}

#pragma endregion
}


/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExRoamingSocketParamsData : public UPCGPointData
{
	GENERATED_BODY()

public:
	UPCGExRoamingSocketParamsData(const FObjectInitializer& ObjectInitializer);
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Param; }

	FPCGExSocketDescriptor Descriptor;

	virtual void BeginDestroy() override;
};

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

	TArray<FPCGExSocketDescriptor> SocketsDescriptors;
	FPCGExSocketGlobalOverrides GlobalOverrides;
	FPCGExSocketDescriptor OverrideSocket = FPCGExSocketDescriptor(NAME_None);

	/**
	 * 
	 * @param PointData 
	 * @return 
	 */
	bool HasMatchingGraphData(const UPCGPointData* PointData) const;

	FName GraphIdentifier = "GraphIdentifier";
	FName CachedIndexAttributeName;
	uint64 GraphUID = 0;

	virtual void BeginDestroy() override;

protected:
	PCGExGraph::FSocketMapping* SocketMapping = nullptr;

public:
	const PCGExGraph::FSocketMapping* GetSocketMapping() const { return SocketMapping; }

	/**
	 * Initialize this data object from a list of socket descriptors 
	 */
	virtual void Initialize();

	/**
	 * Prepare socket mapping for working with a given PointData object.
	 * @param PointIO
	 * @param bReadOnly 
	 */
	void PrepareForPointData(const PCGExData::FPointIO& PointIO, const bool bReadOnly) const;

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

	void GetSocketsInfos(TArray<PCGExGraph::FSocketInfos>& OutInfos) const;

	void Cleanup() const;
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

		FGraphInputs(FPCGContext* Context, const FName InputLabel): FGraphInputs()
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
				InGraph->GlobalOverrides,
				InGraph->OverrideSocket);
		}

		static UPCGExGraphParamsData* NewGraph(
			const uint64 GraphUID,
			const FName Identifier,
			const TArray<FPCGExSocketDescriptor>& Sockets,
			const FPCGExSocketGlobalOverrides& GlobalOverrides,
			const FPCGExSocketDescriptor& OverrideSocket)
		{
			UPCGExGraphParamsData* OutParams = NewObject<UPCGExGraphParamsData>();

			OutParams->GraphUID = GraphUID;
			OutParams->GraphIdentifier = Identifier;

			OutParams->SocketsDescriptors.Append(Sockets);
			OutParams->GlobalOverrides = GlobalOverrides;
			OutParams->OverrideSocket = OverrideSocket;
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

	static void GetUniqueSocketParams(
		const FPCGContext* Context,
		const FName Pin,
		TArray<FPCGExSocketDescriptor>& OutSockets,
		TArray<FPCGExSocketDescriptor>& OmittedSockets)
	{
		OutSockets.Empty();
		OmittedSockets.Empty();

		TArray<FPCGTaggedData> TaggedData = Context->InputData.GetInputsByPin(Pin);
		for (const FPCGTaggedData& TData : TaggedData)
		{
			const UPCGExRoamingSocketParamsData* SocketData = Cast<UPCGExRoamingSocketParamsData>(TData.Data);
			if (!SocketData) { continue; }
			bool bNameOverlap = false;

			for (const FPCGExSocketDescriptor& SocketDescriptor : OutSockets)
			{
				if (SocketDescriptor.SocketName == SocketData->Descriptor.SocketName)
				{
					bNameOverlap = true;
					break;
				}
			}

			if (bNameOverlap)
			{
				OmittedSockets.Add(SocketData->Descriptor);
				continue;
			}

			OutSockets.Add(SocketData->Descriptor);
		}
	}
}
