// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include <functional>

#include "CoreMinimal.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "PCGExMacros.h"
#include "PCGEx.h"
#include "PCGExBroadcast.h"
#include "PCGExHelpers.h"
#include "PCGExMath.h"
#include "PCGExMT.h"
#include "PCGExPointIO.h"
#include "Blending/PCGExBlendModes.h"

#include "Metadata/Accessors/PCGAttributeAccessor.h"

#include "PCGExAttributeHelpers.generated.h"

#pragma region Input Configs

namespace PCGExData
{
	class FFacade;
}

struct FPCGExAttributeGatherDetails;

#pragma region Legacy

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputConfig
{
	GENERATED_BODY()

	// Legacy horror

	FPCGExInputConfig() = default;
	explicit FPCGExInputConfig(const FPCGAttributePropertyInputSelector& InSelector);
	explicit FPCGExInputConfig(const FPCGExInputConfig& Other);
	explicit FPCGExInputConfig(const FName InName);

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

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAttributeSourceToTargetDetails
{
	GENERATED_BODY()

	FPCGExAttributeSourceToTargetDetails()
	{
	}

	/** Attribute to read on input */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName Source = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bOutputToDifferentName = false;

	/** Attribute to write on output, if different from input */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bOutputToDifferentName"))
	FName Target = NAME_None;

	bool ValidateNames(FPCGExContext* InContext) const;
	FName GetOutputName() const;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAttributeSourceToTargetList
{
	GENERATED_BODY()

	FPCGExAttributeSourceToTargetList()
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, TitleProperty="{Source}"))
	TArray<FPCGExAttributeSourceToTargetDetails> Attributes;

	bool IsEmpty() const { return Attributes.IsEmpty(); }
	int32 Num() const { return Attributes.Num(); }

	bool ValidateNames(FPCGExContext* InContext) const;
	void SetOutputTargetNames(const TSharedRef<PCGExData::FFacade>& InFacade) const;

	void GetSources(TArray<FName>& OutNames) const;
};

#pragma endregion

namespace PCGEx
{
#pragma region Attribute identity

	struct PCGEXTENDEDTOOLKIT_API FAttributeIdentity
	{
		FName Name = NAME_None;
		EPCGMetadataTypes UnderlyingType = EPCGMetadataTypes::Unknown;
		bool bAllowsInterpolation = true;

		FAttributeIdentity() = default;

		FAttributeIdentity(const FAttributeIdentity& Other)
			: Name(Other.Name), UnderlyingType(Other.UnderlyingType), bAllowsInterpolation(Other.bAllowsInterpolation)
		{
		}

		FAttributeIdentity(const FName InName, const EPCGMetadataTypes InUnderlyingType, const bool InAllowsInterpolation)
			: Name(InName), UnderlyingType(InUnderlyingType), bAllowsInterpolation(InAllowsInterpolation)
		{
		}

		int16 GetTypeId() const { return static_cast<int16>(UnderlyingType); }
		bool IsA(const int16 InType) const { return GetTypeId() == InType; }
		bool IsA(const EPCGMetadataTypes InType) const { return UnderlyingType == InType; }

		FString GetDisplayName() const { return FString(Name.ToString() + FString::Printf(TEXT("( %d )"), UnderlyingType)); }
		bool operator==(const FAttributeIdentity& Other) const { return Name == Other.Name; }

		static void Get(const UPCGMetadata* InMetadata, TArray<FAttributeIdentity>& OutIdentities);
		static void Get(const UPCGMetadata* InMetadata, TArray<FName>& OutNames, TMap<FName, FAttributeIdentity>& OutIdentities);

		using FForEachFunc = std::function<void (const FAttributeIdentity&, const int32)>;
		static int32 ForEach(const UPCGMetadata* InMetadata, FForEachFunc&& Func);
	};

	class FAttributesInfos : public TSharedFromThis<FAttributesInfos>
	{
	public:
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

		using FilterCallback = std::function<bool(const FName&)>;

		void Filter(const FilterCallback& FilterFn);

		~FAttributesInfos() = default;

		static TSharedPtr<FAttributesInfos> Get(const UPCGMetadata* InMetadata, const TSet<FName>* IgnoredAttributes = nullptr);
		static TSharedPtr<FAttributesInfos> Get(const TSharedPtr<PCGExData::FPointIOCollection>& InCollection, TSet<FName>& OutTypeMismatch, const TSet<FName>* IgnoredAttributes = nullptr);
	};

	PCGEXTENDEDTOOLKIT_API
	void GatherAttributes(
		const TSharedPtr<FAttributesInfos>& OutInfos, const FPCGContext* InContext, const FName InputLabel,
		const FPCGExAttributeGatherDetails& InDetails, TSet<FName>& Mismatches);

	PCGEXTENDEDTOOLKIT_API
	TSharedPtr<FAttributesInfos> GatherAttributes(
		const FPCGContext* InContext, const FName InputLabel,
		const FPCGExAttributeGatherDetails& InDetails, TSet<FName>& Mismatches);

	struct PCGEXTENDEDTOOLKIT_API FAttributeProcessingInfos
	{
		bool bIsValid = false;

		FAttributeIdentity SourceIdentity = FAttributeIdentity();
		FAttributeIdentity TargetIdentity = FAttributeIdentity();

		const FPCGMetadataAttributeBase* Attribute = nullptr;

		FPCGAttributePropertyInputSelector Selector;
		FSubSelection SubSelection = FSubSelection();

		FAttributeProcessingInfos() = default;

		FAttributeProcessingInfos(const UPCGData* InData, const FPCGAttributePropertyInputSelector& InSelector);
		FAttributeProcessingInfos(const UPCGData* InData, const FName InAttributeName);

		operator const FPCGMetadataAttributeBase*() const { return Attribute; }

		operator EPCGMetadataTypes() const
		{
			if (Attribute) { return static_cast<EPCGMetadataTypes>(Attribute->GetTypeId()); }
			return EPCGMetadataTypes::Unknown;
		}

		operator EPCGAttributePropertySelection() const { return Selector.GetSelection(); }
		operator EPCGPointProperties() const { return Selector.GetPointProperty(); }
		operator EPCGExtraProperties() const { return Selector.GetExtraProperty(); }

	protected:
		void Init(const UPCGData* InData, const FPCGAttributePropertyInputSelector& InSelector);
	};

	template<EPCGPointProperties PROPERTY, typename T>
	inline static void SetPointProperty(FPCGPoint& Point, const T& InValue)
	{
#define PCGEX_STATIC_POINTPROPERTY_NONE(_TYPE)
#define PCGEX_STATIC_POINTPROPERTY_SETTER(_TYPE) PCGEx::Convert<T, _TYPE>(InValue)
		PCGEX_CONSTEXPR_IFELSE_SETPOINTPROPERTY(PROPERTY, Point, PCGEX_STATIC_POINTPROPERTY_NONE, PCGEX_STATIC_POINTPROPERTY_SETTER)
#undef PCGEX_STATIC_POINTPROPERTY_SETTER
#undef PCGEX_STATIC_POINTPROPERTY_NONE
	}

#pragma endregion

#pragma region Attribute Broadcaster

	template <bool bInitialized>
	static FName GetSelectorFullName(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData)
	{
		if (!InData) { return FName(TEXT("NULL_DATA")); }

		if constexpr (bInitialized)
		{
			if (InSelector.GetExtraNames().IsEmpty()) { return FName(InSelector.GetName().ToString()); }
			return FName(InSelector.GetName().ToString() + TEXT(".") + FString::Join(InSelector.GetExtraNames(), TEXT(".")));
		}
		else
		{
			if (InSelector.GetSelection() == EPCGAttributePropertySelection::Attribute && InSelector.GetName() == "@Last")
			{
				return GetSelectorFullName<true>(InSelector.CopyAndFixLast(InData), InData);
			}

			return GetSelectorFullName<true>(InSelector, InData);
		}
	}

	PCGEXTENDEDTOOLKIT_API
	FString GetSelectorDisplayName(const FPCGAttributePropertyInputSelector& InSelector);

	class PCGEXTENDEDTOOLKIT_API FAttributeBroadcasterBase
	{
	public:
		FAttributeProcessingInfos ProcessingInfos = FAttributeProcessingInfos();
		virtual ~FAttributeBroadcasterBase() = default;
	};

	template <typename T>
	class TAttributeBroadcaster : public FAttributeBroadcasterBase
	{
	protected:
		TSharedPtr<PCGExData::FPointIO> PointIO;
		bool bMinMaxDirty = true;
		bool bNormalized = false;
		TSharedPtr<IPCGAttributeAccessor> InternalAccessor;

		EPCGExTransformComponent Component = EPCGExTransformComponent::Position;
		EPCGExAxis Axis = EPCGExAxis::Forward;
		EPCGExSingleField Field = EPCGExSingleField::X;

		bool ApplySelector(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData)
		{
			ProcessingInfos = FAttributeProcessingInfos(InData, InSelector);

			if (!ProcessingInfos.bIsValid) { return false; }

			FullName = GetSelectorFullName<true>(ProcessingInfos.Selector, InData);

			if (ProcessingInfos.Attribute)
			{
				PCGEx::ExecuteWithRightType(
					ProcessingInfos.Attribute->GetTypeId(), [&](auto DummyValue)
					{
						using RawT = decltype(DummyValue);
						InternalAccessor = MakeShared<FPCGAttributeAccessor<RawT>>(static_cast<const FPCGMetadataAttribute<RawT>*>(ProcessingInfos.Attribute), Cast<UPCGSpatialData>(InData)->Metadata);
					});
			}

			return ProcessingInfos.bIsValid;
		}

	public:
		TAttributeBroadcaster()
		{
		}

		FString GetName() const { return ProcessingInfos.Selector.GetName().ToString(); }

		virtual EPCGMetadataTypes GetType() const { return GetMetadataType<T>(); }
		const FPCGMetadataAttributeBase* GetAttribute() const { return ProcessingInfos; }

		FName FullName = NAME_None;

		TArray<T> Values;
		mutable T Min = T{};
		mutable T Max = T{};

		bool bEnabled = true;

		bool IsUsable(int32 NumEntries) { return ProcessingInfos.bIsValid && Values.Num() >= NumEntries; }

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
		 */
		void Fetch(TArray<T>& Dump, const PCGExMT::FScope& Scope)
		{
			check(ProcessingInfos.bIsValid)
			check(Dump.Num() == PointIO->GetNum(PCGExData::ESource::In)) // Dump target should be initialized at full length before using Fetch

			const UPCGPointData* InData = PointIO->GetIn();

			if (ProcessingInfos == EPCGAttributePropertySelection::Attribute)
			{
				PCGEx::ExecuteWithRightType(
					ProcessingInfos.Attribute->GetTypeId(), [&](auto DummyValue)
					{
						using RawT = decltype(DummyValue);

						TArray<RawT> RawValues;
						PCGEx::InitArray(RawValues, Scope.Count);

						TArrayView<RawT> RawView(RawValues);
						InternalAccessor->GetRange(RawView, Scope.Start, *PointIO->GetInKeys().Get(), EPCGAttributeAccessorFlags::AllowBroadcast);

						const FSubSelection& S = ProcessingInfos.SubSelection;
						if (S.bIsValid) { for (int i = 0; i < Scope.Count; i++) { Dump[Scope.Start + i] = S.Get<RawT, T>(RawValues[i]); } }
						else { for (int i = 0; i < Scope.Count; i++) { Dump[Scope.Start + i] = PCGEx::Convert<RawT, T>(RawValues[i]); } }
					});
			}
			else if (ProcessingInfos == EPCGAttributePropertySelection::PointProperty)
			{
				// TODO : constexpr 
				const FSubSelection& S = ProcessingInfos.SubSelection;
				const TArray<FPCGPoint>& InPoints = InData->GetPoints();
#define PCGEX_GET_BY_ACCESSOR(_ENUM, _ACCESSOR, _TYPE) case _ENUM: \
	if (S.bIsValid) { for (int i = Scope.Start; i < Scope.End; i++) { Dump[i] = S.Get<_TYPE, T>(InPoints[i]._ACCESSOR); } } \
	else{ for (int i = Scope.Start; i < Scope.End; i++) { Dump[i] = PCGEx::Convert<_TYPE, T>(InPoints[i]._ACCESSOR); } } \
	break;

				switch (static_cast<EPCGPointProperties>(ProcessingInfos)) { PCGEX_FOREACH_POINTPROPERTY(PCGEX_GET_BY_ACCESSOR) }
#undef PCGEX_GET_BY_ACCESSOR
			}
			else if (ProcessingInfos == EPCGAttributePropertySelection::ExtraProperty)
			{
				const FSubSelection& S = ProcessingInfos.SubSelection;
				switch (static_cast<EPCGExtraProperties>(ProcessingInfos))
				{
				case EPCGExtraProperties::Index:
					if (S.bIsValid) { for (int i = Scope.Start; i < Scope.End; i++) { Dump[i] = S.Get<int32, T>(i); } }
					else { for (int i = Scope.Start; i < Scope.End; i++) { Dump[i] = PCGEx::Convert<int32, T>(i); } }
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
			if (!ProcessingInfos.bIsValid)
			{
				int32 NumPoints = PointIO->GetNum(PCGExData::ESource::In);
				PCGEx::InitArray(Dump, NumPoints);
				for (int i = 0; i < NumPoints; i++) { Dump[i] = T{}; }
				return;
			}

			const UPCGPointData* InData = PointIO->GetIn();

			int32 NumPoints = PointIO->GetNum(PCGExData::ESource::In);
			PCGEx::InitArray(Dump, NumPoints);

			if (ProcessingInfos == EPCGAttributePropertySelection::Attribute)
			{
				PCGEx::ExecuteWithRightType(
					ProcessingInfos.Attribute->GetTypeId(), [&](auto DummyValue)
					{
						using RawT = decltype(DummyValue);
						TArray<RawT> RawValues;

						PCGEx::InitArray(RawValues, NumPoints);

						TArrayView<RawT> View(RawValues);
						InternalAccessor->GetRange(View, 0, *PointIO->GetInKeys().Get(), EPCGAttributeAccessorFlags::AllowBroadcast);

						const FSubSelection& S = ProcessingInfos.SubSelection;

						if (bCaptureMinMax)
						{
							if (S.bIsValid)
							{
								for (int i = 0; i < NumPoints; i++)
								{
									T V = S.Get<RawT, T>(RawValues[i]);
									OutMin = PCGExBlend::Min(V, OutMin);
									OutMax = PCGExBlend::Max(V, OutMax);
									Dump[i] = V;
								}
							}
							else
							{
								for (int i = 0; i < NumPoints; i++)
								{
									T V = PCGEx::Convert<RawT, T>(RawValues[i]);
									OutMin = PCGExBlend::Min(V, OutMin);
									OutMax = PCGExBlend::Max(V, OutMax);
									Dump[i] = V;
								}
							}
						}
						else
						{
							if (S.bIsValid) { for (int i = 0; i < NumPoints; i++) { Dump[i] = S.Get<RawT, T>(RawValues[i]); } }
							else { for (int i = 0; i < NumPoints; i++) { Dump[i] = PCGEx::Convert<RawT, T>(RawValues[i]); } }
						}

						RawValues.Empty();
					});
			}
			else if (ProcessingInfos == EPCGAttributePropertySelection::PointProperty)
			{
				const FSubSelection& S = ProcessingInfos.SubSelection;
				const TArray<FPCGPoint>& InPoints = InData->GetPoints();

#define PCGEX_GET_BY_ACCESSOR(_ENUM, _ACCESSOR, _TYPE) case _ENUM:\
				if (bCaptureMinMax) {\
					if (S.bIsValid){ for (int i = 0; i < NumPoints; i++) { T V = S.Get<_TYPE, T>(InPoints[i]._ACCESSOR); OutMin = PCGExBlend::Min(V, OutMin); OutMax = PCGExBlend::Max(V, OutMax); Dump[i] = V; } } \
					else { for (int i = 0; i < NumPoints; i++) { T V = PCGEx::Convert<_TYPE, T>(InPoints[i]._ACCESSOR); OutMin = PCGExBlend::Min(V, OutMin); OutMax = PCGExBlend::Max(V, OutMax); Dump[i] = V; } } \
				}else{ \
					if (S.bIsValid){ for (int i = 0; i < NumPoints; i++) { Dump[i] = S.Get<_TYPE, T>(InPoints[i]._ACCESSOR); } } \
					else{ for (int i = 0; i < NumPoints; i++) { Dump[i] = PCGEx::Convert<_TYPE, T>(InPoints[i]._ACCESSOR); } } \
				} break;

				switch (static_cast<EPCGPointProperties>(ProcessingInfos)) { PCGEX_FOREACH_POINTPROPERTY(PCGEX_GET_BY_ACCESSOR) }
#undef PCGEX_GET_BY_ACCESSOR
			}
			else if (ProcessingInfos == EPCGAttributePropertySelection::ExtraProperty)
			{
				const FSubSelection& S = ProcessingInfos.SubSelection;

				switch (static_cast<EPCGExtraProperties>(ProcessingInfos))
				{
				case EPCGExtraProperties::Index:
					if (bCaptureMinMax) { for (int i = 0; i < NumPoints; i++) { Dump[i] = S.Get<int32, T>(i); } }
					else { for (int i = 0; i < NumPoints; i++) { Dump[i] = PCGEx::Convert<int32, T>(i); } }
					break;
				default: ;
				}
			}
		}

		void GrabUniqueValues(TSet<T>& Dump)
		{
			if (!ProcessingInfos.bIsValid) { return; }

			const UPCGPointData* InData = PointIO->GetIn();

			int32 NumPoints = PointIO->GetNum(PCGExData::ESource::In);
			Dump.Reserve(Dump.Num() + NumPoints);

			if (ProcessingInfos == EPCGAttributePropertySelection::Attribute)
			{
				PCGEx::ExecuteWithRightType(
					ProcessingInfos.Attribute->GetTypeId(), [&](auto DummyValue)
					{
						using RawT = decltype(DummyValue);
						TArray<RawT> RawValues;

						PCGEx::InitArray(RawValues, NumPoints);

						TArrayView<RawT> View(RawValues);
						InternalAccessor->GetRange(View, 0, *PointIO->GetInKeys().Get(), EPCGAttributeAccessorFlags::AllowBroadcast);

						const FSubSelection& S = ProcessingInfos.SubSelection;
						if (S.bIsValid) { for (int i = 0; i < NumPoints; i++) { Dump.Add(S.Get<RawT, T>(RawValues[i])); } }
						else { for (int i = 0; i < NumPoints; i++) { Dump.Add(PCGEx::Convert<RawT, T>(RawValues[i])); } }

						RawValues.Empty();
					});
			}
			else if (ProcessingInfos == EPCGAttributePropertySelection::PointProperty)
			{
				const FSubSelection& S = ProcessingInfos.SubSelection;
				const TArray<FPCGPoint>& InPoints = InData->GetPoints();

#define PCGEX_GET_BY_ACCESSOR(_ENUM, _ACCESSOR, _TYPE) case _ENUM: \
				if (S.bIsValid) { for (int i = 0; i < NumPoints; i++) { Dump.Add(S.Get<_TYPE, T>(InPoints[i]._ACCESSOR)); } } \
				else{ for (int i = 0; i < NumPoints; i++) { Dump.Add(PCGEx::Convert<_TYPE, T>(InPoints[i]._ACCESSOR)); } } break;
				switch (static_cast<EPCGPointProperties>(ProcessingInfos)) { PCGEX_FOREACH_POINTPROPERTY(PCGEX_GET_BY_ACCESSOR) }
#undef PCGEX_GET_BY_ACCESSOR
			}
			else if (ProcessingInfos == EPCGAttributePropertySelection::ExtraProperty)
			{
				const FSubSelection& S = ProcessingInfos.SubSelection;
				switch (static_cast<EPCGExtraProperties>(ProcessingInfos))
				{
				case EPCGExtraProperties::Index:
					if (S.bIsValid) { for (int i = 0; i < NumPoints; i++) { Dump.Add(S.Get<int32, T>(i)); } }
					else { for (int i = 0; i < NumPoints; i++) { Dump.Add(PCGEx::Convert<int32, T>(i)); } }
					break;
				default: ;
				}
			}

			Dump.Shrink();
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
				Min = PCGExBlend::Min(V, Min);
				Max = PCGExBlend::Max(V, Max);
			}
		}

		void Normalize()
		{
			if (bNormalized) { return; }
			bNormalized = true;
			UpdateMinMax();
			T Range = PCGExBlend::Sub(Max, Min);
			for (int i = 0; i < Values.Num(); i++) { Values[i] = PCGExBlend::Div(Values[i], Range); }
		}

		T SoftGet(const int32 Index, const FPCGPoint& Point, const T& Fallback) const
		{
			if (!ProcessingInfos.bIsValid) { return Fallback; }
			switch (static_cast<EPCGAttributePropertySelection>(ProcessingInfos))
			{
			case EPCGAttributePropertySelection::Attribute:
				return PCGMetadataAttribute::CallbackWithRightType(
					ProcessingInfos.Attribute->GetTypeId(),
					[&](auto DummyValue) -> T
					{
						using RawT = decltype(DummyValue);
						const FPCGMetadataAttribute<RawT>* TypedAttribute = static_cast<const FPCGMetadataAttribute<RawT>*>(ProcessingInfos.Attribute);
						return ProcessingInfos.SubSelection.Get<RawT, T>(TypedAttribute->GetValueFromItemKey(Point.MetadataEntry));
					});
			case EPCGAttributePropertySelection::PointProperty:
#define PCGEX_GET_BY_ACCESSOR(_ENUM, _ACCESSOR, _TYPE) case _ENUM: return ProcessingInfos.SubSelection.Get<_TYPE, T>(Point._ACCESSOR); break;
				switch (static_cast<EPCGPointProperties>(ProcessingInfos)) { PCGEX_FOREACH_POINTPROPERTY(PCGEX_GET_BY_ACCESSOR) }
#undef PCGEX_GET_BY_ACCESSOR
			case EPCGAttributePropertySelection::ExtraProperty:
				switch (static_cast<EPCGExtraProperties>(ProcessingInfos))
				{
				case EPCGExtraProperties::Index:
					return ProcessingInfos.SubSelection.Get<T>(Index);
				default: ;
				}
			default:
				return Fallback;
			}
		}

		T SoftGet(const PCGExData::FPointRef& PointRef, const T& Fallback) const { return SoftGet(PointRef.Index, *PointRef.Point, Fallback); }

		T SafeGet(const int32 Index, const T& Fallback) const { return !ProcessingInfos.bIsValid ? Fallback : Values[Index]; }
		T operator[](int32 Index) const { return ProcessingInfos.bIsValid ? Values[Index] : T{}; }

		static TSharedPtr<TAttributeBroadcaster<T>> Make(const FName& InName, const TSharedRef<PCGExData::FPointIO>& InPointIO)
		{
			PCGEX_MAKE_SHARED(Broadcaster, TAttributeBroadcaster<T>)
			if (!Broadcaster->Prepare(InName, InPointIO)) { return nullptr; }
			return Broadcaster;
		}

		static TSharedPtr<TAttributeBroadcaster<T>> Make(const FPCGAttributePropertyInputSelector& InSelector, const TSharedRef<PCGExData::FPointIO>& InPointIO)
		{
			PCGEX_MAKE_SHARED(Broadcaster, TAttributeBroadcaster<T>)
			if (!Broadcaster->Prepare(InSelector, InPointIO)) { return nullptr; }
			return Broadcaster;
		}

	protected:
		virtual void ResetMinMax() { PCGExMath::TypeMinMax(Min, Max); }
	};

#pragma endregion

#pragma region Attribute copy

	void CopyPoints(
		const PCGExData::FPointIO* Source,
		const PCGExData::FPointIO* Target,
		const TSharedPtr<const TArray<int32>>& SourceIndices,
		const int32 TargetIndex = 0,
		const bool bKeepSourceMetadataEntry = false);

#pragma endregion

	TSharedPtr<FAttributesInfos> GatherAttributeInfos(const FPCGContext* InContext, const FName InPinLabel, const FPCGExAttributeGatherDetails& InGatherDetails, const bool bThrowError);
}

#undef PCGEX_AAFLAG
