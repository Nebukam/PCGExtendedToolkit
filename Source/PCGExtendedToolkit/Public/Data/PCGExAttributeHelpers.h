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
#include "PCGExDataHelpers.h"
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
	void GetSources(TArray<FName>& OutNames) const;
};

#pragma endregion

namespace PCGEx
{
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

		bool InDataDomain() const { return Identifier.MetadataDomain.Flag == EPCGMetadataDomainFlag::Data; }
		int16 GetTypeId() const { return static_cast<int16>(UnderlyingType); }
		bool IsA(const int16 InType) const { return GetTypeId() == InType; }
		bool IsA(const EPCGMetadataTypes InType) const { return UnderlyingType == InType; }

		FString GetDisplayName() const { return FString(Identifier.Name.ToString() + FString::Printf(TEXT("( %d )"), UnderlyingType)); }
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
		TUniquePtr<const IPCGAttributeAccessor> InternalAccessor;

		TSharedPtr<PCGExData::IDataValue> DataValue;
		T TypedDataValue = T{};

		bool ApplySelector(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData)
		{
			ProcessingInfos = FAttributeProcessingInfos(InData, InSelector);
			if (!ProcessingInfos.bIsValid) { return false; }

			if (ProcessingInfos.bIsDataDomain)
			{
				PCGEx::ExecuteWithRightType(
					ProcessingInfos.Attribute->GetTypeId(), [&](auto DummyValue)
					{
						using T_REAL = decltype(DummyValue);
						DataValue = MakeShared<PCGExData::TDataValue<T_REAL>>(PCGExDataHelpers::ReadDataValue(static_cast<const FPCGMetadataAttribute<T_REAL>*>(ProcessingInfos.Attribute)));

						const FSubSelection& S = ProcessingInfos.SubSelection;
						TypedDataValue = S.bIsValid ? S.Get<T_REAL, T>(DataValue->GetValue<T_REAL>()) : PCGEx::Convert<T_REAL, T>(DataValue->GetValue<T_REAL>());
					});
			}
			else
			{
				InternalAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InData, ProcessingInfos.Selector);
				ProcessingInfos.bIsValid = InternalAccessor.IsValid();
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

		TArray<T> Values;
		mutable T Min = T{};
		mutable T Max = T{};

		bool bEnabled = true;

		bool IsUsable(int32 NumEntries) { return ProcessingInfos.bIsValid && Values.Num() >= NumEntries; }

		bool Prepare(const FPCGAttributePropertyInputSelector& InSelector, const TSharedRef<PCGExData::FPointIO>& InPointIO)
		{
			PointIO = InPointIO;
			PCGExMath::TypeMinMax(Min, Max);
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

			if (!ProcessingInfos.bIsValid)
			{
				PCGEX_SCOPE_LOOP(i) { Dump[i] = T{}; }
				return;
			}

			if (DataValue)
			{
				PCGEX_SCOPE_LOOP(i) { Dump[i] = TypedDataValue; }
			}
			else
			{
				TArrayView<T> DumpView = MakeArrayView(Dump.GetData() + Scope.Start, Scope.Count);
				const bool bSuccess = InternalAccessor->GetRange<T>(DumpView, Scope.Start, *PointIO->GetInKeys().Get(), EPCGAttributeAccessorFlags::AllowBroadcastAndConstructible);

				if (!bSuccess)
				{
					// TODO : Log error
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
			const int32 NumPoints = PointIO->GetNum(PCGExData::EIOSide::In);
			PCGEx::InitArray(Dump, NumPoints);

			PCGExMath::TypeMinMax(OutMin, OutMax);

			if (!ProcessingInfos.bIsValid)
			{
				for (int i = 0; i < NumPoints; i++) { Dump[i] = T{}; }
				return;
			}

			if (DataValue)
			{
				for (int i = 0; i < NumPoints; i++) { Dump[i] = TypedDataValue; }

				if (bCaptureMinMax)
				{
					OutMin = TypedDataValue;
					OutMax = TypedDataValue;
				}
			}
			else
			{
				const bool bSuccess = InternalAccessor->GetRange<T>(Dump, 0, *PointIO->GetInKeys().Get(), EPCGAttributeAccessorFlags::AllowBroadcastAndConstructible);
				if (!bSuccess)
				{
					// TODO : Log error
				}
				else if (bCaptureMinMax)
				{
					for (int i = 0; i < NumPoints; i++)
					{
						const T& V = Dump[i];
						OutMin = PCGExBlend::Min(OutMin, V);
						OutMax = PCGExBlend::Max(OutMax, V);
					}
				}
			}
		}

		void GrabUniqueValues(TSet<T>& OutUniqueValues)
		{
			if (!ProcessingInfos.bIsValid) { return; }

			if (DataValue)
			{
				OutUniqueValues.Add(TypedDataValue);
			}
			else
			{
				T TempMin = T{};
				T TempMax = T{};

				int32 NumPoints = PointIO->GetNum(PCGExData::EIOSide::In);
				OutUniqueValues.Reserve(OutUniqueValues.Num() + NumPoints);

				TArray<T> Dump;
				GrabAndDump(Dump, false, TempMin, TempMax);
				OutUniqueValues.Append(Dump);

				OutUniqueValues.Shrink();
			}
		}

		void Grab(const bool bCaptureMinMax = false)
		{
			GrabAndDump(Values, bCaptureMinMax, Min, Max);
		}

		T SoftGet(const PCGExData::FConstPoint& Point, const T& Fallback) const
		{
			if (!ProcessingInfos.bIsValid) { return Fallback; }
			if (DataValue) { return TypedDataValue; }

			T OutValue = Fallback;
			if (!InternalAccessor->Get<T>(OutValue, Point.Index, *PointIO->GetInKeys().Get(), EPCGAttributeAccessorFlags::AllowBroadcastAndConstructible)) { OutValue = Fallback; }
			return OutValue;
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
	};

#pragma endregion

	TSharedPtr<FAttributesInfos> GatherAttributeInfos(const FPCGContext* InContext, const FName InPinLabel, const FPCGExAttributeGatherDetails& InGatherDetails, const bool bThrowError);
}

#undef PCGEX_AAFLAG
