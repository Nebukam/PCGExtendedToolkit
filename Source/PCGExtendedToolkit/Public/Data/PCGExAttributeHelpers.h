// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include <functional>

#include "CoreMinimal.h"
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
	 * Bind & cache the current selector for a given point data
	 * @param InData 
	 * @return 
	 */
	virtual bool Validate(const UPCGData* InData);
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

	bool WantsRemappedOutput() const { return (bOutputToDifferentName && Source != GetOutputName()); }

	bool ValidateNames(FPCGExContext* InContext) const;
	bool ValidateNamesOrProperties(FPCGExContext* InContext) const;

	FName GetOutputName() const;

	FPCGAttributePropertyInputSelector GetSourceSelector() const;
	FPCGAttributePropertyInputSelector GetTargetSelector() const;
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
	const FName InvalidName = "INVALID_DATA";

#pragma region Attribute identity

	struct PCGEXTENDEDTOOLKIT_API FAttributeIdentity
	{
		FPCGAttributeIdentifier Identifier = NAME_None;
		EPCGMetadataTypes UnderlyingType = EPCGMetadataTypes::Unknown;
		bool bAllowsInterpolation = true;

		FAttributeIdentity() = default;

		FAttributeIdentity(const FPCGAttributeIdentifier& InIdentifier, const EPCGMetadataTypes InUnderlyingType, const bool InAllowsInterpolation)
			: Identifier(InIdentifier), UnderlyingType(InUnderlyingType), bAllowsInterpolation(InAllowsInterpolation)
		{
		}

		int16 GetTypeId() const { return static_cast<int16>(UnderlyingType); }
		bool IsA(const int16 InType) const { return GetTypeId() == InType; }
		bool IsA(const EPCGMetadataTypes InType) const { return UnderlyingType == InType; }

		FString GetDisplayName() const { return FString(Identifier.ToString() + FString::Printf(TEXT("( %d )"), UnderlyingType)); }
		bool operator==(const FAttributeIdentity& Other) const { return Identifier == Other.Identifier; }

		static void Get(const UPCGMetadata* InMetadata, TArray<FAttributeIdentity>& OutIdentities, const TSet<FName>* OptionalIgnoreList = nullptr);
		static void Get(const UPCGMetadata* InMetadata, TArray<FPCGAttributeIdentifier>& OutIdentifiers, TMap<FPCGAttributeIdentifier, FAttributeIdentity>& OutIdentities, const TSet<FName>* OptionalIgnoreList = nullptr);
		static bool Get(const UPCGData* InData, const FPCGAttributePropertyInputSelector& InSelector, FAttributeIdentity& OutIdentity);

		using FForEachFunc = std::function<void (const FAttributeIdentity&, const int32)>;
		static int32 ForEach(const UPCGMetadata* InMetadata, FForEachFunc&& Func);
	};

	class FAttributesInfos : public TSharedFromThis<FAttributesInfos>
	{
	public:
		TMap<FPCGAttributeIdentifier, int32> Map;
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

#pragma endregion

#pragma region Attribute Broadcaster

	template <bool bInitialized>
	static FName GetLongNameFromSelector(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData)
	{
		// This return a domain-less unique identifier for the provided selector
		// It's mostly used to create uniquely identified value buffers

		if (!InData) { return InvalidName; }

		if constexpr (bInitialized)
		{
			if (InSelector.GetExtraNames().IsEmpty()) { return FName(InSelector.GetName().ToString()); }
			return FName(InSelector.GetName().ToString() + TEXT(".") + FString::Join(InSelector.GetExtraNames(), TEXT(".")));
		}
		else
		{
			if (InSelector.GetSelection() == EPCGAttributePropertySelection::Attribute && InSelector.GetName() == "@Last")
			{
				return GetLongNameFromSelector<true>(InSelector.CopyAndFixLast(InData), InData);
			}

			return GetLongNameFromSelector<true>(InSelector, InData);
		}
	}

	static FPCGAttributeIdentifier GetIdentifierFromSelector(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData)
	{
		// This return an identifier suitable to be used for data facade

		FPCGAttributeIdentifier Identifier;

		if (!InData) { return FPCGAttributeIdentifier(InvalidName, EPCGMetadataDomainFlag::Invalid); }

		FPCGAttributePropertyInputSelector FixedSelector = InSelector.CopyAndFixLast(InData);

		if (InSelector.GetExtraNames().IsEmpty()) { Identifier.Name = FixedSelector.GetName(); }
		else { Identifier.Name = FName(FixedSelector.GetName().ToString() + TEXT(".") + FString::Join(FixedSelector.GetExtraNames(), TEXT("."))); }

		Identifier.MetadataDomain = InData->GetMetadataDomainIDFromSelector(FixedSelector);

		return Identifier;
	}

	PCGEXTENDEDTOOLKIT_API
	FString GetSelectorDisplayName(const FPCGAttributePropertyInputSelector& InSelector);

	class PCGEXTENDEDTOOLKIT_API IAttributeBroadcaster : public TSharedFromThis<IAttributeBroadcaster>
	{
	public:
		FAttributeProcessingInfos ProcessingInfos = FAttributeProcessingInfos();
		IAttributeBroadcaster() = default;
		virtual ~IAttributeBroadcaster() = default;

		IAttributeBroadcaster(IAttributeBroadcaster&&) = default;
		IAttributeBroadcaster& operator=(IAttributeBroadcaster&&) = default;

		IAttributeBroadcaster(const IAttributeBroadcaster&) = delete;
		IAttributeBroadcaster& operator=(const IAttributeBroadcaster&) = delete;
	};

	template <typename T>
	class TAttributeBroadcaster : public IAttributeBroadcaster
	{
	protected:
		TSharedPtr<PCGExData::FPointIO> PointIO;
		bool bMinMaxDirty = true;
		bool bNormalized = false;
		TUniquePtr<const IPCGAttributeAccessor> InternalAccessor;

		EPCGExTransformComponent Component = EPCGExTransformComponent::Position;
		EPCGExAxis Axis = EPCGExAxis::Forward;
		EPCGExSingleField Field = EPCGExSingleField::X;

		bool ApplySelector(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData)
		{
			ProcessingInfos = FAttributeProcessingInfos(InData, InSelector);

			if (!ProcessingInfos.bIsValid) { return false; }

			FullName = GetLongNameFromSelector<true>(ProcessingInfos.Selector, InData);

			if (ProcessingInfos.Attribute)
			{
				PCGEx::ExecuteWithRightType(
					ProcessingInfos.Attribute->GetTypeId(), [&](auto DummyValue)
					{
						using RawT = decltype(DummyValue);
						InternalAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(static_cast<const FPCGMetadataAttribute<RawT>*>(ProcessingInfos.Attribute), Cast<UPCGSpatialData>(InData)->Metadata);
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
			check(Dump.Num() == PointIO->GetNum(PCGExData::EIOSide::In)) // Dump target should be initialized at full length before using Fetch

			const UPCGBasePointData* InData = PointIO->GetIn();

			if (ProcessingInfos == EPCGAttributePropertySelection::Attribute)
			{
				PCGEx::ExecuteWithRightType(
					ProcessingInfos.Attribute->GetTypeId(), [&](auto DummyValue)
					{
						using RawT = decltype(DummyValue);

						TArray<RawT> RawValues;
						PCGEx::InitArray(RawValues, Scope.Count);

						TArrayView<RawT> RawView(RawValues);
						if (!InternalAccessor->GetRange<RawT>(RawView, Scope.Start, *PointIO->GetInKeys().Get(), EPCGAttributeAccessorFlags::AllowBroadcast))
						{
							// TODO : Log error
						}

						const FSubSelection& S = ProcessingInfos.SubSelection;
						if (S.bIsValid) { for (int i = 0; i < Scope.Count; i++) { Dump[Scope.Start + i] = S.Get<RawT, T>(RawValues[i]); } }
						else { for (int i = 0; i < Scope.Count; i++) { Dump[Scope.Start + i] = PCGEx::Convert<RawT, T>(RawValues[i]); } }
					});
			}
			else if (ProcessingInfos == EPCGAttributePropertySelection::Property)
			{
				const FSubSelection& S = ProcessingInfos.SubSelection;
				const EPCGPointProperties Property = static_cast<EPCGPointProperties>(ProcessingInfos);

#define PCGEX_GET_BY_ACCESSOR(_ACCESSOR, _TYPE)\
if (S.bIsValid) { PCGEX_SCOPE_LOOP(Index) { Dump[Index] =S.Get<_TYPE, T>(InData->_ACCESSOR); } } \
else{ PCGEX_SCOPE_LOOP(Index){ Dump[Index] =PCGEx::Convert<_TYPE, T>(InData->_ACCESSOR); } }
				PCGEX_IFELSE_GETPOINTPROPERTY(Property, PCGEX_GET_BY_ACCESSOR)
#undef PCGEX_GET_BY_ACCESSOR
			}
			else if (ProcessingInfos == EPCGAttributePropertySelection::ExtraProperty)
			{
				const FSubSelection& S = ProcessingInfos.SubSelection;
				switch (static_cast<EPCGExtraProperties>(ProcessingInfos))
				{
				case EPCGExtraProperties::Index:
					if (S.bIsValid) { PCGEX_SCOPE_LOOP(i) { Dump[i] = S.Get<int32, T>(i); } }
					else { PCGEX_SCOPE_LOOP(i) { Dump[i] = PCGEx::Convert<int32, T>(i); } }
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
				int32 NumPoints = PointIO->GetNum(PCGExData::EIOSide::In);
				PCGEx::InitArray(Dump, NumPoints);
				for (int i = 0; i < NumPoints; i++) { Dump[i] = T{}; }
				return;
			}

			const UPCGBasePointData* InData = PointIO->GetIn();

			int32 NumPoints = PointIO->GetNum(PCGExData::EIOSide::In);
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
						if (!InternalAccessor->GetRange<RawT>(View, 0, *PointIO->GetInKeys().Get(), EPCGAttributeAccessorFlags::AllowBroadcast))
						{
							// TODO : Log error
						}

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
			else if (ProcessingInfos == EPCGAttributePropertySelection::Property)
			{
				const FSubSelection& S = ProcessingInfos.SubSelection;
				const EPCGPointProperties Property = static_cast<EPCGPointProperties>(ProcessingInfos);

#define PCGEX_GET_BY_ACCESSOR(_ACCESSOR, _TYPE)\
				if (bCaptureMinMax) {\
				if (S.bIsValid){ for (int Index = 0; Index < NumPoints; Index++) { T V = S.Get<_TYPE, T>(InData->_ACCESSOR); OutMin = PCGExBlend::Min(V, OutMin); OutMax = PCGExBlend::Max(V, OutMax); Dump[Index] = V; } } \
				else { for (int Index = 0; Index < NumPoints; Index++) { T V = PCGEx::Convert<_TYPE, T>(InData->_ACCESSOR); OutMin = PCGExBlend::Min(V, OutMin); OutMax = PCGExBlend::Max(V, OutMax); Dump[Index] = V; } } \
				}else{ \
				if (S.bIsValid){ for (int Index = 0; Index < NumPoints; Index++) { Dump[Index] = S.Get<_TYPE, T>(InData->_ACCESSOR); } } \
				else{ for (int Index = 0; Index < NumPoints; Index++) { Dump[Index] = PCGEx::Convert<_TYPE, T>(InData->_ACCESSOR); } } \
				}
				PCGEX_IFELSE_GETPOINTPROPERTY(Property, PCGEX_GET_BY_ACCESSOR)
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

			const UPCGBasePointData* InData = PointIO->GetIn();

			int32 NumPoints = PointIO->GetNum(PCGExData::EIOSide::In);
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
						if (!InternalAccessor->GetRange<RawT>(View, 0, *PointIO->GetInKeys().Get(), EPCGAttributeAccessorFlags::AllowBroadcast))
						{
							// TODO : Log error
						}

						const FSubSelection& S = ProcessingInfos.SubSelection;
						if (S.bIsValid) { for (int i = 0; i < NumPoints; i++) { Dump.Add(S.Get<RawT, T>(RawValues[i])); } }
						else { for (int i = 0; i < NumPoints; i++) { Dump.Add(PCGEx::Convert<RawT, T>(RawValues[i])); } }

						RawValues.Empty();
					});
			}
			else if (ProcessingInfos == EPCGAttributePropertySelection::Property)
			{
				const FSubSelection& S = ProcessingInfos.SubSelection;
				const EPCGPointProperties Property = static_cast<EPCGPointProperties>(ProcessingInfos);

#define PCGEX_GET_BY_ACCESSOR(_ACCESSOR, _TYPE)\
				if (S.bIsValid) { for (int Index = 0; Index < NumPoints; Index++) { Dump.Add(S.Get<_TYPE, T>(InData->_ACCESSOR)); } } \
				else{ for (int Index = 0; Index < NumPoints; Index++) { Dump.Add(PCGEx::Convert<_TYPE, T>(InData->_ACCESSOR)); } }
				PCGEX_IFELSE_GETPOINTPROPERTY(Property, PCGEX_GET_BY_ACCESSOR)
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

		T SoftGet(const PCGExData::FConstPoint& Point, const T& Fallback) const
		{
			if (!ProcessingInfos.bIsValid) { return Fallback; }

			const int32 Index = Point.Index;
			const EPCGPointProperties Property = static_cast<EPCGPointProperties>(ProcessingInfos);

			switch (static_cast<EPCGAttributePropertySelection>(ProcessingInfos))
			{
			case EPCGAttributePropertySelection::Attribute:

				return PCGMetadataAttribute::CallbackWithRightType(
					ProcessingInfos.Attribute->GetTypeId(),
					[&](auto DummyValue) -> T
					{
						using RawT = decltype(DummyValue);
						const FPCGMetadataAttribute<RawT>* TypedAttribute = static_cast<const FPCGMetadataAttribute<RawT>*>(ProcessingInfos.Attribute);
						return ProcessingInfos.SubSelection.Get<RawT, T>(TypedAttribute->GetValueFromItemKey(Point.GetMetadataEntry()));
					});

			case EPCGAttributePropertySelection::Property:

#define PCGEX_GET_BY_ACCESSOR(_ACCESSOR, _TYPE) return ProcessingInfos.SubSelection.Get<_TYPE, T>(Point.Data->_ACCESSOR);
				PCGEX_IFELSE_GETPOINTPROPERTY(Property, PCGEX_GET_BY_ACCESSOR)
#undef PCGEX_GET_BY_ACCESSOR

				return T{};

			case EPCGAttributePropertySelection::ExtraProperty:

				switch (static_cast<EPCGExtraProperties>(ProcessingInfos))
				{
				case EPCGExtraProperties::Index:
					return ProcessingInfos.SubSelection.Get<T>(Index);
				default:
					return Fallback;
				}

			default:
				return Fallback;
			}
		}

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

	TSharedPtr<FAttributesInfos> GatherAttributeInfos(const FPCGContext* InContext, const FName InPinLabel, const FPCGExAttributeGatherDetails& InGatherDetails, const bool bThrowError);
}

#undef PCGEX_AAFLAG
