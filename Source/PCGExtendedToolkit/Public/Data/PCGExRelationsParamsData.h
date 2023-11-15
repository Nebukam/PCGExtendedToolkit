// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "PCGParamData.h"
#include "Math/UnrealMathUtility.h"
#include "PCGExLocalAttributeHelpers.h"
#include "Relational/PCGExRelationsProcessor.h"
#include "PCGExRelationsParamsData.generated.h"

class UPCGExCreateRelationsParamsSettings;
struct FPCGExRelationsProcessorContext;
class UPCGPointData;

UENUM(BlueprintType)
enum class EPCGExRelationType : uint8
{
	Unknown  = 0 UMETA(DisplayName = "Unknown", Tooltip="Unknown relation."),
	Unique   = 10 UMETA(DisplayName = "Unique", Tooltip="Unique relation."),
	Shared   = 11 UMETA(DisplayName = "Shared", Tooltip="Shared relation, both sockets are connected; but do not match."),
	Match    = 21 UMETA(DisplayName = "Match", Tooltip="Shared relation, considered a match by the primary socket owner; but does not match on the other."),
	Complete = 22 UMETA(DisplayName = "Complete", Tooltip="Shared, matching relation on both sockets."),
	Mirror   = 96 UMETA(DisplayName = "Mirrored relation", Tooltip="Mirrored relation, connected sockets are the same on both points."),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSocketDirection
{
	GENERATED_BODY()

	FPCGExSocketDirection()
	{
	}

	FPCGExSocketDirection(const FVector& Dir)
	{
		Direction = Dir;
	}

	FPCGExSocketDirection(
		const FPCGExSocketDirection& Other):
		Direction(Other.Direction),
		DotTolerance(Other.DotTolerance),
		MaxDistance(Other.MaxDistance)
	{
	}

public:
	/** Slot 'look-at' direction. Used along with DotTolerance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector Direction = FVector::UpVector;

	/** Cone threshold. Used along with the direction of the slot when looking for the closest candidate. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(Units="Degrees", UIMin=0, UIMax=180))
	double Cone = 45.0; // 0.707f dot

	UPROPERTY(BlueprintReadOnly, meta=(UIMin=-1, UIMax=1))
	double DotTolerance = 0.707;

	/** Maximum sampling distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	double MaxDistance = 1000.0f;
};

#pragma region Descriptors

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSocketModifierDescriptor : public FPCGExInputSelectorWithSingleField
{
	GENERATED_BODY()

	FPCGExSocketModifierDescriptor(): FPCGExInputSelectorWithSingleField()
	{
	}

	FPCGExSocketModifierDescriptor(
		const FPCGExSocketModifierDescriptor& Other): FPCGExInputSelectorWithSingleField(Other)
	{
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSocketDescriptor
{
	GENERATED_BODY()

	FPCGExSocketDescriptor()
	{
	}

	FPCGExSocketDescriptor(FName InName, FVector InDirection, FColor InDebugColor):
		SocketName(InName),
		DebugColor(InDebugColor)
	{
		Direction.Direction = InDirection;
	}

	FPCGExSocketDescriptor(FName InName, FVector InDirection, FName InMatchingSlot, FColor InDebugColor):
		SocketName(InName),
		DebugColor(InDebugColor)
	{
		Direction.Direction = InDirection;
		MatchingSlots.Add(InMatchingSlot);
	}

public:
	/** Name of the attribute to write neighbor index to. */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere)
	FName SocketName = NAME_None;

	/** Socket spatial definition */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta=(ShowOnlyInnerProperties))
	FPCGExSocketDirection Direction;

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
	FPCGExInputSelectorWithDirection AttributeDirectionVector;

	/** If enabled, multiplies the max sampling distance of this socket by the value of a local attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle))
	bool bApplyAttributeModifier = false;

	/** Local attribute to multiply the max distance by.  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bApplyAttributeModifier", ShowOnlyInnerProperties))
	FPCGExSocketModifierDescriptor AttributeModifier;

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

public:
	/** Override all socket orientation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOverrideRelativeOrientation = false;

	/** If true, the direction vector will be affected by the point' world rotation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOverrideRelativeOrientation"))
	bool bRelativeOrientation = false;


	/** Override all socket cone. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOverrideCone = false;

	/** Cone threshold. Used along with the direction of the slot when looking for the closest candidate. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOverrideCone"))
	double Cone = 45.0; // 0.707f dot


	/** Override all socket distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOverrideMaxDistance = false;

	/** Maximum sampling distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOverrideMaxDistance"))
	double MaxDistance = 1000.0f;


	/** Override all socket Read direction vector from local attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle))
	bool bOverrideDirectionVectorFromAttribute = false;

	/** Is the direction vector read from local attributes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle, EditCondition="bOverrideDirectionVectorFromAttribute"))
	bool bDirectionVectorFromAttribute = false;

	/** Local attribute from which the direction will be read. Must be a FVectorN otherwise will lead to bad results. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOverrideDirectionVectorFromAttribute", ShowOnlyInnerProperties))
	FPCGExInputSelector AttributeDirectionVector;


	/** Override all socket Modifiers. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOverrideAttributeModifier = false;

	/** Is the distance modified by local attributes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOverrideAttributeModifier", InlineEditConditionToggle))
	bool bApplyAttributeModifier = false;

	/** Which local attribute is used to factor the distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOverrideAttributeModifier"))
	FPCGExSocketModifierDescriptor AttributeModifier;
};

#pragma endregion

namespace PCGExRelational
{
	// Per-socket infos, will end up as FVector4 value
	struct PCGEXTENDEDTOOLKIT_API FSocketMetadata
	{
		FSocketMetadata()
		{
		}

		FSocketMetadata(int64 InIndex, PCGMetadataEntryKey InEntryKey, EPCGExRelationType InRelationType):
			Index(InIndex),
			EntryKey(InEntryKey),
			RelationType(InRelationType)
		{
		}

	public:
		int64 Index = -1; // Index of the point this socket connects to
		PCGMetadataEntryKey EntryKey = PCGInvalidEntryKey;
		EPCGExRelationType RelationType = EPCGExRelationType::Unknown;

		double IndexedDot = -1.0;
		double IndexedDistance = TNumericLimits<double>::Max();

		/*
		 
		friend FArchive& operator<<(FArchive& Ar, FSocketMetadata& SocketMetadata)
		{
			Ar << SocketMetadata.Index;
			Ar << SocketMetadata.EntryKey;
			Ar << SocketMetadata.RelationType;
			Ar << SocketMetadata.IndexedDistance;
			return Ar;
		}

		FSocketMetadata operator+(const FSocketMetadata& Other) const { return FSocketMetadata{}; }

		FSocketMetadata operator-(const FSocketMetadata& Other) const { return FSocketMetadata{}; }

		FSocketMetadata operator*(const FSocketMetadata& Scalar) const { return FSocketMetadata{}; }

		FSocketMetadata operator*(float Scalar) const { return FSocketMetadata{}; }

		FSocketMetadata operator/(const FSocketMetadata& Scalar) const { return FSocketMetadata{}; }

		bool operator!=(const FSocketMetadata& Other) const { return !(*this == Other); }

		bool operator<(const FSocketMetadata& Other) const { return false; }
		
		*/

		bool operator==(const FSocketMetadata& Other) const
		{
			return Index == Other.Index && EntryKey == Other.EntryKey && RelationType == Other.RelationType;
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FModifier : public PCGEx::FLocalSingleComponentInput
	{
		FModifier(): PCGEx::FLocalSingleComponentInput()
		{
		}

		FModifier(const FPCGExSocketDescriptor& InDescriptor): PCGEx::FLocalSingleComponentInput()
		{
			Descriptor = static_cast<FPCGExInputSelector>(InDescriptor.AttributeModifier);
			bEnabled = InDescriptor.bApplyAttributeModifier;
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FLocalDirection : public PCGEx::FLocalDirectionInput
	{
		FLocalDirection(): PCGEx::FLocalDirectionInput()
		{
		}

		FLocalDirection(const FPCGExSocketDescriptor& InDescriptor): PCGEx::FLocalDirectionInput()
		{
			Descriptor = static_cast<FPCGExInputSelector>(InDescriptor.AttributeDirectionVector);
			bEnabled = InDescriptor.bDirectionVectorFromAttribute;
		}
	};

	const FName SocketPropertyNameIndex = FName("Index");
	const FName SocketPropertyNameRelationType = FName("RelationType");
	const FName SocketPropertyNameEntryKey = FName("EntryKey");

	struct PCGEXTENDEDTOOLKIT_API FSocket
	{
		FSocket()
		{
			MatchingSockets.Empty();
		}

		FSocket(const FPCGExSocketDescriptor& InDescriptor): FSocket()
		{
			Descriptor = InDescriptor;
			Descriptor.Direction.DotTolerance = FMath::Cos(Descriptor.Direction.Cone * (PI / 180.0)); //Degrees to dot product
		}

		friend struct FSocketMapping;
		FPCGExSocketDescriptor Descriptor;

		int32 SocketIndex = -1;
		TSet<int32> MatchingSockets;

	protected:
		FPCGMetadataAttribute<int64>* AttributeIndex = nullptr;
		FPCGMetadataAttribute<int32>* AttributeRelationType = nullptr;
		FPCGMetadataAttribute<int64>* AttributeEntryKey = nullptr;
		FName AttributeNameBase = NAME_None;

	public:
		FName GetName() const { return AttributeNameBase; }

		void DeleteFrom(UPCGPointData* PointData) const
		{
			PointData->Metadata->DeleteAttribute(AttributeIndex->Name);
			PointData->Metadata->DeleteAttribute(AttributeRelationType->Name);
			PointData->Metadata->DeleteAttribute(AttributeEntryKey->Name);
		}
		/**
		 * Find or create the attribute matching this socket on a given PointData object,
		 * as well as prepare the scape modifier for that same object.
		 * @param PointData 
		 */
		void PrepareForPointData(const UPCGPointData* PointData)
		{
			AttributeIndex = GetAttribute(PointData, SocketPropertyNameIndex, static_cast<int64>(-1));
			AttributeRelationType = GetAttribute(PointData, SocketPropertyNameRelationType, static_cast<int32>(EPCGExRelationType::Unknown));
			AttributeEntryKey = GetAttribute(PointData, SocketPropertyNameEntryKey, PCGInvalidEntryKey);
		}

	protected:
		template <typename T>
		FPCGMetadataAttribute<T>* GetAttribute(const UPCGPointData* PointData, const FName PropertyName, T DefaultValue)
		{
			return PointData->Metadata->FindOrCreateAttribute<T>(GetSocketPropertyName(PropertyName), DefaultValue);
		}

	public:
		void SetData(const PCGMetadataEntryKey MetadataEntry, const FSocketMetadata& SocketMetadata) const
		{
			SetRelationIndex(MetadataEntry, SocketMetadata.Index);
			SetRelationType(MetadataEntry, SocketMetadata.RelationType);
		}

		// Point index within the same data group.
		void SetRelationIndex(const PCGMetadataEntryKey MetadataEntry, int64 InIndex) const { AttributeIndex->SetValue(MetadataEntry, InIndex); }
		int64 GetRelationIndex(const PCGMetadataEntryKey MetadataEntry) const { return AttributeIndex->GetValueFromItemKey(MetadataEntry); }

		// Point metadata entry key, faster than retrieving index if you only need to access attributes
		void SetRelationEntryKey(const PCGMetadataEntryKey MetadataEntry, PCGMetadataEntryKey InEntryKey) const { AttributeEntryKey->SetValue(MetadataEntry, InEntryKey); }
		PCGMetadataEntryKey GetRelationEntryKey(const PCGMetadataEntryKey MetadataEntry) const { return AttributeEntryKey->GetValueFromItemKey(MetadataEntry); }

		// Relation type
		void SetRelationType(const PCGMetadataEntryKey MetadataEntry, EPCGExRelationType InRelationType) const { AttributeRelationType->SetValue(MetadataEntry, static_cast<int32>(InRelationType)); }
		EPCGExRelationType GetRelationType(const PCGMetadataEntryKey MetadataEntry) const { return static_cast<EPCGExRelationType>(AttributeRelationType->GetValueFromItemKey(MetadataEntry)); }

		FSocketMetadata GetData(const PCGMetadataEntryKey MetadataEntry) const
		{
			return FSocketMetadata(
					GetRelationIndex(MetadataEntry),
					GetRelationEntryKey(MetadataEntry),
					GetRelationType(MetadataEntry)
				);
		}

		FName GetSocketPropertyName(FName PropertyName) const
		{
			const FString Separator = TEXT("/");
			return *(AttributeNameBase.ToString() + Separator + PropertyName.ToString());
		}

	public:
		~FSocket()
		{
			AttributeIndex = nullptr;
			AttributeRelationType = nullptr;
			AttributeEntryKey = nullptr;
			MatchingSockets.Empty();
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FSocketInfos
	{
		FSocket* Socket = nullptr;
		FModifier* Modifier = nullptr;
		FLocalDirection* LocalDirection = nullptr;
	};

	struct PCGEXTENDEDTOOLKIT_API FSocketMapping
	{
		FSocketMapping()
		{
			Reset();
		}

	public:
		FName Identifier = NAME_None;
		TArray<FSocket> Sockets;
		TArray<FModifier> Modifiers;
		TArray<FLocalDirection> LocalDirections;
		TMap<FName, int32> NameToIndexMap;
		int32 NumSockets = 0;

		void Initialize(FName InIdentifier, TArray<FPCGExSocketDescriptor>& InSockets)
		{
			Reset();
			Identifier = InIdentifier;
			for (FPCGExSocketDescriptor& Descriptor : InSockets)
			{
				if (!Descriptor.bEnabled) { continue; }

				FModifier& NewModifier = Modifiers.Emplace_GetRef(Descriptor);
				FLocalDirection& NewLocalDirection = LocalDirections.Emplace_GetRef(Descriptor);

				FSocket& NewSocket = Sockets.Emplace_GetRef(Descriptor);
				NewSocket.AttributeNameBase = GetCompoundName(Descriptor.SocketName);
				NewSocket.SocketIndex = NumSockets;
				NameToIndexMap.Add(NewSocket.GetName(), NewSocket.SocketIndex);
				NumSockets++;
			}

			PostProcessSockets();
		}

		void InitializeWithOverrides(FName InIdentifier, TArray<FPCGExSocketDescriptor>& InSockets, const FPCGExSocketGlobalOverrides& Overrides)
		{
			Reset();
			Identifier = InIdentifier;
			const FString PCGExName = TEXT("PCGEx");
			for (FPCGExSocketDescriptor& Descriptor : InSockets)
			{
				if (!Descriptor.bEnabled) { continue; }

				FModifier& NewModifier = Modifiers.Emplace_GetRef(Descriptor);
				NewModifier.bEnabled = Overrides.bOverrideAttributeModifier ? Overrides.bApplyAttributeModifier : Descriptor.bApplyAttributeModifier;
				NewModifier.Descriptor = static_cast<FPCGExInputSelector>(Overrides.bOverrideAttributeModifier ? Overrides.AttributeModifier : Descriptor.AttributeModifier);

				FLocalDirection& NewLocalDirection = LocalDirections.Emplace_GetRef(Descriptor);
				NewLocalDirection.bEnabled = Overrides.bOverrideDirectionVectorFromAttribute ? Overrides.bDirectionVectorFromAttribute : Descriptor.bDirectionVectorFromAttribute;
				NewLocalDirection.Descriptor = Overrides.bOverrideDirectionVectorFromAttribute ? Overrides.AttributeDirectionVector : Descriptor.AttributeDirectionVector;

				FSocket& NewSocket = Sockets.Emplace_GetRef(Descriptor);
				NewSocket.AttributeNameBase = GetCompoundName(Descriptor.SocketName);
				NewSocket.SocketIndex = NumSockets;
				NameToIndexMap.Add(NewSocket.GetName(), NewSocket.SocketIndex);

				if (Overrides.bOverrideRelativeOrientation)
				{
					NewSocket.Descriptor.bRelativeOrientation = Overrides.bRelativeOrientation;
				}

				if (Overrides.bOverrideCone)
				{
					NewSocket.Descriptor.Direction.Cone = Overrides.Cone;
				}

				if (Overrides.bOverrideMaxDistance)
				{
					NewSocket.Descriptor.Direction.MaxDistance = Overrides.MaxDistance;
				}

				NewSocket.Descriptor.Direction.DotTolerance = FMath::Cos(Descriptor.Direction.Cone * (PI / 180.0));

				NumSockets++;
			}

			PostProcessSockets();
		}

		FName GetCompoundName(FName SecondaryIdentifier)
		{
			const FString Separator = TEXT("/");
			return *(TEXT("PCGEx") + Separator + Identifier.ToString() + Separator + SecondaryIdentifier.ToString()); // PCGEx/ParamsIdentifier/SocketIdentifier
		}

		/**
		 * Prepare socket mapping for working with a given PointData object.
		 * Each socket will cache Attribute & accessors
		 * @param PointData 
		 */
		void PrepareForPointData(const UPCGPointData* PointData)
		{
			for (int i = 0; i < Sockets.Num(); i++)
			{
				Sockets[i].PrepareForPointData(PointData);
				Modifiers[i].Validate(PointData);
				LocalDirections[i].Validate(PointData);
			}
		}

		const TArray<FSocket>& GetSockets() const { return Sockets; }
		const TArray<FModifier>& GetModifiers() const { return Modifiers; }

		void GetSocketsInfos(TArray<FSocketInfos>& OutInfos)
		{
			OutInfos.Empty(NumSockets);
			for (int i = 0; i < NumSockets; i++)
			{
				FSocketInfos& Infos = OutInfos.Emplace_GetRef();
				Infos.Socket = &(Sockets[i]);
				Infos.Modifier = &(Modifiers[i]);
				Infos.LocalDirection = &(LocalDirections[i]);
			}
		}

		void Reset()
		{
			Sockets.Empty();
			Modifiers.Empty();
			LocalDirections.Empty();
		}

	private:
		/**
		 * Build matching set
		 */
		void PostProcessSockets()
		{
			for (FSocket& Socket : Sockets)
			{
				for (const FName MatchingSocketName : Socket.Descriptor.MatchingSlots)
				{
					FName OtherSocketName = GetCompoundName(MatchingSocketName);
					if (const int32* Index = NameToIndexMap.Find(OtherSocketName))
					{
						Socket.MatchingSockets.Add(*Index);
						if (Socket.Descriptor.bMirrorMatchingSockets) { Sockets[*Index].MatchingSockets.Add(Socket.SocketIndex); }
					}
				}
			}
		}

	public:
		~FSocketMapping()
		{
			Reset();
		}
	};
}

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExRelationsParamsData : public UPCGPointData
{
	GENERATED_BODY()

public:
	UPCGExRelationsParamsData(const FObjectInitializer& ObjectInitializer);

	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Param; }

	/**
	 * 
	 * @param PointData 
	 * @return 
	 */
	bool HasMatchingRelationsData(const UPCGPointData* PointData);

public:
	UPROPERTY(BlueprintReadOnly)
	FName RelationIdentifier = "RelationIdentifier";

	UPROPERTY(BlueprintReadOnly)
	double GreatestStaticMaxDistance = 0.0;

	UPROPERTY(BlueprintReadOnly)
	bool bHasVariableMaxDistance = false;

	FName CachedIndexAttributeName;

protected:
	PCGExRelational::FSocketMapping SocketMapping;

public:
	const PCGExRelational::FSocketMapping* GetSocketMapping() const { return &SocketMapping; }

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
	 */
	void PrepareForPointData(FPCGExRelationsProcessorContext* Context, const UPCGPointData* PointData);

	/**
		 * Fills an array in order with each' socket metadata registered for a given point.
		 * Make sure to call PrepareForPointData first.
		 * @param MetadataEntry 
		 * @param OutMetadata 
		 */
	void GetSocketsData(const PCGMetadataEntryKey MetadataEntry, TArray<PCGExRelational::FSocketMetadata>& OutMetadata) const;

	/**
	 * Make sure InMetadata has the same length as the nu
	 * @param MetadataEntry 
	 * @param InMetadata 
	 */
	void SetSocketsData(const PCGMetadataEntryKey MetadataEntry, TArray<PCGExRelational::FSocketMetadata>& InMetadata);

	void GetSocketsInfos(TArray<PCGExRelational::FSocketInfos>& OutInfos);
};
