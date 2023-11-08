// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "PCGParamData.h"
#include "PCGExRelationsParamsData.generated.h"

class UPCGExRelationsParamsBuilderSettings;
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

	FPCGExSocketDirection(const FPCGExSocketDirection& Other):
		Direction(Other.Direction),
		DotTolerance(Other.DotTolerance),
		MaxDistance(Other.MaxDistance)
	{
	}

public:
	/** Slot 'look-at' direction. Used along with DotTolerance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector Direction = FVector::UpVector;

	/** Tolerance threshold. Used along with the direction of the slot when looking for the closest candidate. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	double DotTolerance = 0.707f; // 45deg

	/** Maximum sampling distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	double MaxDistance = 1000.0f;
};

#pragma region Descriptors

// Editable Property/Attribute selector
USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSocketModifierDescriptor : public FPCGExSelectorSettingsBase
{
	GENERATED_BODY()

	FPCGExSocketModifierDescriptor(): FPCGExSelectorSettingsBase()
	{
	}

	FPCGExSocketModifierDescriptor(const FPCGExSocketModifierDescriptor& Other): FPCGExSelectorSettingsBase(Other)
	{
	}
};

// Editable relation slot
USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSocketDescriptor
{
	GENERATED_BODY()

public:
	/** Name of the attribute to write neighbor index to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName AttributeName = NAME_None;

	/** Relation direction in space. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ShowOnlyInnerProperties))
	FPCGExSocketDirection Direction;

	/** Whether the orientation of the direction is relative to the point or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bRelativeOrientation = true;

	/** Is the distance modified by local attributes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bApplyAttributeModifier = false;

	/** Which local attribute is used to factor the distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bApplyAttributeModifier", EditConditionHides))
	FPCGExSocketModifierDescriptor AttributeModifier;

	/** Whether this slot is enabled or not. Handy to do trial-and-error without adding/deleting array elements. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(AdvancedDisplay))
	bool bEnabled = true;

	/** Debug color. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(AdvancedDisplay))
	FColor DebugColor = FColor::Red;
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

		bool operator==(const FSocketData& Other) const
		{
			return Index == Other.Index && IndexedDot == Other.IndexedDot && IndexedDistance == Other.IndexedDistance;
		}

		operator FVector4() const
		{
			return FVector4(Index, IndexedDot, IndexedDistance, 0.0);
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FModifier
	{
	public:
		bool bEnabled = false;
		bool bValid = false;
		FPCGExSocketModifierDescriptor* Descriptor = nullptr;
		FPCGAttributePropertyInputSelector Selector;

		/**
		 * Build and validate a property/attribute accessor for the selected
		 * @param PointData 
		 */
		void PrepareForPoints(const UPCGPointData* PointData)
		{
			if (!bEnabled || !Descriptor)
			{
				bValid = false;
				return;
			}

			bValid = Descriptor->CopyAndFixLast(PointData);
		}

		double GetScale(const FPCGPoint& Point)
		{
			if (!bValid || !bEnabled) { return 1.0; }

			switch (Selector.GetSelection())
			{
			case EPCGAttributePropertySelection::Attribute:
				return PCGMetadataAttribute::CallbackWithRightType(Descriptor->Attribute->GetTypeId(), [this, &Point]<typename T>(T DummyValue) -> double
				{
					FPCGMetadataAttribute<T>* Attribute = static_cast<FPCGMetadataAttribute<T>*>(Descriptor->Attribute);
					return GetScaleFactor(Attribute->GetValue(Point.MetadataEntry));
				});
#define PCGEX_SCALE_BY_ACCESSOR(_ENUM, _ACCESSOR) case _ENUM: return GetScaleFactor(Point._ACCESSOR);
			case EPCGAttributePropertySelection::PointProperty:
				switch (Selector.GetPointProperty())
				{
				PCGEX_FOREACH_POINTPROPERTY(PCGEX_SCALE_BY_ACCESSOR)
				}
				break;
			case EPCGAttributePropertySelection::ExtraProperty:
				switch (Selector.GetExtraProperty())
				{
				PCGEX_FOREACH_POINTEXTRAPROPERTY(PCGEX_SCALE_BY_ACCESSOR)
				}
				break;
			}

			return 1.0;

#undef PCGEX_SCALE_BY_ACCESSOR
		}

	protected:
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

	public:
		~FModifier()
		{
			Descriptor = nullptr;
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FSocket
	{
	public:
		FPCGExSocketDescriptor* Descriptor = nullptr;
		FName AttributeName = NAME_None;
		FPCGMetadataAttribute<FVector4>* SocketDataAttribute = nullptr;
		FModifier* ScaleModifier = nullptr;

		/**
		 * Find or create the attribute matching this socket on a given PointData object,
		 * as well as prepare the scape modifier for that same object.
		 * @param PointData 
		 */
		void PrepareForPointData(UPCGPointData* PointData)
		{
			SocketDataAttribute = PointData->Metadata->FindOrCreateAttribute<FVector4>(AttributeName, FVector4(-1.0, 0.0, 0.0, 0.0), false, true, true);
			ScaleModifier->PrepareForPoints(PointData);
		}

		/**
		 * Return the socket details stored as FVector4
		 * X = Index, Y, Z and W are reserved for later use.
		 * This is mostly to circumvent the fact that we can't have custom FPCGMetadataAttribute<T> outside of a list of primitive types.
		 * @param Key
		 * @return 
		 */
		FVector4 GetValue(const int64 Key) const { return SocketDataAttribute->GetValue(Key); }
		void SetValue(const int64 Key, const FVector4& Value) const { SocketDataAttribute->SetValue(Key, Value); }

		FSocketData GetSocketData(const int64 Key) const { return SocketDataAttribute->GetValue(Key); }

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
			Descriptor = nullptr;
			SocketDataAttribute = nullptr;
			ScaleModifier = nullptr;
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
		TMap<FName, FSocket*> SocketMap;

		void Initialize(FName Identifier, TArray<FPCGExSocketDescriptor>& InSockets)
		{
			Reset();
			const FString PCGExName = TEXT("PCGEx");
			const FString BaseIdentifierStr = Identifier.ToString(), Dash = TEXT("__"), Slash = TEXT("/");
			for (FPCGExSocketDescriptor& Descriptor : InSockets)
			{
				if (!Descriptor.bEnabled) { continue; }

				FModifier& NewModifier = Modifiers.Emplace_GetRef();
				NewModifier.bEnabled = Descriptor.bApplyAttributeModifier;
				NewModifier.Descriptor = &Descriptor.AttributeModifier;

				FSocket& NewSocket = Sockets.Emplace_GetRef();
				NewSocket.Descriptor = &Descriptor;
				NewSocket.AttributeName = *(PCGExName + Slash + BaseIdentifierStr + Slash + Descriptor.AttributeName.ToString()); // PCGEx/RelationIdentifier/SocketName
				NewSocket.ScaleModifier = &NewModifier;
				SocketMap.Add(Descriptor.AttributeName, &NewSocket);
			}
		}

		/**
		 * Prepare socket mapping for working with a given PointData object.
		 * Each socket will cache Attribute & accessors
		 * @param PointData 
		 */
		void PrepareForPointData(UPCGPointData* PointData)
		{
			for (FSocket& Socket : Sockets) { Socket.PrepareForPointData(PointData); }
		}

		TArray<FSocket>& GetSockets() { return Sockets; }
		TArray<FModifier>& GetModifiers() { return Modifiers; }

		void Reset()
		{
			Sockets.Empty();
			Modifiers.Empty();
			SocketMap.Empty();
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

	// Checks whether the data have the matching attribute
	bool HasMatchingRelationsData(UPCGPointData* PointData);

public:
	UPROPERTY(BlueprintReadOnly)
	FName RelationIdentifier = "RelationIdentifier";

	UPROPERTY(BlueprintReadOnly)
	double GreatestStaticMaxDistance = 0.0;

	UPROPERTY(BlueprintReadOnly)
	bool bMarkMutualRelations = true;

	UPROPERTY(BlueprintReadOnly)
	bool bHasVariableMaxDistance = false;

protected:
	PCGExRelational::FSocketMapping SocketMapping = PCGExRelational::FSocketMapping{};

public:
	const PCGExRelational::FSocketMapping* GetSocketMapping() const { return &SocketMapping; }

	/**
	 * Initialize this data object from a list of socket descriptors
	 * @param InSockets 
	 */
	virtual void InitializeSockets(TArray<FPCGExSocketDescriptor>& InSockets);

	/**
	 * Mute internal socket mapping to read from a given PointData object
	 * @param PointData 
	 * @return 
	 */
	void PrepareForPointData(UPCGPointData* PointData);
};
