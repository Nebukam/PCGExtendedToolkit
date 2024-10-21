// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "PCGExMacros.h"
#include "PCGEx.h"
#include "PCGExHelpers.h"
#include "PCGExMath.h"
#include "PCGExPointIO.h"

#include "Metadata/Accessors/PCGAttributeAccessor.h"

#include "PCGExAttributeHelpers.generated.h"

#pragma region Input Configs

struct FPCGExAttributeGatherDetails;

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExInputConfig
{
	GENERATED_BODY()

	FPCGExInputConfig()
	{
	}

	explicit FPCGExInputConfig(const FPCGAttributePropertyInputSelector& InSelector)
	{
		Selector.ImportFromOtherSelector(InSelector);
	}

	explicit FPCGExInputConfig(const FPCGExInputConfig& Other)
		: Attribute(Other.Attribute)
	{
		Selector.ImportFromOtherSelector(Other.Selector);
	}

	explicit FPCGExInputConfig(const FName InName)
	{
		Selector.Update(InName.ToString());
	}

	virtual ~FPCGExInputConfig() = default;
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (HideInDetailPanel, Hidden, EditConditionHides, EditCondition="false"))
	FString TitlePropertyName;

	/** Attribute or $Property. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayPriority=0))
	FPCGAttributePropertyInputSelector Selector;

	FPCGMetadataAttributeBase* Attribute = nullptr;
	int16 UnderlyingType = static_cast<int16>(EPCGMetadataTypes::Unknown);

	FPCGAttributePropertyInputSelector& GetMutableSelector() { return Selector; }

	EPCGAttributePropertySelection GetSelection() const { return Selector.GetSelection(); }
	FName GetName() const { return Selector.GetName(); }
#if WITH_EDITOR
	virtual FString GetDisplayName() const;
	void UpdateUserFacingInfos();
#endif
	/**
	 * Bind & cache the current selector for a given UPCGPointData
	 * @param InData 
	 * @return 
	 */
	virtual bool Validate(const UPCGPointData* InData);
	FString ToString() const { return GetName().ToString(); }
};

#pragma endregion

namespace PCGEx
{
	static FPCGAttributePropertyInputSelector CopyAndFixLast(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData, TArray<FString>& OutExtraNames)
	{
		//Copy, fix, and clear ExtraNames to support PCGEx custom selectors
		FPCGAttributePropertyInputSelector NewSelector = InSelector.CopyAndFixLast(InData);
		OutExtraNames = NewSelector.GetExtraNames();
		switch (NewSelector.GetSelection())
		{
		default: ;
		case EPCGAttributePropertySelection::Attribute:
			NewSelector.SetAttributeName(NewSelector.GetAttributeName(), true);
			break;
		case EPCGAttributePropertySelection::PointProperty:
			NewSelector.SetPointProperty(NewSelector.GetPointProperty(), true);
			break;
		case EPCGAttributePropertySelection::ExtraProperty:
			NewSelector.SetExtraProperty(NewSelector.GetExtraProperty(), true);
			break;
		}
		return NewSelector;
	}

#pragma region Attribute identity

	struct /*PCGEXTENDEDTOOLKIT_API*/ FAttributeIdentity
	{
		const FName Name = NAME_None;
		const EPCGMetadataTypes UnderlyingType = EPCGMetadataTypes::Unknown;
		const bool bAllowsInterpolation = true;

		FAttributeIdentity(const FAttributeIdentity& Other)
			: Name(Other.Name), UnderlyingType(Other.UnderlyingType), bAllowsInterpolation(Other.bAllowsInterpolation)
		{
		}

		FAttributeIdentity(const FName InName, const EPCGMetadataTypes InUnderlyingType, const bool AllowsInterpolation)
			: Name(InName), UnderlyingType(InUnderlyingType), bAllowsInterpolation(AllowsInterpolation)
		{
		}

		int16 GetTypeId() const { return static_cast<int16>(UnderlyingType); }
		bool IsA(const int16 InType) const { return GetTypeId() == InType; }
		bool IsA(const EPCGMetadataTypes InType) const { return UnderlyingType == InType; }

		FString GetDisplayName() const { return FString(Name.ToString() + FString::Printf(TEXT("( %d )"), UnderlyingType)); }
		bool operator==(const FAttributeIdentity& Other) const { return Name == Other.Name; }

		static void Get(const UPCGMetadata* InMetadata, TArray<FAttributeIdentity>& OutIdentities);
		static void Get(const UPCGMetadata* InMetadata, TArray<FName>& OutNames, TMap<FName, FAttributeIdentity>& OutIdentities);
	};

	struct FAttributesInfos
	{
		TMap<FName, int32> Map;
		TArray<FAttributeIdentity> Identities;
		TArray<FPCGMetadataAttributeBase*> Attributes;

		bool Contains(FName AttributeName, EPCGMetadataTypes Type);
		bool Contains(FName AttributeName);
		FAttributeIdentity* Find(FName AttributeName);

		bool FindMissing(const TSet<FName>& Checklist, TSet<FName>& OutMissing);
		bool FindMissing(const TArray<FName>& Checklist, TSet<FName>& OutMissing);

		void Append(const TSharedPtr<FAttributesInfos>& Other, const FPCGExAttributeGatherDetails& InGatherDetails, TSet<FName>& OutTypeMismatch);
		void Append(const TSharedPtr<FAttributesInfos>& Other, TSet<FName>& OutTypeMismatch, const TSet<FName>* InIgnoredAttributes = nullptr);
		void Update(const FAttributesInfos* Other, const FPCGExAttributeGatherDetails& InGatherDetails, TSet<FName>& OutTypeMismatch);

		~FAttributesInfos() = default;

		static TSharedPtr<FAttributesInfos> Get(const UPCGMetadata* InMetadata, const TSet<FName>* IgnoredAttributes = nullptr);
		static TSharedPtr<FAttributesInfos> Get(const TSharedPtr<PCGExData::FPointIOCollection>& InCollection, TSet<FName>& OutTypeMismatch, const TSet<FName>* IgnoredAttributes = nullptr);
	};

	static void GatherAttributes(
		const TSharedPtr<FAttributesInfos>& OutInfos, const FPCGContext* InContext, const FName InputLabel,
		const FPCGExAttributeGatherDetails& InDetails, TSet<FName>& Mismatches)
	{
		TArray<FPCGTaggedData> InputData = InContext->InputData.GetInputsByPin(InputLabel);
		for (const FPCGTaggedData& TaggedData : InputData)
		{
			if (const UPCGParamData* AsParamData = Cast<UPCGParamData>(TaggedData.Data))
			{
				OutInfos->Append(FAttributesInfos::Get(AsParamData->Metadata), InDetails, Mismatches);
			}
			else if (const UPCGSpatialData* AsSpatialData = Cast<UPCGSpatialData>(TaggedData.Data))
			{
				OutInfos->Append(FAttributesInfos::Get(AsSpatialData->Metadata), InDetails, Mismatches);
			}
		}
	}

	static TSharedPtr<FAttributesInfos> GatherAttributes(
		const FPCGContext* InContext, const FName InputLabel,
		const FPCGExAttributeGatherDetails& InDetails, TSet<FName>& Mismatches)
	{
		TSharedPtr<FAttributesInfos> OutInfos = MakeShared<FAttributesInfos>();
		GatherAttributes(OutInfos, InContext, InputLabel, InDetails, Mismatches);
		return OutInfos;
	}

#pragma endregion

#pragma region Local Attribute Inputs

	class /*PCGEXTENDEDTOOLKIT_API*/ FAttributeBroadcasterBase
	{
	public:
		virtual ~FAttributeBroadcasterBase() = default;
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TAttributeBroadcaster : public FAttributeBroadcasterBase
	{
	protected:
		TSharedPtr<PCGExData::FPointIO> PointIO;
		bool bMinMaxDirty = true;
		bool bNormalized = false;
		FPCGAttributePropertyInputSelector InternalSelector;
		TUniquePtr<IPCGAttributeAccessor> InternalAccessor;

		EPCGPointProperties PointProperty = EPCGPointProperties::Position;
		EPCGExtraProperties ExtraProperty = EPCGExtraProperties::Index;
		EPCGAttributePropertySelection Selection = EPCGAttributePropertySelection::Attribute;

		const FPCGMetadataAttributeBase* Attribute = nullptr;
		EPCGExTransformComponent Component = EPCGExTransformComponent::Position;
		bool bUseAxis = false;
		EPCGExAxis Axis = EPCGExAxis::Forward;
		EPCGExSingleField Field = EPCGExSingleField::X;

		bool ApplySelector(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData)
		{
			InternalSelector = InSelector.CopyAndFixLast(InData);
			bValid = InternalSelector.IsValid();

			const TArray<FString>& ExtraNames = InternalSelector.GetExtraNames();
			if (GetAxisSelection(ExtraNames, Axis))
			{
				bUseAxis = true;
				// An axis is set, treat as vector.
				// Only axis is set, assume rotation instead of position
				if (!GetComponentSelection(ExtraNames, Component)) { Component = EPCGExTransformComponent::Rotation; }
			}
			else
			{
				bUseAxis = false;
				GetComponentSelection(ExtraNames, Component);
			}

			GetFieldSelection(ExtraNames, Field);
			FullName = ExtraNames.IsEmpty() ? InternalSelector.GetName() : FName(InternalSelector.GetName().ToString() + FString::Join(ExtraNames, TEXT(".")));

			if (InternalSelector.GetSelection() == EPCGAttributePropertySelection::Attribute)
			{
				Attribute = nullptr;
				bValid = false;

				if (const UPCGSpatialData* AsSpatial = Cast<UPCGSpatialData>(InData))
				{
					Attribute = AsSpatial->Metadata->GetConstAttribute(InternalSelector.GetName());
					if (Attribute)
					{
						PCGMetadataAttribute::CallbackWithRightType(
							Attribute->GetTypeId(),
							[&](auto DummyValue) -> void
							{
								using RawT = decltype(DummyValue);
								InternalAccessor = MakeUnique<FPCGAttributeAccessor<RawT>>(static_cast<const FPCGMetadataAttribute<RawT>*>(Attribute), AsSpatial->Metadata);
							});

						bValid = true;
					}
				}
			}

			return bValid;
		}

	public:
		TAttributeBroadcaster()
		{
		}

		virtual EPCGMetadataTypes GetType() const { return GetMetadataType<T>(); }
		const FPCGMetadataAttributeBase* GetAttribute() const { return Attribute; }

		FName FullName = NAME_None;

		TArray<T> Values;
		mutable T Min = T{};
		mutable T Max = T{};

		bool bEnabled = true;
		bool bValid = false;

		bool IsUsable(int32 NumEntries) { return bValid && Values.Num() >= NumEntries; }

		bool Prepare(const FPCGAttributePropertyInputSelector& InSelector, const TSharedRef<PCGExData::FPointIO>& InPointIO)
		{
			ResetMinMax();

			bMinMaxDirty = true;
			bNormalized = false;
			PointIO = InPointIO;

			return ApplySelector(InSelector, InPointIO->GetIn());
		}

		bool Prepare(const FName& InName, const TSharedRef<PCGExData::FPointIO>& InPointIO)
		{
			FPCGAttributePropertyInputSelector InSelector = FPCGAttributePropertyInputSelector();
			InSelector.Update(InName.ToString());
			return Prepare(InSelector, InPointIO);
		}

		/**
		 * Build and validate a property/attribute accessor for the selected
		 * @param Dump
		 * @param StartIndex
		 * @param Count
		 */
		void Fetch(TArray<T>& Dump, const int32 StartIndex, const int32 Count)
		{
			check(bValid)
			check(Dump.Num() == PointIO->GetNum(PCGExData::ESource::In)) // Dump target should be initialized at full length before using Fetch

			const UPCGPointData* InData = PointIO->GetIn();
			const int32 LastIndex = StartIndex + Count;

			if (InternalSelector.GetSelection() == EPCGAttributePropertySelection::Attribute)
			{
				PCGMetadataAttribute::CallbackWithRightType(
					Attribute->GetTypeId(),
					[&](auto DummyValue) -> void
					{
						using RawT = decltype(DummyValue);

						TArray<RawT> RawValues;
						PCGEx::InitArray(RawValues, Count);

						TArrayView<RawT> RawView(RawValues);
						InternalAccessor->GetRange(RawView, StartIndex, *PointIO->GetInKeys().Get(), EPCGAttributeAccessorFlags::AllowBroadcast);

						for (int i = 0; i < Count; i++) { Dump[StartIndex + i] = Convert(RawValues[i]); }
					});
			}
			else if (Selection == EPCGAttributePropertySelection::PointProperty)
			{
				const TArray<FPCGPoint>& InPoints = InData->GetPoints();
#define PCGEX_GET_BY_ACCESSOR(_ENUM, _ACCESSOR) case _ENUM: for (int i = StartIndex; i < LastIndex; i++) { Dump[i] = Convert(InPoints[i]._ACCESSOR); } break;

				switch (InternalSelector.GetPointProperty()) { PCGEX_FOREACH_POINTPROPERTY(PCGEX_GET_BY_ACCESSOR) }
#undef PCGEX_GET_BY_ACCESSOR
			}
			else if (Selection == EPCGAttributePropertySelection::ExtraProperty)
			{
				switch (InternalSelector.GetExtraProperty())
				{
				case EPCGExtraProperties::Index:
					for (int i = StartIndex; i < LastIndex; i++) { Dump[i] = Convert(i); }
					break;
				default: ;
				}
			}
		}

		/**
		 * Build and validate a property/attribute accessor for the selected
		 * @param Dump
		 * @param bCaptureMinMax
		 * @param OutMin
		 * @param OutMax
		 */
		void GrabAndDump(TArray<T>& Dump, const bool bCaptureMinMax, T& OutMin, T& OutMax)
		{
			if (!bValid)
			{
				int32 NumPoints = PointIO->GetNum(PCGExData::ESource::In);
				PCGEx::InitArray(Dump, NumPoints);
				for (int i = 0; i < NumPoints; i++) { Dump[i] = T{}; }
				return;
			}

			const UPCGPointData* InData = PointIO->GetIn();

			int32 NumPoints = PointIO->GetNum(PCGExData::ESource::In);
			PCGEx::InitArray(Dump, NumPoints);

			if (InternalSelector.GetSelection() == EPCGAttributePropertySelection::Attribute)
			{
				PCGMetadataAttribute::CallbackWithRightType(
					Attribute->GetTypeId(),
					[&](auto DummyValue) -> void
					{
						using RawT = decltype(DummyValue);
						TArray<RawT> RawValues;

						PCGEx::InitArray(RawValues, NumPoints);

						TArrayView<RawT> View(RawValues);
						InternalAccessor->GetRange(View, 0, *PointIO->GetInKeys().Get(), EPCGAttributeAccessorFlags::AllowBroadcast);

						if (bCaptureMinMax)
						{
							for (int i = 0; i < NumPoints; i++)
							{
								T V = Convert(RawValues[i]);
								OutMin = PCGExMath::Min(V, OutMin);
								OutMax = PCGExMath::Max(V, OutMax);
								Dump[i] = V;
							}
						}
						else
						{
							for (int i = 0; i < NumPoints; i++) { Dump[i] = Convert(RawValues[i]); }
						}

						RawValues.Empty();
					});
			}
			else if (Selection == EPCGAttributePropertySelection::PointProperty)
			{
				const TArray<FPCGPoint>& InPoints = InData->GetPoints();

#define PCGEX_GET_BY_ACCESSOR(_ENUM, _ACCESSOR) case _ENUM:\
				if (bCaptureMinMax) { for (int i = 0; i < NumPoints; i++) {\
						T V = Convert(InPoints[i]._ACCESSOR); OutMin = PCGExMath::Min(V, OutMin); OutMax = PCGExMath::Max(V, OutMax); Dump[i] = V;\
					} } else { for (int i = 0; i < NumPoints; i++) { Dump[i] = Convert(InPoints[i]._ACCESSOR); } } break;

				switch (InternalSelector.GetPointProperty()) { PCGEX_FOREACH_POINTPROPERTY(PCGEX_GET_BY_ACCESSOR) }
#undef PCGEX_GET_BY_ACCESSOR
			}
			else if (Selection == EPCGAttributePropertySelection::ExtraProperty)
			{
				switch (InternalSelector.GetExtraProperty())
				{
				case EPCGExtraProperties::Index:
					for (int i = 0; i < NumPoints; i++) { Dump[i] = Convert(i); }
					break;
				default: ;
				}
			}
		}

		void Grab(const bool bCaptureMinMax = false)
		{
			GrabAndDump(Values, bCaptureMinMax, Min, Max);
		}

		void UpdateMinMax()
		{
			if (!bMinMaxDirty) { return; }
			ResetMinMax();
			bMinMaxDirty = false;
			for (int i = 0; i < Values.Num(); i++)
			{
				T V = Values[i];
				Min = PCGExMath::Min(V, Min);
				Max = PCGExMath::Max(V, Max);
			}
		}

		void Normalize()
		{
			if (bNormalized) { return; }
			bNormalized = true;
			UpdateMinMax();
			T Range = PCGExMath::Sub(Max, Min);
			for (int i = 0; i < Values.Num(); i++) { Values[i] = PCGExMath::Div(Values[i], Range); }
		}

		FORCEINLINE T SoftGet(const int32 Index, const FPCGPoint& Point, const T& fallback)
		{
			if (!bValid) { return fallback; }
			switch (InternalSelector.GetSelection())
			{
			case EPCGAttributePropertySelection::Attribute:
				return PCGMetadataAttribute::CallbackWithRightType(
					Attribute->GetTypeId(),
					[&](auto DummyValue) -> T
					{
						using RawT = decltype(DummyValue);
						const FPCGMetadataAttribute<RawT>* TypedAttribute = static_cast<const FPCGMetadataAttribute<RawT>*>(Attribute);
						return Convert(TypedAttribute->GetValueFromItemKey(Point.MetadataEntry));
					});
			case EPCGAttributePropertySelection::PointProperty:
#define PCGEX_GET_BY_ACCESSOR(_ENUM, _ACCESSOR) case _ENUM: return Convert(Point._ACCESSOR); break;
				switch (PointProperty) { PCGEX_FOREACH_POINTPROPERTY(PCGEX_GET_BY_ACCESSOR) }
#undef PCGEX_GET_BY_ACCESSOR
			case EPCGAttributePropertySelection::ExtraProperty:
				switch (ExtraProperty)
				{
				case EPCGExtraProperties::Index:
					return Convert(Index);
				default: ;
				}
			default:
				return fallback;
			}
		}

		FORCEINLINE T SafeGet(const int32 Index, const T& fallback) const { return !bValid ? fallback : Values[Index]; }
		FORCEINLINE T operator[](int32 Index) const { return bValid ? Values[Index] : T{}; }

	protected:
		virtual void ResetMinMax()
		{
			if constexpr (std::is_same_v<T, bool>)
			{
				Min = false;
				Max = true;
			}
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
			{
				Min = TNumericLimits<T>::Max();
				Max = TNumericLimits<T>::Min();
			}
			else if constexpr (std::is_same_v<T, FVector2D>)
			{
				Min = FVector2D(MAX_dbl);
				Max = FVector2D(MIN_dbl);
			}
			else if constexpr (std::is_same_v<T, FVector>)
			{
				Min = FVector(MAX_dbl);
				Max = FVector(MIN_dbl);
			}
			else if constexpr (std::is_same_v<T, FVector4>)
			{
				Min = FVector4(MAX_dbl, MAX_dbl, MAX_dbl, MAX_dbl);
				Max = FVector4(MIN_dbl, MIN_dbl, MIN_dbl, MIN_dbl);
			}
			else if constexpr (std::is_same_v<T, FQuat>)
			{
				Min = FRotator(MAX_dbl, MAX_dbl, MAX_dbl).Quaternion();
				Max = FRotator(MIN_dbl, MIN_dbl, MIN_dbl).Quaternion();
			}
			else if constexpr (std::is_same_v<T, FRotator>)
			{
				Min = FRotator(MAX_dbl, MAX_dbl, MAX_dbl);
				Max = FRotator(MIN_dbl, MIN_dbl, MIN_dbl);
			}
			else if constexpr (std::is_same_v<T, FTransform>)
			{
				Min = FTransform(FRotator(MAX_dbl, MAX_dbl, MAX_dbl).Quaternion(), FVector(MAX_dbl), FVector(MAX_dbl));
				Max = FTransform(FRotator(MIN_dbl, MIN_dbl, MIN_dbl).Quaternion(), FVector(MIN_dbl), FVector(MIN_dbl));
			}
			else
			{
				Min = T{};
				Max = T{};
			}
		}

#pragma region Conversions

#pragma region Convert from bool

		FORCEINLINE virtual T Convert(const bool Value) const
		{
			if constexpr (std::is_same_v<T, bool>) { return Value; }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { return Value ? 1 : 0; }
			else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value ? 1 : 0); }
			else if constexpr (std::is_same_v<T, FVector>) { return FVector(Value ? 1 : 0); }
			else if constexpr (std::is_same_v<T, FVector4>)
			{
				const double D = Value ? 1 : 0;
				return FVector4(D, D, D, D);
			}
			else if constexpr (std::is_same_v<T, FQuat>)
			{
				const double D = Value ? 180 : 0;
				return FRotator(D, D, D).Quaternion();
			}
			else if constexpr (std::is_same_v<T, FRotator>)
			{
				const double D = Value ? 180 : 0;
				return FRotator(D, D, D);
			}
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; }
			else if constexpr (std::is_same_v<T, FString>) { return FString::Printf(TEXT("%s"), Value ? TEXT("true") : TEXT("false")); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(FString::Printf(TEXT("%s"), Value ? TEXT("true") : TEXT("false"))); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from Integer32

		FORCEINLINE virtual T Convert(const int32 Value) const
		{
			if constexpr (std::is_same_v<T, bool>) { return Value > 0; }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { return Value; }
			else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value); }
			else if constexpr (std::is_same_v<T, FVector>) { return FVector(Value); }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value, Value, Value, Value); }
			else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value, Value, Value).Quaternion(); }
			else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value, Value, Value); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; }
			else if constexpr (std::is_same_v<T, FString>) { return FString::Printf(TEXT("%d"), Value); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(FString::Printf(TEXT("%d"), Value)); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from Integer64

		FORCEINLINE virtual T Convert(const int64 Value) const
		{
			if constexpr (std::is_same_v<T, bool>) { return Value > 0; }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { return Value; }
			else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value); }
			else if constexpr (std::is_same_v<T, FVector>) { return FVector(Value); }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value, Value, Value, Value); }
			else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value, Value, Value).Quaternion(); }
			else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value, Value, Value); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; }
			else if constexpr (std::is_same_v<T, FString>) { return FString::Printf(TEXT("%lld"), Value); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(FString::Printf(TEXT("(%lld)"), Value)); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from Float

		FORCEINLINE virtual T Convert(const float Value) const
		{
			if constexpr (std::is_same_v<T, bool>) { return Value > 0; }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { return Value; }
			else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value); }
			else if constexpr (std::is_same_v<T, FVector>) { return FVector(Value); }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value, Value, Value, Value); }
			else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value, Value, Value).Quaternion(); }
			else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value, Value, Value); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; }
			else if constexpr (std::is_same_v<T, FString>) { return FString::Printf(TEXT("%f"), Value); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(FString::Printf(TEXT("(%f)"), Value)); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from Double

		FORCEINLINE virtual T Convert(const double Value) const
		{
			if constexpr (std::is_same_v<T, bool>) { return Value > 0; }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { return Value; }
			else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value); }
			else if constexpr (std::is_same_v<T, FVector>) { return FVector(Value); }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value, Value, Value, Value); }
			else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value, Value, Value).Quaternion(); }
			else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value, Value, Value); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; }
			else if constexpr (std::is_same_v<T, FString>) { return FString::Printf(TEXT("%f"), Value); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(FString::Printf(TEXT("(%f)"), Value)); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FVector2D

		FORCEINLINE virtual T Convert(const FVector2D& Value) const
		{
			if constexpr (std::is_same_v<T, bool>)
			{
				switch (Field)
				{
				default:
				case EPCGExSingleField::X:
					return Value.X > 0;
				case EPCGExSingleField::Y:
				case EPCGExSingleField::Z:
				case EPCGExSingleField::W:
					return Value.Y > 0;
				case EPCGExSingleField::Length:
					return Value.SquaredLength() > 0;
				}
			}
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
			{
				switch (Field)
				{
				default:
				case EPCGExSingleField::X:
					return Value.X;
				case EPCGExSingleField::Y:
				case EPCGExSingleField::Z:
				case EPCGExSingleField::W:
					return Value.Y;
				case EPCGExSingleField::Length:
					return Value.SquaredLength();
				}
			}
			else if constexpr (std::is_same_v<T, FVector2D>) { return Value; }
			else if constexpr (std::is_same_v<T, FVector>) { return FVector(Value.X, Value.Y, 0); }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value.X, Value.Y, 0, 0); }
			else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value.X, Value.Y, 0).Quaternion(); }
			else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value.X, Value.Y, 0); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; }
			else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FVector

		FORCEINLINE virtual T Convert(const FVector& Value) const
		{
			if constexpr (std::is_same_v<T, bool>)
			{
				switch (Field)
				{
				default:
				case EPCGExSingleField::X:
					return Value.X > 0;
				case EPCGExSingleField::Y:
					return Value.Y > 0;
				case EPCGExSingleField::Z:
				case EPCGExSingleField::W:
					return Value.Z > 0;
				case EPCGExSingleField::Length:
					return Value.SquaredLength() > 0;
				}
			}
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
			{
				switch (Field)
				{
				default:
				case EPCGExSingleField::X:
					return Value.X;
				case EPCGExSingleField::Y:
					return Value.Y;
				case EPCGExSingleField::Z:
				case EPCGExSingleField::W:
					return Value.Z;
				case EPCGExSingleField::Length:
					return Value.SquaredLength();
				}
			}
			else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value.X, Value.Y); }
			else if constexpr (std::is_same_v<T, FVector>) { return Value; }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value.X, Value.Y, Value.Z, 0); }
			else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value.X, Value.Y, Value.Z).Quaternion(); /* TODO : Handle axis selection */ }
			else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value.X, Value.Y, Value.Z); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; /* TODO : Handle axis selection */ }
			else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FVector4

		FORCEINLINE virtual T Convert(const FVector4& Value) const
		{
			if constexpr (std::is_same_v<T, bool>)
			{
				switch (Field)
				{
				default:
				case EPCGExSingleField::X:
					return Value.X > 0;
				case EPCGExSingleField::Y:
					return Value.Y > 0;
				case EPCGExSingleField::Z:
					return Value.Z > 0;
				case EPCGExSingleField::W:
					return Value.W > 0;
				case EPCGExSingleField::Length:
					return FVector(Value).SquaredLength() > 0;
				}
			}
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
			{
				switch (Field)
				{
				default:
				case EPCGExSingleField::X:
					return Value.X > 0;
				case EPCGExSingleField::Y:
					return Value.Y > 0;
				case EPCGExSingleField::Z:
					return Value.Z > 0;
				case EPCGExSingleField::W:
					return Value.W > 0;
				case EPCGExSingleField::Length:
					return FVector(Value).SquaredLength() > 0;
				}
			}
			else if constexpr (std::is_same_v<T, FVector2D>) { return FVector2D(Value.X, Value.Y); }
			else if constexpr (std::is_same_v<T, FVector>) { return Value; }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value.X, Value.Y, Value.Z, Value.W); }
			else if constexpr (std::is_same_v<T, FQuat>) { return FRotator(Value.X, Value.Y, Value.Z).Quaternion(); }
			else if constexpr (std::is_same_v<T, FRotator>) { return FRotator(Value.X, Value.Y, Value.Z); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform::Identity; /* TODO : Handle axis */ }
			else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FQuat

		FORCEINLINE virtual T Convert(const FQuat& Value) const
		{
			if constexpr (std::is_same_v<T, bool>)
			{
				const FVector Dir = PCGExMath::GetDirection(Value, Axis);
				switch (Field)
				{
				default:
				case EPCGExSingleField::X:
					return Dir.X > 0;
				case EPCGExSingleField::Y:
					return Dir.Y > 0;
				case EPCGExSingleField::Z:
				case EPCGExSingleField::W:
					return Dir.Z > 0;
				case EPCGExSingleField::Length:
					return Dir.SquaredLength() > 0;
				}
			}
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
			{
				const FVector Dir = PCGExMath::GetDirection(Value, Axis);
				switch (Field)
				{
				default:
				case EPCGExSingleField::X:
					return Dir.X;
				case EPCGExSingleField::Y:
					return Dir.Y;
				case EPCGExSingleField::Z:
				case EPCGExSingleField::W:
					return Dir.Z;
				case EPCGExSingleField::Length:
					return Dir.SquaredLength();
				}
			}
			else if constexpr (std::is_same_v<T, FVector2D>)
			{
				const FVector Dir = PCGExMath::GetDirection(Value, Axis);
				return FVector2D(Dir.X, Dir.Y);
			}
			else if constexpr (std::is_same_v<T, FVector>) { return PCGExMath::GetDirection(Value, Axis); }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(PCGExMath::GetDirection(Value, Axis), 0); }
			else if constexpr (std::is_same_v<T, FQuat>) { return Value; }
			else if constexpr (std::is_same_v<T, FRotator>) { return Value.Rotator(); }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform(Value, FVector::ZeroVector, FVector::OneVector); }
			else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FRotator

		FORCEINLINE virtual T Convert(const FRotator& Value) const
		{
			if constexpr (std::is_same_v<T, bool>)
			{
				switch (Field)
				{
				default:
				case EPCGExSingleField::X:
					return Value.Pitch > 0;
				case EPCGExSingleField::Y:
					return Value.Yaw > 0;
				case EPCGExSingleField::Z:
				case EPCGExSingleField::W:
					return Value.Roll > 0;
				case EPCGExSingleField::Length:
					return Value.Euler().SquaredLength() > 0;
				}
			}
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
			{
				switch (Field)
				{
				default:
				case EPCGExSingleField::X:
					return Value.Pitch > 0;
				case EPCGExSingleField::Y:
					return Value.Yaw > 0;
				case EPCGExSingleField::Z:
				case EPCGExSingleField::W:
					return Value.Roll > 0;
				case EPCGExSingleField::Length:
					return Value.Euler().SquaredLength() > 0;
				}
			}
			else if constexpr (std::is_same_v<T, FVector2D>) { return Convert(Value.Quaternion()); }
			else if constexpr (std::is_same_v<T, FVector>) { return Convert(Value.Quaternion()); }
			else if constexpr (std::is_same_v<T, FVector4>) { return FVector4(Value.Euler(), 0); /* TODO : Handle axis */ }
			else if constexpr (std::is_same_v<T, FQuat>) { return Value.Quaternion(); }
			else if constexpr (std::is_same_v<T, FRotator>) { return Value; }
			else if constexpr (std::is_same_v<T, FTransform>) { return FTransform(Value.Quaternion(), FVector::ZeroVector, FVector::OneVector); }
			else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FTransform

		FORCEINLINE virtual T Convert(const FTransform& Value) const
		{
			if constexpr (
				std::is_same_v<T, bool> ||
				std::is_same_v<T, int32> ||
				std::is_same_v<T, int64> ||
				std::is_same_v<T, float> ||
				std::is_same_v<T, double> ||
				std::is_same_v<T, FVector2D> ||
				std::is_same_v<T, FVector> ||
				std::is_same_v<T, FVector4> ||
				std::is_same_v<T, FQuat> ||
				std::is_same_v<T, FRotator>)
			{
				switch (Component)
				{
				default:
				case EPCGExTransformComponent::Position: return Convert(Value.GetLocation());
				case EPCGExTransformComponent::Rotation: return Convert(Value.GetRotation());
				case EPCGExTransformComponent::Scale: return Convert(Value.GetScale3D());
				}
			}
			else if constexpr (std::is_same_v<T, FTransform>) { return Value; }
			else if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FString

		FORCEINLINE virtual T Convert(const FString& Value) const
		{
			if constexpr (std::is_same_v<T, FString>) { return Value; }
			else if constexpr (std::is_same_v<T, FName>) { return FName(Value); }
			else if constexpr (std::is_same_v<T, FSoftClassPath>) { return FSoftClassPath(Value); }
			else if constexpr (std::is_same_v<T, FSoftObjectPath>) { return FSoftObjectPath(Value); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FName

		FORCEINLINE virtual T Convert(const FName& Value) const
		{
			if constexpr (std::is_same_v<T, FString>) { return Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return Value; }
			else if constexpr (std::is_same_v<T, FSoftClassPath>) { return FSoftClassPath(Value.ToString()); }
			else if constexpr (std::is_same_v<T, FSoftObjectPath>) { return FSoftObjectPath(Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FSoftClassPath

		FORCEINLINE virtual T Convert(const FSoftClassPath& Value) const
		{
			if constexpr (std::is_same_v<T, FString>) { return Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(Value.ToString()); }
			else if constexpr (std::is_same_v<T, FSoftClassPath>) { return Value; }
			else if constexpr (std::is_same_v<T, FSoftObjectPath>) { return FSoftObjectPath(Value.ToString()); }
			else { return T{}; }
		}

#pragma endregion

#pragma region Convert from FSoftObjectPath

		FORCEINLINE virtual T Convert(const FSoftObjectPath& Value) const
		{
			if constexpr (std::is_same_v<T, FString>) { return *Value.ToString(); }
			else if constexpr (std::is_same_v<T, FName>) { return FName(*Value.ToString()); }
			else if constexpr (std::is_same_v<T, FSoftClassPath>) { return FSoftClassPath(Value.ToString()); }
			else if constexpr (std::is_same_v<T, FSoftObjectPath>) { return Value; }
			else { return T{}; }
		}

#pragma endregion

#pragma endregion
	};

#pragma endregion

#pragma region Attribute copy

	static void CopyPoints(
		const PCGExData::FPointIO* Source,
		const PCGExData::FPointIO* Target,
		const TArrayView<const int32>& SourceIndices,
		const int32 TargetIndex = 0,
		const bool bKeepSourceMetadataEntry = false)
	{
		const int32 NumIndices = SourceIndices.Num();
		const TArray<FPCGPoint>& SourcePoints = Source->GetIn()->GetPoints();
		TArray<FPCGPoint>& TargetPoints = Target->GetOut()->GetMutablePoints();

		if (bKeepSourceMetadataEntry)
		{
			for (int i = 0; i < NumIndices; i++)
			{
				TargetPoints[TargetIndex + i] = SourcePoints[SourceIndices[i]];
			}
		}
		else
		{
			for (int i = 0; i < NumIndices; i++)
			{
				const int32 WriteIndex = TargetIndex + i;
				const PCGMetadataEntryKey Key = TargetPoints[WriteIndex].MetadataEntry;

				const FPCGPoint& SourcePt = SourcePoints[SourceIndices[i]];
				FPCGPoint& TargetPt = TargetPoints[WriteIndex] = SourcePt;
				TargetPt.MetadataEntry = Key;
			}
		}
	}

#pragma endregion

	static TSharedPtr<FAttributesInfos> GatherAttributeInfos(const FPCGContext* InContext, const FName InPinLabel, const FPCGExAttributeGatherDetails& InGatherDetails, const bool bThrowError)
	{
		TSharedPtr<FAttributesInfos> OutInfos = MakeShared<FAttributesInfos>();
		TArray<FPCGTaggedData> TaggedDatas = InContext->InputData.GetInputsByPin(InPinLabel);

		bool bHasErrors = false;

		for (FPCGTaggedData& TaggedData : TaggedDatas)
		{
			const UPCGMetadata* Metadata = nullptr;

			if (const UPCGParamData* ParamData = Cast<UPCGParamData>(TaggedData.Data)) { Metadata = ParamData->Metadata; }
			else if (const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(TaggedData.Data)) { Metadata = SpatialData->Metadata; }

			if (!Metadata) { continue; }

			TSet<FName> Mismatch;
			TSharedPtr<FAttributesInfos> Infos = FAttributesInfos::Get(Metadata);

			OutInfos->Append(Infos, InGatherDetails, Mismatch);

			if (bThrowError && !Mismatch.IsEmpty())
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("Some inputs share the same name but not the same type."));
				bHasErrors = true;
				break;
			}
		}

		if (bHasErrors) { OutInfos.Reset(); }
		return OutInfos;
	}
}

#undef PCGEX_AAFLAG
