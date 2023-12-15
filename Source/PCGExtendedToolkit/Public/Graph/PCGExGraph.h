// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "NetworkMessage.h"

#include "Data/PCGExPointIO.h"
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(Units="Degrees", UIMin=0, UIMax=180))
	double Angle = 45.0; // 0.707f dot

	UPROPERTY(BlueprintReadOnly, meta=(UIMin=-1, UIMax=1))
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

	const FName SourcePathsLabel = TEXT("Paths");
	const FName OutputPathsLabel = TEXT("Paths");

	const FName PUIDAttributeName = TEXT("__PCGEx/PUID_");

	constexpr PCGExMT::AsyncState State_ReadyForNextGraph = __COUNTER__;
	constexpr PCGExMT::AsyncState State_ProcessingGraph = __COUNTER__;

	constexpr PCGExMT::AsyncState State_CachingGraphIndices = __COUNTER__;
	constexpr PCGExMT::AsyncState State_SwappingGraphIndices = __COUNTER__;

	constexpr PCGExMT::AsyncState State_FindingEdgeTypes = __COUNTER__;

	constexpr PCGExMT::AsyncState State_BuildNetwork = __COUNTER__;
	constexpr PCGExMT::AsyncState State_WritingIslands = __COUNTER__;
	constexpr PCGExMT::AsyncState State_WaitingOnWritingIslands = __COUNTER__;

	constexpr PCGExMT::AsyncState State_PromotingEdges = __COUNTER__;


#pragma region Sockets

	struct PCGEXTENDEDTOOLKIT_API FSocketMetadata
	{
		FSocketMetadata()
		{
		}

		FSocketMetadata(int32 InIndex, EPCGExEdgeType InEdgeType):
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
		FProbeDistanceModifier(): FLocalSingleFieldGetter()
		{
		}

		FProbeDistanceModifier(const FPCGExSocketDescriptor& InDescriptor): FLocalSingleFieldGetter()
		{
			Descriptor = static_cast<FPCGExInputDescriptor>(InDescriptor.AttributeModifier);
			bEnabled = InDescriptor.bApplyAttributeModifier;
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FLocalDirection : public PCGEx::FLocalDirectionGetter
	{
		FLocalDirection(): FLocalDirectionGetter()
		{
		}

		FLocalDirection(const FPCGExSocketDescriptor& InDescriptor): FLocalDirectionGetter()
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

		FSocket(const FPCGExSocketDescriptor& InDescriptor): FSocket()
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
		EPCGExSocketType GetType() const { return Descriptor.SocketType; }
		bool IsExclusive() const { return Descriptor.bExclusiveBehavior; }
		bool Matches(const FSocket* OtherSocket) const { return MatchingSockets.Contains(OtherSocket->SocketIndex); }

		void DeleteFrom(const UPCGPointData* PointData) const;

		void Write(bool DoCleanup = true);

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

		PCGEx::TFAttributeWriter<int32>& GetTargetIndexWriter() { return *TargetIndexWriter; }
		PCGEx::TFAttributeWriter<int32>& GetEdgeTypeWriter() { return *EdgeTypeWriter; }
		PCGEx::TFAttributeReader<int32>& GetTargetIndexReader() { return *TargetIndexReader; }
		PCGEx::TFAttributeReader<int32>& GetEdgeTypeReader() { return *EdgeTypeReader; }
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
		 * @param bEnsureEdgeType Whether EdgeType attribute must be created if it doesn't exist. Set this to true if you intend on updating it. 
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
	static EPCGExEdgeType GetEdgeType(const FSocketInfos& StartSocket, const FSocketInfos& EndSocket);

	static void ComputeEdgeType(const TArray<FSocketInfos>& SocketInfos, const int32 PointIndex);

#pragma endregion
}
