// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "PCGParamData.h"
#include "Math/UnrealMathUtility.h"
#include "PCGExLocalAttributeHelpers.h"
#include "PCGExRelationsParamsData.generated.h"

namespace PCGExRelational
{
	struct FModifier;
}

class UPCGExCreateRelationsParamsSettings;
class UPCGPointData;

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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(Units="Degrees"))
	double Cone = 45.0; // 0.707f dot

	UPROPERTY(BlueprintReadOnly)
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
	struct PCGEXTENDEDTOOLKIT_API FSocketData
	{
		FSocketData()
		{
		}

		FSocketData(int64 InIndex, double InIndexedDot, double InIndexedDistance)
		{
			Index = InIndex;
			IndexedDot = InIndexedDot;
			IndexedDistance = InIndexedDistance;
		}

		FSocketData(const FVector4& InVector)
			: Index(InVector.X), IndexedDot(InVector.Y), IndexedDistance(InVector.Z) //, W(InVector.W)
		{
		}

	public:
		int64 Index = -1;
		double IndexedDot = -1.0;
		double IndexedDistance = TNumericLimits<double>::Max();

		operator FVector4() const
		{
			return FVector4(Index, IndexedDot, IndexedDistance, 0.0);
		}

		friend FArchive& operator<<(FArchive& Ar, FSocketData& Data)
		{
			Ar << Data.Index;
			Ar << Data.IndexedDot;
			Ar << Data.IndexedDistance;
			return Ar;
		}

		FSocketData operator+(const FSocketData& Other) const { return FSocketData{}; }

		FSocketData operator-(const FSocketData& Other) const { return FSocketData{}; }

		FSocketData operator*(const FSocketData& Scalar) const { return FSocketData{}; }

		FSocketData operator*(float Scalar) const { return FSocketData{}; }

		FSocketData operator/(const FSocketData& Scalar) const { return FSocketData{}; }

		bool operator==(const FSocketData& Other) const
		{
			return Index == Other.Index && IndexedDot == Other.IndexedDot && IndexedDistance == Other.IndexedDistance;
		}

		bool operator!=(const FSocketData& Other) const { return !(*this == Other); }

		bool operator<(const FSocketData& Other) const { return false; }
	};

	struct PCGEXTENDEDTOOLKIT_API FModifier : public PCGEx::FLocalSingleComponentInput
	{
		FModifier(): PCGEx::FLocalSingleComponentInput()
		{
		}

		FModifier(FPCGExSocketDescriptor& InDescriptor): PCGEx::FLocalSingleComponentInput()
		{
			Descriptor = InDescriptor.AttributeModifier;
			bEnabled = InDescriptor.bApplyAttributeModifier;
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FLocalDirection : public PCGEx::FLocalDirectionInput
	{
		FLocalDirection(): PCGEx::FLocalDirectionInput()
		{
		}

		FLocalDirection(FPCGExSocketDescriptor& InDescriptor): PCGEx::FLocalDirectionInput()
		{
			Descriptor = static_cast<FPCGExInputSelector>(InDescriptor.AttributeDirectionVector);
			bEnabled = InDescriptor.bDirectionVectorFromAttribute;
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FSocket
	{
		FSocket()
		{
		}

		FSocket(FPCGExSocketDescriptor& InDescriptor)
		{
			Descriptor = InDescriptor;
			Descriptor.Direction.DotTolerance = FMath::Cos(Descriptor.Direction.Cone * (PI / 180.0)); //Degrees to dot product
		}

		friend struct FSocketMapping;
		FPCGExSocketDescriptor Descriptor;
		FPCGMetadataAttribute<FVector4>* SocketDataAttribute = nullptr;

	protected:
		FName AttributeName = NAME_None;

	public:
		FName GetName() const { return AttributeName; }
		/**
		 * Find or create the attribute matching this socket on a given PointData object,
		 * as well as prepare the scape modifier for that same object.
		 * @param PointData 
		 */
		void PrepareForPointData(UPCGPointData* PointData)
		{
			SocketDataAttribute = PointData->Metadata->FindOrCreateAttribute<FVector4>(AttributeName, FVector4(-1.0, 0.0, 0.0, 0.0), false, true, true);
		}

		/**
		 * Return the socket details stored as FVector4
		 * X = Index, Y, Z and W are reserved for later use.
		 * This is mostly to circumvent the fact that we can't have custom FPCGMetadataAttribute<T> outside of a list of primitive types.
		 * @param MetadataEntry
		 * @return 
		 */
		FSocketData GetValue(const PCGMetadataEntryKey MetadataEntry) const
		{
			return SocketDataAttribute->GetValueFromItemKey(MetadataEntry);
		}

		void SetValue(const PCGMetadataEntryKey MetadataEntry, const FSocketData& Value) const
		{
			SocketDataAttribute->SetValue(MetadataEntry, Value);
		}

		FSocketData GetSocketData(const PCGMetadataEntryKey MetadataEntry) const
		{
			return SocketDataAttribute->GetValueFromItemKey(MetadataEntry);
		}

		/**
		 * Returns the point pluggued in this socket for a given Origin
		 * @param Origin 
		 * @param Points 
		 * @return 
		 */
		const FPCGPoint* GetSocketContent(const FPCGPoint& Origin, const TArray<FPCGPoint>& Points) const
		{
			const FVector4 Details = GetValue(Origin.MetadataEntry);
			const int32 Index = FMath::RoundToInt32(Details.X);
			if (Index < 0) { return nullptr; }
			return &Points[Index];
		}

	public:
		~FSocket()
		{
			SocketDataAttribute = nullptr;
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FSocketMapping
	{
		FSocketMapping()
		{
			Reset();
		}

	public:
		TArray<FSocket> Sockets;
		TArray<FModifier> Modifiers;
		TArray<FLocalDirection> LocalDirections;
		int32 NumSockets = 0;

		void Initialize(FName Identifier, TArray<FPCGExSocketDescriptor>& InSockets)
		{
			Reset();
			for (FPCGExSocketDescriptor& Descriptor : InSockets)
			{
				if (!Descriptor.bEnabled) { continue; }

				FModifier& NewModifier = Modifiers.Emplace_GetRef(Descriptor);
				FLocalDirection& NewLocalDirection = LocalDirections.Emplace_GetRef(Descriptor);
				FSocket& NewSocket = Sockets.Emplace_GetRef(Descriptor);
				NewSocket.AttributeName = GetSocketName(Identifier, Descriptor.SocketName);
				NumSockets++;
			}
		}

		void InitializeWithOverrides(FName Identifier, TArray<FPCGExSocketDescriptor>& InSockets, FPCGExSocketGlobalOverrides& Overrides)
		{
			Reset();
			const FString PCGExName = TEXT("PCGEx");
			for (FPCGExSocketDescriptor& Descriptor : InSockets)
			{
				if (!Descriptor.bEnabled) { continue; }

				FModifier& NewModifier = Modifiers.Emplace_GetRef(Descriptor);
				NewModifier.bEnabled = Overrides.bOverrideAttributeModifier ? Overrides.bApplyAttributeModifier : Descriptor.bApplyAttributeModifier;
				NewModifier.Descriptor = Overrides.bOverrideAttributeModifier ? Overrides.AttributeModifier : Descriptor.AttributeModifier;

				FLocalDirection& NewLocalDirection = LocalDirections.Emplace_GetRef(Descriptor);
				NewLocalDirection.bEnabled = Overrides.bOverrideDirectionVectorFromAttribute ? Overrides.bDirectionVectorFromAttribute : Descriptor.bDirectionVectorFromAttribute;
				NewLocalDirection.Descriptor = Overrides.bOverrideDirectionVectorFromAttribute ? Overrides.AttributeDirectionVector : Descriptor.AttributeDirectionVector;

				FSocket& NewSocket = Sockets.Emplace_GetRef(Descriptor);

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

				NewSocket.AttributeName = GetSocketName(Identifier, Descriptor.SocketName);
				NumSockets++;
			}
		}

		static FName GetSocketName(FName ParamsIdentifier, FName SocketIdentifier)
		{
			const FString Separator = TEXT("/");
			return *(TEXT("PCGEx") + Separator + ParamsIdentifier.ToString() + Separator + SocketIdentifier.ToString()); // PCGEx/ParamsIdentifier/SocketIdentifier
		}

		/**
		 * Prepare socket mapping for working with a given PointData object.
		 * Each socket will cache Attribute & accessors
		 * @param PointData 
		 */
		void PrepareForPointData(UPCGPointData* PointData)
		{
			for (int i = 0; i < Sockets.Num(); i++)
			{
				Sockets[i].PrepareForPointData(PointData);
				Modifiers[i].PrepareForPointData(PointData);
				LocalDirections[i].PrepareForPointData(PointData);
			}
		}

		const TArray<FSocket>& GetSockets() const { return Sockets; }
		const TArray<FModifier>& GetModifiers() const { return Modifiers; }

		void Reset()
		{
			Sockets.Empty();
			Modifiers.Empty();
			LocalDirections.Empty();
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
UCLASS(BlueprintType, ClassGroup = (Procedural))
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
	bool HasMatchingRelationsData(UPCGPointData* PointData);

public:
	UPROPERTY(BlueprintReadOnly)
	FName RelationIdentifier = "RelationIdentifier";

	UPROPERTY(BlueprintReadOnly)
	double GreatestStaticMaxDistance = 0.0;

	UPROPERTY(BlueprintReadOnly)
	bool bHasVariableMaxDistance = false;

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
	virtual void InitializeSockets(
		TArray<FPCGExSocketDescriptor>& InSockets,
		const bool bApplyOverrides,
		FPCGExSocketGlobalOverrides& Overrides);

	/**
	 * Prepare socket mapping for working with a given PointData object.
	 * @param PointData 
	 */
	void PrepareForPointData(UPCGPointData* PointData);
};
