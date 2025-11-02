// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include <functional>
#include <type_traits>

#include "CoreMinimal.h"
#include "Metadata/PCGMetadataCommon.h"
#include "PCGExBroadcast.h"

#include "Details/PCGExMacros.h"
#include "Metadata/PCGAttributePropertySelector.h"

#include "PCGExAttributeHelpers.generated.h"

class UPCGMetadata;
struct FPCGContext;
class FPCGMetadataAttributeBase;
struct FPCGExContext;

namespace PCGExMT
{
	struct FScope;
}

// Primary template: assumes method does NOT exist
template <typename, typename = std::void_t<>>
struct has_GetTypeHash_v : std::false_type
{
};

// Specialization: chosen if T::Foo(int) is valid
template <typename T>
struct has_GetTypeHash_v<T, std::void_t<decltype(std::declval<T>().Foo(42))>> : std::true_type
{
};

namespace PCGExData
{
	struct FElement;
	struct FTaggedData;
	class FPointIO;
	class IDataValue;
	class FPointIOCollection;
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
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, HideInDetailPanel, Hidden, EditConditionHides, EditCondition="false"))
	FString TitlePropertyName;

	/** Attribute or $Property. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Attribute", DisplayPriority=0))
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

namespace PCGEx
{
#pragma region Attribute utils

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

		bool InDataDomain() const;
		int16 GetTypeId() const { return static_cast<int16>(UnderlyingType); }
		bool IsA(const int16 InType) const { return GetTypeId() == InType; }
		bool IsA(const EPCGMetadataTypes InType) const { return UnderlyingType == InType; }

		FString GetDisplayName() const;
		bool operator==(const FAttributeIdentity& Other) const;

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
		bool bIsDataDomain = false;

		FAttributeIdentity SourceIdentity = FAttributeIdentity();
		FAttributeIdentity TargetIdentity = FAttributeIdentity();

		const FPCGMetadataAttributeBase* Attribute = nullptr;

		FPCGAttributePropertyInputSelector Selector;
		FSubSelection SubSelection = FSubSelection();

		FAttributeProcessingInfos() = default;

		FAttributeProcessingInfos(const UPCGData* InData, const FPCGAttributePropertyInputSelector& InSelector);
		FAttributeProcessingInfos(const UPCGData* InData, const FName InAttributeName);

		operator const FPCGMetadataAttributeBase*() const { return Attribute; }

		operator EPCGMetadataTypes() const;

		operator EPCGAttributePropertySelection() const { return Selector.GetSelection(); }
		operator EPCGPointProperties() const { return Selector.GetPointProperty(); }
		operator EPCGExtraProperties() const { return Selector.GetExtraProperty(); }

	protected:
		void Init(const UPCGData* InData, const FPCGAttributePropertyInputSelector& InSelector);
	};

	TSharedPtr<FAttributesInfos> GatherAttributeInfos(const FPCGContext* InContext, const FName InPinLabel, const FPCGExAttributeGatherDetails& InGatherDetails, const bool bThrowError);

#pragma endregion

#pragma region Attribute Broadcaster

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

		const FPCGMetadataAttributeBase* GetAttribute() const;
		virtual EPCGMetadataTypes GetMetadataType() const;
		virtual FName GetName() const;
	};

	template <typename T>
	class TAttributeBroadcaster : public IAttributeBroadcaster
	{
	protected:
		TSharedPtr<IPCGAttributeAccessorKeys> Keys;
		TUniquePtr<const IPCGAttributeAccessor> InternalAccessor;

		TSharedPtr<PCGExData::IDataValue> DataValue;
		T TypedDataValue = T{};

		bool ApplySelector(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData);

	public:
		TAttributeBroadcaster() = default;

		virtual FName GetName() const override;

		TArray<T> Values;
		mutable T Min = T{};
		mutable T Max = T{};

		bool bEnabled = true;

		bool IsUsable(int32 NumEntries);

		bool Prepare(const FPCGAttributePropertyInputSelector& InSelector, const TSharedRef<PCGExData::FPointIO>& InPointIO);
		bool Prepare(const FName& InName, const TSharedRef<PCGExData::FPointIO>& InPointIO);
		bool Prepare(const FPCGAttributeIdentifier& InIdentifier, const TSharedRef<PCGExData::FPointIO>& InPointIO);

		bool PrepareForSingleFetch(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData, const TSharedPtr<IPCGAttributeAccessorKeys> InKeys = nullptr);
		bool PrepareForSingleFetch(const FName& InName, const UPCGData* InData, const TSharedPtr<IPCGAttributeAccessorKeys> InKeys = nullptr);
		bool PrepareForSingleFetch(const FPCGAttributeIdentifier& InIdentifier, const UPCGData* InData, const TSharedPtr<IPCGAttributeAccessorKeys> InKeys = nullptr);

		bool PrepareForSingleFetch(const FPCGAttributePropertyInputSelector& InSelector, const PCGExData::FTaggedData& InData);
		bool PrepareForSingleFetch(const FName& InName, const PCGExData::FTaggedData& InData);
		bool PrepareForSingleFetch(const FPCGAttributeIdentifier& InIdentifier, const PCGExData::FTaggedData& InData);

		/**
		 * Build and validate a property/attribute accessor for the selected
		 * @param Dump
		 * @param Scope
		 */
		void Fetch(TArray<T>& Dump, const PCGExMT::FScope& Scope);

		/**
		 * Build and validate a property/attribute accessor for the selected
		 * @param Dump
		 * @param bCaptureMinMax
		 * @param OutMin
		 * @param OutMax
		 */
		void GrabAndDump(TArray<T>& Dump, const bool bCaptureMinMax, T& OutMin, T& OutMax);

		void GrabUniqueValues(TSet<T>& OutUniqueValues);

		void Grab(const bool bCaptureMinMax = false);

		T FetchSingle(const PCGExData::FElement& Element, const T& Fallback) const;
		bool TryFetchSingle(const PCGExData::FElement& Element, T& OutValue) const;
		virtual EPCGMetadataTypes GetMetadataType() const override;
	};

	PCGEXTENDEDTOOLKIT_API
	TSharedPtr<IAttributeBroadcaster> MakeBroadcaster(const FName& InName, const TSharedRef<PCGExData::FPointIO>& InPointIO, bool bSingleFetch = false);

	PCGEXTENDEDTOOLKIT_API
	TSharedPtr<IAttributeBroadcaster> MakeBroadcaster(const FPCGAttributeIdentifier& InIdentifier, const TSharedRef<PCGExData::FPointIO>& InPointIO, bool bSingleFetch = false);

	PCGEXTENDEDTOOLKIT_API
	TSharedPtr<IAttributeBroadcaster> MakeBroadcaster(const FPCGAttributePropertyInputSelector& InSelector, const TSharedRef<PCGExData::FPointIO>& InPointIO, bool bSingleFetch = false);

	template <typename T>
	TSharedPtr<TAttributeBroadcaster<T>> MakeTypedBroadcaster(const FName& InName, const TSharedRef<PCGExData::FPointIO>& InPointIO, bool bSingleFetch = false);

	template <typename T>
	TSharedPtr<TAttributeBroadcaster<T>> MakeTypedBroadcaster(const FPCGAttributeIdentifier& InIdentifier, const TSharedRef<PCGExData::FPointIO>& InPointIO, bool bSingleFetch = false);

	template <typename T>
	TSharedPtr<TAttributeBroadcaster<T>> MakeTypedBroadcaster(const FPCGAttributePropertyInputSelector& InSelector, const TSharedRef<PCGExData::FPointIO>& InPointIO, bool bSingleFetch = false);


#pragma region externalization

#define PCGEX_TPL(_TYPE, _NAME, ...)\
extern template class TAttributeBroadcaster<_TYPE>; \
extern template TSharedPtr<TAttributeBroadcaster<_TYPE>> MakeTypedBroadcaster(const FName& InName, const TSharedRef<PCGExData::FPointIO>& InPointIO, bool bSingleFetch); \
extern template TSharedPtr<TAttributeBroadcaster<_TYPE>> MakeTypedBroadcaster(const FPCGAttributeIdentifier& InIdentifier, const TSharedRef<PCGExData::FPointIO>& InPointIO, bool bSingleFetch); \
extern template TSharedPtr<TAttributeBroadcaster<_TYPE>> MakeTypedBroadcaster(const FPCGAttributePropertyInputSelector& InSelector, const TSharedRef<PCGExData::FPointIO>& InPointIO, bool bSingleFetch);
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)

#undef PCGEX_TPL

#pragma endregion

#pragma endregion
}
