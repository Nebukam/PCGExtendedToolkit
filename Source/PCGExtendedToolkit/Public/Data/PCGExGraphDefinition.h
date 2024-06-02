// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Math/UnrealMathUtility.h"

#include "PCGExAttributeHelpers.h"
#include "PCGExCompare.h"
#include "PCGExDataState.h"
#include "PCGParamData.h"
#include "Graph/PCGExGraph.h"

#include "PCGExGraphDefinition.generated.h"

class UPCGPointData;

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Socket State Mode"))
enum class EPCGExSocketStateMode : uint8
{
	AnyOf UMETA(DisplayName = "Any of", ToolTip="Any of the selection"),
	Exactly UMETA(DisplayName = "Exactly", ToolTip="Exactly the selection"),
	Ignore UMETA(DisplayName = "Ignore", ToolTip="Ignore (Always pass)")
};

struct FPCGExPointsProcessorContext;
ENUM_CLASS_FLAGS(EPCGExEdgeType)

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Tangent Type"))
enum class EPCGExTangentType : uint8
{
	Custom UMETA(DisplayName = "Custom", Tooltip="Custom Attributes."),
	Extrapolate UMETA(DisplayName = "Extrapolate", Tooltip="Extrapolate from neighbors position and direction"),
};

ENUM_CLASS_FLAGS(EPCGExTangentType)

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Socket Type"))
enum class EPCGExSocketType : uint8
{
	None   = 0 UMETA(DisplayName = "None", Tooltip="This socket has no particular type."),
	Output = 1 << 0 UMETA(DisplayName = "Output", Tooltip="This socket in an output socket. It can only connect to Input sockets."),
	Input  = 1 << 1 UMETA(DisplayName = "Input", Tooltip="This socket in an input socket. It can only connect to Output sockets"),
	Any    = Output | Input UMETA(DisplayName = "Any", Tooltip="This socket is considered both an Output and and Input."),
};

ENUM_CLASS_FLAGS(EPCGExSocketType)

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Adjacency Test Mode"))
enum class EPCGExAdjacencyTestMode : uint8
{
	All UMETA(DisplayName = "All", Tooltip="Test a condition using all adjacent nodes."),
	Some UMETA(DisplayName = "Some", Tooltip="Test a condition using some adjacent nodes only.")
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Adjacency Gather Mode"))
enum class EPCGExAdjacencyGatherMode : uint8
{
	Individual UMETA(DisplayName = "Individual", Tooltip="Test individual nodes"),
	Average UMETA(DisplayName = "Average", Tooltip="Average value"),
	Min UMETA(DisplayName = "Min", Tooltip="Min value"),
	Max UMETA(DisplayName = "Max", Tooltip="Max value"),
	Sum UMETA(DisplayName = "Sum", Tooltip="Sum value"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Adjacency Subset Mode"))
enum class EPCGExAdjacencySubsetMode : uint8
{
	AtLeast UMETA(DisplayName = "At Least", Tooltip="Requirements must be met by at least X adjacent nodes."),
	AtMost UMETA(DisplayName = "At Most", Tooltip="Requirements must be met by at most X adjacent nodes."),
	Exactly UMETA(DisplayName = "Exactly", Tooltip="Requirements must be met by exactly X adjacent nodes, no more, no less.")
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Relative Rounding Mode"))
enum class EPCGExRelativeRoundingMode : uint8
{
	Round UMETA(DisplayName = "Round", Tooltip="Rounds value to closest integer (0.1 = 0, 0.9 = 1)"),
	Floor UMETA(DisplayName = "Floor", Tooltip="Rounds value to closest smaller integer (0.1 = 0, 0.9 = 0)"),
	Ceil UMETA(DisplayName = "Ceil", Tooltip="Rounds value to closest highest integer (0.1 = 1, 0.9 = 1)"),
};

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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Probing", meta=(PCG_Overridable))
	FPCGExDistanceSettings DistanceSettings;

	/** Slot 'look-at' direction. Used along with DotTolerance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Probing", meta=(PCG_Overridable))
	FVector Direction = FVector::UpVector;

	/** If true, the direction vector of the socket will be read from a local attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Probing", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalDirection = false;

	/** Local property or attribute to read Direction from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Probing", meta = (PCG_Overridable, EditCondition="bUseLocalDirection"))
	FPCGAttributePropertyInputSelector LocalDirection;

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
	FPCGAttributePropertyInputSelector LocalAngle;

	/** Enable if the local angle should be read as degrees instead of radians. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Probing", meta = (PCG_Overridable, EditCondition="bUseLocalAngle"))
	bool bLocalAngleIsDegrees = true;

	//

	/** Maximum search radius. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Probing", meta=(PCG_Overridable, ClampMin=0.0001))
	double Radius = 100.0f;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Probing", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalRadius = false;

	/** Local property or attribute to read Radius from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Probing", meta = (PCG_Overridable, EditCondition="bUseLocalRadius"))
	FPCGAttributePropertyInputSelector LocalRadius;

	/** The balance over distance to prioritize closer distance or better alignment. Curve X is normalized distance; Y = 0 means narrower dot wins, Y = 1 means closer distance wins */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Probing")
	TSoftObjectPtr<UCurveFloat> DotOverDistance = TSoftObjectPtr<UCurveFloat>(PCGEx::DefaultDotOverDistanceCurve);

	TObjectPtr<UCurveFloat> DotOverDistanceCurve = nullptr;

	void LoadCurve()
	{
		PCGEX_LOAD_SOFTOBJECT(UCurveFloat, DotOverDistance, DotOverDistanceCurve, PCGEx::DefaultDotOverDistanceCurve)
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
struct PCGEXTENDEDTOOLKIT_API FPCGExSocketTestDescriptor
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bEnabled = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName SocketName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Must be ..."))
	EPCGExSocketStateMode MustBeMode = EPCGExSocketStateMode::AnyOf;

	/** Edge types to crawl to create a Cluster */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExEdgeType", EditCondition="MustBeMode==EPCGExSocketStateMode::AnyOf", EditConditionHides))
	uint8 MustBeAnyOf = static_cast<uint8>(EPCGExEdgeType::Complete);

	/** Edge types to crawl to create a Cluster */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="MustBeMode==EPCGExSocketStateMode::Exactly", EditConditionHides))
	EPCGExEdgeType MustBeExactly = EPCGExEdgeType::Complete;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Must NOT be ..."))
	EPCGExSocketStateMode MustNotBeMode = EPCGExSocketStateMode::Exactly;

	/** Edge types to crawl to create a Cluster */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExEdgeType", EditCondition="MustNotBeMode==EPCGExSocketStateMode::AnyOf", EditConditionHides))
	uint8 MustNotBeAnyOf = static_cast<uint8>(EPCGExEdgeType::Unknown);

	/** Edge types to crawl to create a Cluster */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="MustNotBeMode==EPCGExSocketStateMode::Exactly", EditConditionHides))
	EPCGExEdgeType MustNotBeExactly = EPCGExEdgeType::Unknown;

	void Populate(const FPCGExSocketDescriptor& Descriptor)
	{
		SocketName = Descriptor.SocketName;
	}

	bool MeetCondition(int32 InValue) const
	{
		switch (MustBeMode)
		{
		default: break;
		case EPCGExSocketStateMode::AnyOf:
			if (static_cast<uint8>(static_cast<uint8>(InValue) & MustBeAnyOf) == 0) { return false; }
			break;
		case EPCGExSocketStateMode::Exactly:
			if (static_cast<EPCGExEdgeType>(InValue) != MustBeExactly) { return false; }
			break;
		}

		switch (MustNotBeMode)
		{
		default: break;
		case EPCGExSocketStateMode::AnyOf:
			if (static_cast<uint8>(static_cast<uint8>(InValue) & MustNotBeAnyOf) != 0) { return false; }
			break;
		case EPCGExSocketStateMode::Exactly:
			if (static_cast<EPCGExEdgeType>(InValue) == MustNotBeExactly) { return false; }
			break;
		}

		return true;
	}
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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probing", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalDirection = false;

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
	bool bDistanceSettings = false;

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
			Descriptor.DotThreshold = PCGExMath::DegreesToDot(Descriptor.Angle);
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
			OutDirection = LocalDirectionGetter ? LocalDirectionGetter->SafeGet(PointIndex, Descriptor.Direction).GetSafeNormal() : Descriptor.Direction;
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
class PCGEXTENDEDTOOLKIT_API UPCGExSocketFactory : public UPCGExParamFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExSocketDescriptor Descriptor;
};

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExSocketStateFactory : public UPCGExDataStateFactoryBase
{
	GENERATED_BODY()

public:
	TArray<FPCGExSocketTestDescriptor> FilterFactories;
	virtual PCGExFactories::EType GetFactoryType() const override;
	virtual PCGExDataFilter::TFilter* CreateFilter() const override;
	virtual void BeginDestroy() override;
};

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExGraphDefinition : public UPCGPointData
{
	GENERATED_BODY()

public:
	UPCGExGraphDefinition(const FObjectInitializer& ObjectInitializer);
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Param; }

	TArray<FPCGExSocketDescriptor> SocketsDescriptors;
	FPCGExSocketGlobalOverrides GlobalOverrides;
	FPCGExSocketDescriptor OverrideSocket = FPCGExSocketDescriptor(NAME_None);

	/**
	 * 
	 * @param PointData 
	 * @return 
	 */
	bool HasMatchingGraphDefinition(const UPCGPointData* PointData) const;

	FName GraphIdentifier = "GraphIdentifier";
	FName CachedIndexAttributeName;
	uint64 GraphUID = 0;

	bool ContainsNamedSocked(FName InName) const;
	void AddSocketNames(TSet<FName>& OutUniqueNames) const;

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
		TArray<UPCGExGraphDefinition*> Params;
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
				const UPCGExGraphDefinition* GraphData = Cast<UPCGExGraphDefinition>(Source.Data);
				if (!GraphData) { continue; }
				if (UniqueParams.Contains(GraphData->GraphUID)) { continue; }
				UniqueParams.Add(GraphData->GraphUID);
				Params.Add(const_cast<UPCGExGraphDefinition*>(GraphData));
				//Params.Add(CopyGraph(GraphData));
				ParamsSources.Add(Source);
			}
			UniqueParams.Empty();
		}

		static UPCGExGraphDefinition* CopyGraph(const UPCGExGraphDefinition* InGraph)
		{
			return NewGraph(
				InGraph->GraphUID,
				InGraph->GraphIdentifier,
				InGraph->SocketsDescriptors,
				InGraph->GlobalOverrides,
				InGraph->OverrideSocket);
		}

		static UPCGExGraphDefinition* NewGraph(
			const uint64 GraphUID,
			const FName Identifier,
			const TArray<FPCGExSocketDescriptor>& Sockets,
			const FPCGExSocketGlobalOverrides& GlobalOverrides,
			const FPCGExSocketDescriptor& OverrideSocket)
		{
			UPCGExGraphDefinition* OutParams = NewObject<UPCGExGraphDefinition>();

			OutParams->GraphUID = GraphUID;
			OutParams->GraphIdentifier = Identifier;

			OutParams->SocketsDescriptors.Append(Sockets);
			OutParams->GlobalOverrides = GlobalOverrides;
			OutParams->OverrideSocket = OverrideSocket;
			OutParams->Initialize();

			return OutParams;
		}

		void ForEach(FPCGContext* Context, const TFunction<void(UPCGExGraphDefinition*, const int32)>& BodyLoop)
		{
			for (int i = 0; i < Params.Num(); i++)
			{
				UPCGExGraphDefinition* ParamsData = Params[i]; //TODO : Create "working copies" so we don't start working with destroyed sockets, as params lie early in the graph
				BodyLoop(ParamsData, i);
			}
		}

		void OutputTo(FPCGContext* Context) const
		{
			for (int i = 0; i < ParamsSources.Num(); i++)
			{
				FPCGTaggedData& OutputRef = Context->OutputData.TaggedData.Add_GetRef(ParamsSources[i]);
				OutputRef.Pin = OutputForwardGraphsLabel;
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
			const UPCGExSocketFactory* SocketData = Cast<UPCGExSocketFactory>(TData.Data);
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

	class PCGEXTENDEDTOOLKIT_API FSocketStateHandler : public PCGExDataState::TDataState
	{
	public:
		explicit FSocketStateHandler(const UPCGExSocketStateFactory* InDefinition);

		const UPCGExSocketStateFactory* SocketStateDefinition = nullptr;
		TArray<FPCGMetadataAttribute<int32>*> EdgeTypeAttributes;
		TArray<PCGEx::TFAttributeReader<int32>*> EdgeTypeReaders;

		void CaptureGraph(const FGraphInputs* GraphInputs, const PCGExData::FPointIO* InPointIO);
		void CaptureGraph(const UPCGExGraphDefinition* Graph, const PCGExData::FPointIO* InPointIO);

		virtual void PrepareForTesting(const PCGExData::FPointIO* PointIO) override;

		virtual bool Test(const int32 PointIndex) const override;

		virtual ~FSocketStateHandler() override
		{
			EdgeTypeAttributes.Empty();
			PCGEX_DELETE_TARRAY(EdgeTypeReaders)
		}
	};
}


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSocketQualityOfLifeInfos
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FString BaseName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FString FullName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FString IndexAttribute;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FString EdgeTypeAttribute;

	void Populate(const FName Identifier, const FPCGExSocketDescriptor& Descriptor)
	{
		BaseName = Descriptor.SocketName.ToString();
		const FString Separator = TEXT("/");
		FullName = *(TEXT("PCGEx") + Separator + Identifier.ToString() + Separator + BaseName);
		IndexAttribute = *(FullName + Separator + PCGExGraph::SocketPropertyNameIndex.ToString());
		EdgeTypeAttribute = *(FullName + Separator + PCGExGraph::SocketPropertyNameEdgeType.ToString());
	}
};
