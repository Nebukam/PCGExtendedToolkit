// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Data/PCGExAttributeHelpers.h"
#include "PCGExMT.h"
#include "PCGExEdge.h"

#include "PCGExGraph.generated.h"

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
struct PCGEXTENDEDTOOLKIT_API FPCGExSocketBounds
{
	GENERATED_BODY()

	FPCGExSocketBounds()
		: DotOverDistance(PCGEx::DefaultDotOverDistanceCurve)
	{
	}

	FPCGExSocketBounds(const FVector& Dir): FPCGExSocketBounds()
	{
		Direction = Dir;
	}

	FPCGExSocketBounds(
		const FPCGExSocketBounds& Other):
		Direction(Other.Direction),
		DotThreshold(Other.DotThreshold),
		MaxDistance(Other.MaxDistance),
		DotOverDistance(Other.DotOverDistance)
	{
	}

public:
	/** Slot 'look-at' direction. Used along with DotTolerance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector Direction = FVector::UpVector;

	/** Cone threshold. Used along with the direction of the slot when looking for the closest candidate. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(Units="Degrees", ClampMin=0, ClampMax=180))
	double Angle = 45.0; // 0.707f dot

	UPROPERTY(BlueprintReadOnly, meta=(ClampMin=-1, ClampMax=1))
	double DotThreshold = 0.707;

	/** Maximum sampling distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	double MaxDistance = 1000.0f;

	/** The balance over distance to prioritize closer distance or better alignment. Curve X is normalized distance; Y = 0 means narrower dot wins, Y = 1 means closer distance wins */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UCurveFloat> DotOverDistance;

	UCurveFloat* DotOverDistanceCurve = nullptr;

	void LoadCurve() { DotOverDistanceCurve = DotOverDistance.LoadSynchronous(); }
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSocketDescriptor
{
	GENERATED_BODY()

	FPCGExSocketDescriptor()
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
		Bounds.Direction = InDirection;
		Bounds.Angle = InAngle;
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
		Bounds.Direction = InDirection;
		Bounds.Angle = InAngle;
		MatchingSlots.Add(InMatchingSlot);
	}

public:
	/** Name of the attribute to write neighbor index to. */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere)
	FName SocketName = NAME_None;

	/** Type of socket. */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere)
	EPCGExSocketType SocketType = EPCGExSocketType::Any;

	/** Exclusive sockets can only connect to other socket matching */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere)
	bool bExclusiveBehavior = false;

	/** Socket spatial definition */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta=(ShowOnlyInnerProperties))
	FPCGExSocketBounds Bounds;

	/** Whether the orientation of the direction is relative to the point transform or not. */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere)
	bool bRelativeOrientation = true;

	/** If true, the direction vector of the socket will be read from a local attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle))
	bool bDirectionVectorFromAttribute = false;

	/** Sibling slots names that are to be considered as a match. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FName> MatchingSlots;

	/** Inject this slot as a match to slots referenced in the Matching Slots list. Useful to save time */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bMirrorMatchingSockets = true;

	/** Local attribute to override the direction vector with */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bDirectionVectorFromAttribute", ShowOnlyInnerProperties))
	FPCGExInputDescriptorWithDirection AttributeDirectionVector;

	/** If enabled, multiplies the max sampling distance of this socket by the value of a local attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle))
	bool bApplyAttributeModifier = false;

	/** Local attribute to multiply the max distance by.  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bApplyAttributeModifier", ShowOnlyInnerProperties))
	FPCGExInputDescriptorWithSingleField AttributeModifier;

	/** Offset socket origin  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExExtension OffsetOrigin = EPCGExExtension::None;

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
		: DotOverDistance(PCGEx::DefaultDotOverDistanceCurve)
	{
	}

public:
	/** Override all socket orientation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOverrideRelativeOrientation = false;

	/** If true, the direction vector will be affected by the point' world rotation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOverrideRelativeOrientation"))
	bool bRelativeOrientation = false;


	/** Override all socket bExclusiveBehavior. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOverrideExclusiveBehavior = false;

	/** If true, Input & Outputs can only connect to sockets registered as a Match. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOverrideExclusiveBehavior"))
	bool bExclusiveBehavior = false;


	/** Override all socket cone. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOverrideAngle = false;

	/** Cone threshold. Used along with the direction of the slot when looking for the closest candidate. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOverrideAngle"))
	double Angle = 45.0; // 0.707f dot


	/** Override all socket distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOverrideMaxDistance = false;

	/** Maximum sampling distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOverrideMaxDistance"))
	double MaxDistance = 100.0f;


	/** Override all socket Read direction vector from local attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle))
	bool bOverrideDirectionVectorFromAttribute = false;

	/** Is the direction vector read from local attributes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle, EditCondition="bOverrideDirectionVectorFromAttribute"))
	bool bDirectionVectorFromAttribute = false;

	/** Local attribute from which the direction will be read. Must be a FVectorN otherwise will lead to bad results. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOverrideDirectionVectorFromAttribute", ShowOnlyInnerProperties))
	FPCGExInputDescriptor AttributeDirectionVector;


	/** Override all socket Modifiers. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOverrideAttributeModifier = false;

	/** Is the distance modified by local attributes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOverrideAttributeModifier", InlineEditConditionToggle))
	bool bApplyAttributeModifier = false;

	/** Which local attribute is used to factor the distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOverrideAttributeModifier"))
	FPCGExInputDescriptorWithSingleField AttributeModifier;

	/** Is the distance modified by local attributes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOverrideDotOverDistance = false;

	/**Curve to balance picking shortest distance over better angle. Only used by certain solvers.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOverrideDotOverDistance"))
	TSoftObjectPtr<UCurveFloat> DotOverDistance;

	/** Offset socket origin */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOverrideOffsetOrigin = false;

	/** What property to use for the offset.  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOverrideOffsetOrigin"))
	EPCGExExtension OffsetOrigin = EPCGExExtension::None;
};

#pragma endregion

namespace PCGExGraph
{
	const FName SourceParamsLabel = TEXT("Graph");
	const FName OutputParamsLabel = TEXT("➜");

	const FName SourceGraphsLabel = TEXT("In");
	const FName OutputGraphsLabel = TEXT("Out");

	const FName SourceVerticesLabel = TEXT("Vtx");
	const FName OutputVerticesLabel = TEXT("Vtx");

	const FName SourcePathsLabel = TEXT("Paths");
	const FName OutputPathsLabel = TEXT("Paths");

	const FName PUIDAttributeName = TEXT("__PCGEx/PUID_");

	constexpr PCGExMT::AsyncState State_ReadyForNextGraph = __COUNTER__;
	constexpr PCGExMT::AsyncState State_ProcessingGraph = __COUNTER__;

	constexpr PCGExMT::AsyncState State_CachingGraphIndices = __COUNTER__;
	constexpr PCGExMT::AsyncState State_SwappingGraphIndices = __COUNTER__;

	constexpr PCGExMT::AsyncState State_FindingEdgeTypes = __COUNTER__;

	constexpr PCGExMT::AsyncState State_BuildNetwork = __COUNTER__;
	constexpr PCGExMT::AsyncState State_FindingCrossings = __COUNTER__;
	constexpr PCGExMT::AsyncState State_WritingIslands = __COUNTER__;
	constexpr PCGExMT::AsyncState State_WaitingOnWritingIslands = __COUNTER__;

	constexpr PCGExMT::AsyncState State_PromotingEdges = __COUNTER__;


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

	struct PCGEXTENDEDTOOLKIT_API FProbeDistanceModifier : public PCGEx::FLocalSingleFieldGetter
	{
		FProbeDistanceModifier()
		{
		}

		explicit FProbeDistanceModifier(const FPCGExSocketDescriptor& InDescriptor)
		{
			Descriptor = static_cast<FPCGExInputDescriptor>(InDescriptor.AttributeModifier);
			bEnabled = InDescriptor.bApplyAttributeModifier;
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FLocalDirection : public PCGEx::FLocalDirectionGetter
	{
		FLocalDirection()
		{
		}

		explicit FLocalDirection(const FPCGExSocketDescriptor& InDescriptor)
		{
			Descriptor = static_cast<FPCGExInputDescriptor>(InDescriptor.AttributeDirectionVector);
			bEnabled = InDescriptor.bDirectionVectorFromAttribute;
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
			Descriptor.Bounds.DotThreshold = FMath::Cos(Descriptor.Bounds.Angle * (PI / 180.0)); //Degrees to dot product
		}

		~FSocket();

		FPCGExSocketDescriptor Descriptor;

		int32 SocketIndex = -1;
		TSet<int32> MatchingSockets;

	protected:
		bool bReadOnly = false;
		PCGEx::TFAttributeWriter<int32>* TargetIndexWriter = nullptr;
		PCGEx::TFAttributeWriter<int32>* EdgeTypeWriter = nullptr;
		PCGEx::TFAttributeReader<int32>* TargetIndexReader = nullptr;
		PCGEx::TFAttributeReader<int32>* EdgeTypeReader = nullptr;
		FName AttributeNameBase = NAME_None;

		void Cleanup();

	public:
		FName GetName() const { return AttributeNameBase; }
		EPCGExSocketType GetSocketType() const { return Descriptor.SocketType; }
		bool IsExclusive() const { return Descriptor.bExclusiveBehavior; }
		bool Matches(const FSocket* OtherSocket) const { return MatchingSockets.Contains(OtherSocket->SocketIndex); }

		void DeleteFrom(const UPCGPointData* PointData) const;

		void Write(bool bDoCleanup = true);

		/**
		 * Find or create the attribute matching this socket on a given PointData object,
		 * as well as prepare the scape modifier for that same object.
		 * @param PointIO
		 * @param ReadOnly
		 */
		void PrepareForPointData(const PCGExData::FPointIO& PointIO, const bool ReadOnly = true);

		void SetData(const PCGMetadataEntryKey MetadataEntry, const FSocketMetadata& SocketMetadata) const;

		// Point index within the same data group.
		void SetTargetIndex(const int32 PointIndex, int32 InValue) const;

		int32 GetTargetIndex(const int32 PointIndex) const;

		// Relation type
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
		FProbeDistanceModifier* MaxDistanceGetter = nullptr;
		FLocalDirection* LocalDirectionGetter = nullptr;

		bool Matches(const FSocketInfos& Other) const { return Socket->Matches(Other.Socket); }

		~FSocketInfos()
		{
			Socket = nullptr;
			MaxDistanceGetter = nullptr;
			LocalDirectionGetter = nullptr;
		}
	};

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
		TArray<FProbeDistanceModifier> Modifiers;
		TArray<FLocalDirection> LocalDirections;
		TMap<FName, int32> NameToIndexMap;
		int32 NumSockets = 0;

		void Initialize(const FName InIdentifier, TArray<FPCGExSocketDescriptor>& InSockets);
		void InitializeWithOverrides(FName InIdentifier, TArray<FPCGExSocketDescriptor>& InSockets, const FPCGExSocketGlobalOverrides& Overrides);

		FName GetCompoundName(FName SecondaryIdentifier) const;

		/**
		 * Prepare socket mapping for working with a given PointData object.
		 * Each socket will cache Attribute & accessors
		 * @param PointIO
		 * @param bReadOnly 
		 */
		void PrepareForPointData(const PCGExData::FPointIO& PointIO, const bool bReadOnly = true);

		const TArray<FSocket>& GetSockets() const { return Sockets; }
		const TArray<FProbeDistanceModifier>& GetModifiers() const { return Modifiers; }

		void GetSocketsInfos(TArray<FSocketInfos>& OutInfos);
		void Cleanup();
		void Reset();

	private:
		/**
		 * Build matching set
		 */
		void PostProcessSockets();
	};

#pragma endregion

#pragma region Edges

	struct PCGEXTENDEDTOOLKIT_API FCachedSocketData
	{
		FCachedSocketData()
		{
			Neighbors.Empty();
		}

		int64 Index = -1;
		TArray<FSocketMetadata> Neighbors;
	};

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

#pragma region Network
	struct PCGEXTENDEDTOOLKIT_API FNetworkNode
	{
		FNetworkNode()
		{
		}

		int32 Index = -1;
		int32 Island = -1;
		bool bCrossing = false;
		TArray<int32> Edges;

		bool IsIsolated() const { return Island == -1; }

		bool GetNeighbors(TArray<int32>& OutIndices, const TArray<FUnsignedEdge>& InEdges)
		{
			if (Edges.IsEmpty()) { return false; }
			for (const int32 Edge : Edges) { OutIndices.Add(InEdges[Edge].Other(Index)); }
			return true;
		}

		template <typename Func, typename... Args>
		void CallbackOnNeighbors(const TArray<FUnsignedEdge>& InEdges, Func Callback, Args&&... InArgs)
		{
			for (const int32 Edge : Edges) { Callback(InEdges[Edge].Other(Index, std::forward<Args>(InArgs)...)); }
		}

		void AddEdge(const int32 Edge)
		{
			Edges.AddUnique(Edge);
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FEdgeCrossing
	{
		FEdgeCrossing()
		{
		}

		int32 EdgeA = -1;
		int32 EdgeB = -1;
		FVector Center;
	};

	struct PCGEXTENDEDTOOLKIT_API FEdgeNetwork
	{
		mutable FRWLock NetworkLock;

		const int32 NumEdgesReserve;
		int32 IslandIncrement = 0;
		int32 NumIslands = 0;
		int32 NumEdges = 0;

		TArray<FNetworkNode> Nodes;
		TSet<uint64> UniqueEdges;
		TArray<FUnsignedEdge> Edges;
		TMap<int32, int32> IslandSizes;

		~FEdgeNetwork()
		{
			Nodes.Empty();
			UniqueEdges.Empty();
			Edges.Empty();
			IslandSizes.Empty();
		}

		FEdgeNetwork(const int32 InNumEdgesReserve, const int32 InNumNodes)
			: NumEdgesReserve(InNumEdgesReserve)
		{
			Nodes.SetNum(InNumNodes);

			int32 Index = 0;

			for (FNetworkNode& Node : Nodes)
			{
				Node.Index = Index++;
				Node.Edges.Reserve(NumEdgesReserve);
			}
		}

		bool InsertEdge(const FUnsignedEdge Edge)
		{
			const uint64 Hash = Edge.GetUnsignedHash();

			{
				FReadScopeLock ReadLock(NetworkLock);
				if (UniqueEdges.Contains(Hash)) { return false; }
			}


			FWriteScopeLock WriteLock(NetworkLock);
			UniqueEdges.Add(Hash);
			const int32 EdgeIndex = Edges.Add(Edge);


			FNetworkNode& NodeA = Nodes[Edge.Start];
			FNetworkNode& NodeB = Nodes[Edge.End];

			NodeA.AddEdge(EdgeIndex);
			NodeB.AddEdge(EdgeIndex);

			if (NodeA.Island == -1 && NodeB.Island == -1)
			{
				// New island
				NumIslands++;
				NodeA.Island = NodeB.Island = IslandIncrement++;
			}
			else if (NodeA.Island != -1 && NodeB.Island != -1)
			{
				if (NodeA.Island != NodeB.Island)
				{
					// Merge islands
					NumIslands--;
					MergeIsland(NodeB.Index, NodeA.Island);
				}
			}
			else
			{
				// Expand island
				NodeA.Island = NodeB.Island = FMath::Max(NodeA.Island, NodeB.Island);
			}

			return true;
		}

		void MergeIsland(const int32 NodeIndex, const int32 Island)
		{
			FNetworkNode& Node = Nodes[NodeIndex];
			if (Node.Island == Island) { return; }
			Node.Island = Island;
			for (const int32 EdgeIndex : Node.Edges)
			{
				MergeIsland(Edges[EdgeIndex].Other(NodeIndex), Island);
			}
		}

		void PrepareIslands(const int32 MinSize = 1, const int32 MaxSize = TNumericLimits<int32>::Max())
		{
			UniqueEdges.Empty();
			IslandSizes.Empty(NumIslands);
			NumEdges = 0;

			for (const FUnsignedEdge& Edge : Edges)
			{
				if (!Edge.bValid) { continue; } // Crossing may invalidate edges.
				FNetworkNode& NodeA = Nodes[Edge.Start];
				if (const int32* SizePtr = IslandSizes.Find(NodeA.Island); !SizePtr) { IslandSizes.Add(NodeA.Island, 1); }
				else { IslandSizes.Add(NodeA.Island, *SizePtr + 1); }
			}

			for (TPair<int32, int32>& Pair : IslandSizes)
			{
				if (FMath::IsWithin(Pair.Value, MinSize, MaxSize)) { NumEdges += Pair.Value; }
				else { Pair.Value = -1; }
			}
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FEdgeCrossingsHandler
	{
		mutable FRWLock CrossingLock;

		FEdgeNetwork* EdgeNetwork;
		double Tolerance;
		double SquaredTolerance;

		TArray<FBox> SegmentBounds;
		TArray<FEdgeCrossing> Crossings;

		int32 NumEdges;
		int32 StartIndex = 0;

		FEdgeCrossingsHandler(FEdgeNetwork* InEdgeNetwork, const double InTolerance)
			: EdgeNetwork(InEdgeNetwork),
			  Tolerance(InTolerance),
			  SquaredTolerance(InTolerance * InTolerance)
		{
			NumEdges = InEdgeNetwork->Edges.Num();

			Crossings.Empty();

			SegmentBounds.Empty();
			SegmentBounds.Reserve(NumEdges);
		}

		~FEdgeCrossingsHandler()
		{
			SegmentBounds.Empty();
			Crossings.Empty();
			EdgeNetwork = nullptr;
		}

		void Prepare(const TArray<FPCGPoint>& InPoints)
		{
			for (int i = 0; i < NumEdges; i++)
			{
				const FUnsignedEdge& Edge = EdgeNetwork->Edges[i];
				FBox& NewBox = SegmentBounds.Emplace_GetRef(ForceInit);
				NewBox += InPoints[Edge.Start].Transform.GetLocation();
				NewBox += InPoints[Edge.End].Transform.GetLocation();
			}
		}

		void ProcessEdge(const int32 EdgeIndex, const TArray<FPCGPoint>& InPoints)
		{
			TArray<FUnsignedEdge>& Edges = EdgeNetwork->Edges;

			const FUnsignedEdge& Edge = Edges[EdgeIndex];
			const FBox CurrentBox = SegmentBounds[EdgeIndex].ExpandBy(Tolerance);
			const FVector A1 = InPoints[Edge.Start].Transform.GetLocation();
			const FVector B1 = InPoints[Edge.End].Transform.GetLocation();

			for (int i = 0; i < NumEdges; i++)
			{
				if (CurrentBox.Intersect(SegmentBounds[i]))
				{
					const FUnsignedEdge& OtherEdge = Edges[i];
					FVector A2 = InPoints[OtherEdge.Start].Transform.GetLocation();
					FVector B2 = InPoints[OtherEdge.End].Transform.GetLocation();
					FVector A3;
					FVector B3;
					FMath::SegmentDistToSegment(A1, B1, A2, B2, A3, B3);
					const bool bIsEnd = A1 == A3 || B1 == A3 || A2 == A3 || B2 == A3 || A1 == B3 || B1 == B3 || A2 == B3 || B2 == B3;
					if (!bIsEnd && FVector::DistSquared(A3, B3) < SquaredTolerance)
					{
						FWriteScopeLock WriteLock(CrossingLock);
						FEdgeCrossing& EdgeCrossing = Crossings.Emplace_GetRef();
						EdgeCrossing.EdgeA = EdgeIndex;
						EdgeCrossing.EdgeB = i;
						EdgeCrossing.Center = FMath::Lerp(A3, B3, 0.5);
					}
				}
			}
		}

		void InsertCrossings()
		{
			TArray<FNetworkNode>& Nodes = EdgeNetwork->Nodes;
			TArray<FUnsignedEdge>& Edges = EdgeNetwork->Edges;

			Nodes.Reserve(Nodes.Num() + Crossings.Num());
			StartIndex = Nodes.Num();
			int32 Index = StartIndex;
			for (const FEdgeCrossing& EdgeCrossing : Crossings)
			{
				Edges[EdgeCrossing.EdgeA].bValid = false;
				Edges[EdgeCrossing.EdgeB].bValid = false;

				FNetworkNode& NewNode = Nodes.Emplace_GetRef();
				NewNode.Index = Index++;
				NewNode.Edges.Reserve(4);
				NewNode.bCrossing = true;

				EdgeNetwork->InsertEdge(FUnsignedEdge(NewNode.Index, Edges[EdgeCrossing.EdgeA].Start, EPCGExEdgeType::Complete));
				EdgeNetwork->InsertEdge(FUnsignedEdge(NewNode.Index, Edges[EdgeCrossing.EdgeA].End, EPCGExEdgeType::Complete));
				EdgeNetwork->InsertEdge(FUnsignedEdge(NewNode.Index, Edges[EdgeCrossing.EdgeB].Start, EPCGExEdgeType::Complete));
				EdgeNetwork->InsertEdge(FUnsignedEdge(NewNode.Index, Edges[EdgeCrossing.EdgeB].End, EPCGExEdgeType::Complete));
			}
		}
	};
#pragma endregion
}
