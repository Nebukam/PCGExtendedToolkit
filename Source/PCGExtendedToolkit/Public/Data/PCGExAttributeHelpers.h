// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "PCGExMacros.h"
#include "PCGEx.h"
#include "PCGExMath.h"
#include "PCGExMT.h"
#include "PCGExPointIO.h"
#include "PCGParamData.h"
#include "Metadata/Accessors/PCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGCustomAccessor.h"

#include "PCGExAttributeHelpers.generated.h"

#pragma region Input Configs

struct FPCGExAttributeGatherDetails;

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputConfig
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

public:
	virtual ~FPCGExInputConfig()
	{
		Attribute = nullptr;
	};

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (HideInDetailPanel, Hidden, EditConditionHides, EditCondition="false"))
	FString TitlePropertyName;

	/** Attribute or $Property. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayPriority=0))
	FPCGAttributePropertyInputSelector Selector;

	FPCGMetadataAttributeBase* Attribute = nullptr;
	int16 UnderlyingType = static_cast<int16>(EPCGMetadataTypes::Unknown);

	FPCGAttributePropertyInputSelector& GetMutableSelector() { return Selector; }

public:
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

	struct PCGEXTENDEDTOOLKIT_API FAttributeIdentity
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

		void Append(const FAttributesInfos* Other, const FPCGExAttributeGatherDetails& InGatherDetails, TSet<FName>& OutTypeMismatch);
		void Update(const FAttributesInfos* Other, const FPCGExAttributeGatherDetails& InGatherDetails, TSet<FName>& OutTypeMismatch);

		~FAttributesInfos()
		{
			Map.Empty();
			Identities.Empty();
			Attributes.Empty();
		}

		static FAttributesInfos* Get(const UPCGMetadata* InMetadata);
	};

	static void GatherAttributes(
		FAttributesInfos* OutInfos, const FPCGContext* InContext, FName InputLabel,
		const FPCGExAttributeGatherDetails& InDetails, TSet<FName>& Mismatches)
	{
		TArray<FPCGTaggedData> InputData = InContext->InputData.GetInputsByPin(InputLabel);
		for (const FPCGTaggedData& TaggedData : InputData)
		{
			if (const UPCGParamData* AsParamData = Cast<UPCGParamData>(TaggedData.Data))
			{
				FAttributesInfos* Infos = FAttributesInfos::Get(AsParamData->Metadata);
				OutInfos->Append(Infos, InDetails, Mismatches);
				PCGEX_DELETE(Infos);
				continue;
			}

			if (const UPCGSpatialData* AsSpatialData = Cast<UPCGSpatialData>(TaggedData.Data))
			{
				FAttributesInfos* Infos = FAttributesInfos::Get(AsSpatialData->Metadata);
				OutInfos->Append(Infos, InDetails, Mismatches);
				PCGEX_DELETE(Infos);
			}
		}
	}

	static FAttributesInfos* GatherAttributes(
		const FPCGContext* InContext, FName InputLabel,
		const FPCGExAttributeGatherDetails& InDetails, TSet<FName>& Mismatches)
	{
		FAttributesInfos* OutInfos = new FAttributesInfos();
		GatherAttributes(OutInfos, InContext, InputLabel, InDetails, Mismatches);
		return OutInfos;
	}

#pragma endregion

#pragma region Accessors
#define PCGEX_AAFLAG EPCGAttributeAccessorFlags::AllowBroadcast

	class PCGEXTENDEDTOOLKIT_API FAttributeAccessorGeneric
	{
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FAttributeAccessorBase : public FAttributeAccessorGeneric
	{
	protected:
		FPCGMetadataAttribute<T>* Attribute = nullptr;
		int32 NumEntries = -1;
		TUniquePtr<FPCGAttributeAccessor<T>> Accessor;
		FPCGAttributeAccessorKeysPoints* InternalKeys = nullptr;
		IPCGAttributeAccessorKeys* Keys = nullptr;

		void Flush()
		{
			if (Accessor) { Accessor.Reset(); }
			PCGEX_DELETE(InternalKeys)
			Keys = nullptr;
			Attribute = nullptr;
		}

	public:
		FAttributeAccessorBase(const UPCGPointData* InData, FPCGMetadataAttributeBase* InAttribute, FPCGAttributeAccessorKeysPoints* InKeys)
		{
			Flush();
			check(InKeys) // Someone forgot to CreateKeys. Yes, it's you.
			Attribute = static_cast<FPCGMetadataAttribute<T>*>(InAttribute);
			Accessor = MakeUnique<FPCGAttributeAccessor<T>>(Attribute, InData->Metadata);
			NumEntries = InKeys->GetNum();
			Keys = InKeys;
		}

		T GetDefaultValue() const { return Attribute->GetValue(PCGInvalidEntryKey); }
		bool GetAllowsInterpolation() const { return Attribute->AllowsInterpolation(); }

		FPCGMetadataAttributeBase* GetAttribute() { return Attribute; }
		FPCGMetadataAttribute<T>* GetTypedAttribute() { return Attribute; }

		int32 GetNum() const { return NumEntries; }

		virtual T Get(const int32 Index)
		{
			if (T OutValue; Get(OutValue, Index)) { return OutValue; }
			return GetDefaultValue();
		}

		bool Get(T& OutValue, const int32 Index) const
		{
			TArrayView<T> Temp(&OutValue, 1);
			return GetRange(TArrayView<T>(&OutValue, 1), Index);
		}

		bool GetRange(TArrayView<T> OutValues, int32 Index = 0, FPCGAttributeAccessorKeysPoints* InKeys = nullptr) const
		{
			return Accessor->GetRange(OutValues, Index, InKeys ? *InKeys : *Keys, PCGEX_AAFLAG);
		}

		bool GetRange(TArray<T>& OutValues, const int32 Index = 0, FPCGAttributeAccessorKeysPoints* InKeys = nullptr, int32 Count = -1) const
		{
			PCGEX_SET_NUM_UNINITIALIZED(OutValues, Count == -1 ? NumEntries - Index : Count)
			TArrayView<T> View(OutValues);
			return Accessor->GetRange(View, Index, InKeys ? *InKeys : *Keys, PCGEX_AAFLAG);
		}

		bool GetScope(TArray<T>& OutValues, const uint64 Scope, FPCGAttributeAccessorKeysPoints* InKeys = nullptr) const
		{
			const int32 StartIndex = H64A(Scope);
			TArrayView<T> View = MakeArrayView(OutValues.GetData() + StartIndex, H64B(Scope));
			return Accessor->GetRange(View, StartIndex, InKeys ? *InKeys : *Keys, PCGEX_AAFLAG);
		}

		bool Set(const T& InValue, const int32 Index) { return SetRange(TArrayView<const T>(&InValue, 1), Index); }


		bool SetRange(TArrayView<const T> InValues, int32 Index = 0, FPCGAttributeAccessorKeysPoints* InKeys = nullptr)
		{
			return Accessor->SetRange(InValues, Index, InKeys ? *InKeys : *Keys, PCGEX_AAFLAG);
		}

		bool SetRange(TArray<T>& InValues, int32 Index = 0, FPCGAttributeAccessorKeysPoints* InKeys = nullptr)
		{
			TArrayView<const T> View(InValues);
			return Accessor->SetRange(View, Index, InKeys ? *InKeys : *Keys, PCGEX_AAFLAG);
		}

		virtual ~FAttributeAccessorBase()
		{
			Flush();
		}
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FAttributeAccessor final : public FAttributeAccessorBase<T>
	{
	public:
		FAttributeAccessor(const UPCGPointData* InData, FPCGMetadataAttributeBase* InAttribute, FPCGAttributeAccessorKeysPoints* InKeys)
			: FAttributeAccessorBase<T>(InData, InAttribute, InKeys)
		{
		}

		FAttributeAccessor(UPCGPointData* InData, FPCGMetadataAttributeBase* InAttribute)
			: FAttributeAccessorBase<T>()
		{
			this->Cleanup();
			this->Attribute = static_cast<FPCGMetadataAttribute<T>*>(InAttribute);
			this->Accessor = MakeUnique<FPCGAttributeAccessor<T>>(this->Attribute, InData->Metadata);

			const TArrayView<FPCGPoint> View(InData->GetMutablePoints());
			this->InternalKeys = new FPCGAttributeAccessorKeysPoints(View);

			this->NumEntries = this->InternalKeys->GetNum();
			this->Keys = this->InternalKeys;
		}


		static FAttributeAccessor* FindOrCreate(
			UPCGPointData* InData, FName AttributeName,
			const T& DefaultValue = T{}, bool bAllowsInterpolation = true, bool bOverrideParent = true, bool bOverwriteIfTypeMismatch = true)
		{
			FPCGMetadataAttribute<T>* InAttribute = InData->Metadata->FindOrCreateAttribute(
				AttributeName, DefaultValue,
				bAllowsInterpolation, bOverrideParent, bOverwriteIfTypeMismatch);

			return new FAttributeAccessor<T>(InData, InAttribute);
		}

		static FAttributeAccessor* FindOrCreate(
			UPCGPointData* InData, FName AttributeName, FPCGAttributeAccessorKeysPoints* InKeys,
			const T& DefaultValue = T{}, bool bAllowsInterpolation = true, bool bOverrideParent = true, bool bOverwriteIfTypeMismatch = true)
		{
			FPCGMetadataAttribute<T>* InAttribute = InData->Metadata->FindOrCreateAttribute(
				AttributeName, DefaultValue,
				bAllowsInterpolation, bOverrideParent, bOverwriteIfTypeMismatch);

			return new FAttributeAccessor<T>(InData, InAttribute, InKeys);
		}

		static FAttributeAccessor* FindOrCreate(
			PCGExData::FPointIO* InPointIO, FName AttributeName,
			const T& DefaultValue = T{}, bool bAllowsInterpolation = true, bool bOverrideParent = true, bool bOverwriteIfTypeMismatch = true)
		{
			UPCGPointData* InData = InPointIO->GetOut();
			FPCGMetadataAttribute<T>* InAttribute = InData->Metadata->FindOrCreateAttribute(
				AttributeName, DefaultValue,
				bAllowsInterpolation, bOverrideParent, bOverwriteIfTypeMismatch);

			return new FAttributeAccessor<T>(InData, InAttribute, InPointIO->CreateOutKeys());
		}
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FConstAttributeAccessor final : public FAttributeAccessorBase<T>
	{
	public:
		FConstAttributeAccessor(const UPCGPointData* InData, FPCGMetadataAttributeBase* InAttribute, FPCGAttributeAccessorKeysPoints* InKeys)
			: FAttributeAccessorBase<T>(InData, InAttribute, InKeys)
		{
		}

		FConstAttributeAccessor(const UPCGPointData* InData, FPCGMetadataAttributeBase* InAttribute): FAttributeAccessorBase<T>()
		{
			this->Cleanup();
			this->Attribute = static_cast<FPCGMetadataAttribute<T>*>(InAttribute);
			this->Accessor = MakeUnique<FPCGAttributeAccessor<T>>(this->Attribute, InData->Metadata);

			this->InternalKeys = new FPCGAttributeAccessorKeysPoints(InData->GetPoints());

			this->NumEntries = this->InternalKeys->GetNum();
			this->Keys = this->InternalKeys;
		}

		static FConstAttributeAccessor* Find(const PCGExData::FPointIO* InPointIO, const FName AttributeName)
		{
			const UPCGPointData* InData = InPointIO->GetIn();
			if (FPCGMetadataAttributeBase* InAttribute = InData->Metadata->GetMutableAttribute(AttributeName))
			{
				return new FConstAttributeAccessor(InData, InAttribute, InPointIO->GetInKeys());
			}
			return nullptr;
		}
	};

	class PCGEXTENDEDTOOLKIT_API FAAttributeIO
	{
	public:
		FName Name = NAME_None;
		int16 UnderlyingType = static_cast<int16>(EPCGMetadataTypes::Unknown);

		explicit FAAttributeIO(const FName InName):
			Name(InName)
		{
		}

		virtual ~FAAttributeIO()
		{
		}

		virtual void Fetch(const int32 StartIndex, const int32 Count)
		{
		}

		void Fetch(const uint64 Scope) { Fetch(H64A(Scope), H64B(Scope)); }
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FAttributeIOBase : public FAAttributeIO
	{
	public:
		TArray<T> Values;
		FAttributeAccessorBase<T>* Accessor = nullptr;

		explicit FAttributeIOBase(const FName InName):
			FAAttributeIO(InName)
		{
			Values.Empty();
		}

		FORCEINLINE T GetDefaultValue() const { return Accessor->GetDefaultValue(); }
		FORCEINLINE T GetZeroedValue() const { return T{}; }
		FORCEINLINE bool GetAllowsInterpolation() const { return Accessor->GetAllowsInterpolation(); }

		FORCEINLINE void SetNum(int32 Num)
		{
			Values.Reserve(Num);
			Values.SetNumZeroed(Num);
		}

		virtual bool Bind(PCGExData::FPointIO* PointIO) = 0;

		FORCEINLINE T operator[](int32 Index) const { return this->Values[Index]; }

		FORCEINLINE bool IsValid() { return Accessor != nullptr; }

		virtual void Fetch(const int32 StartIndex, const int32 Count) override
		{
			this->Accessor->GetScope(this->Values, H64(StartIndex, Count));
		}

		virtual ~FAttributeIOBase() override
		{
			PCGEX_DELETE(Accessor)
			Values.Empty();
		}
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API TFAttributeWriter final : public FAttributeIOBase<T>
	{
		T DefaultValue;
		bool bAllowsInterpolation;
		bool bOverrideParent;
		bool bOverwriteIfTypeMismatch;

	public:
		explicit TFAttributeWriter(const FName& InName)
			: FAttributeIOBase<T>(InName),
			  DefaultValue(T{}),
			  bAllowsInterpolation(true),
			  bOverrideParent(true),
			  bOverwriteIfTypeMismatch(true)
		{
		}

		TFAttributeWriter(
			const FName& InName,
			const T& InDefaultValue,
			const bool AllowsInterpolation = true,
			const bool OverrideParent = true,
			const bool OverwriteIfTypeMismatch = true)
			: FAttributeIOBase<T>(InName),
			  DefaultValue(InDefaultValue),
			  bAllowsInterpolation(AllowsInterpolation),
			  bOverrideParent(OverrideParent),
			  bOverwriteIfTypeMismatch(OverwriteIfTypeMismatch)
		{
		}

		virtual bool Bind(PCGExData::FPointIO* PointIO) override
		{
			PCGEX_DELETE(this->Accessor)
			this->Accessor = FAttributeAccessor<T>::FindOrCreate(
				PointIO, this->Name, DefaultValue,
				bAllowsInterpolation, bOverrideParent, bOverwriteIfTypeMismatch);
			this->UnderlyingType = PointIO->GetOut()->Metadata->GetConstAttribute(this->Name)->GetTypeId();
			return true;
		}

		bool BindAndGet(PCGExData::FPointIO* PointIO)
		{
			if (Bind(PointIO))
			{
				this->SetNum(PointIO->GetNum(PCGExData::ESource::Out));
				this->Accessor->GetRange(this->Values);
				return true;
			}
			return false;
		}

		bool BindAndSetNumUninitialized(PCGExData::FPointIO* PointIO)
		{
			if (Bind(PointIO))
			{
				PCGEX_SET_NUM_UNINITIALIZED(this->Values, PointIO->GetNum(PCGExData::ESource::Out))
				return true;
			}
			return false;
		}

		T& operator[](int32 Index) { return this->Values[Index]; }

		void Write()
		{
			if (this->Values.IsEmpty()) { return; }
			this->Accessor->SetRange(this->Values);
		}

		void Write(const TArrayView<const int32>& InIndices)
		{
			if (this->Values.IsEmpty()) { return; }
			for (int32 Index : InIndices) { this->Accessor->Set(this->Values[Index], Index); }
		}

		void Write(const uint64 Scope)
		{
			if (this->Values.IsEmpty()) { return; }
			uint32 Start;
			uint32 Count;
			H64(Scope, Start, Count);
			this->Accessor->SetRange(MakeArrayView(this->Values.GetData() + Start, Count), Start);
		}
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API TFAttributeReader final : public FAttributeIOBase<T>
	{
	public:
		explicit TFAttributeReader(const FName& InName)
			: FAttributeIOBase<T>(InName)
		{
		}

		virtual bool Bind(PCGExData::FPointIO* PointIO) override
		{
			PCGEX_DELETE(this->Accessor)
			this->Accessor = FConstAttributeAccessor<T>::Find(PointIO, this->Name);
			if (!this->Accessor) { return false; }
			this->SetNum(PointIO->GetNum());
			this->Accessor->GetRange(this->Values);
			this->UnderlyingType = PointIO->GetIn()->Metadata->GetConstAttribute(this->Name)->GetTypeId();
			return true;
		}

		bool BindForFetch(PCGExData::FPointIO* PointIO)
		{
			PCGEX_DELETE(this->Accessor)
			this->Accessor = FConstAttributeAccessor<T>::Find(PointIO, this->Name);
			if (!this->Accessor) { return false; }
			this->SetNum(PointIO->GetNum());
			this->UnderlyingType = PointIO->GetIn()->Metadata->GetConstAttribute(this->Name)->GetTypeId();
			return true;
		}
	};

#pragma endregion

#pragma region Local Attribute Inputs

	class PCGEXTENDEDTOOLKIT_API FAttributeGetterBase
	{
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FAttributeGetter : public FAttributeGetterBase
	{
	protected:
		bool bMinMaxDirty = true;
		bool bNormalized = false;
		FPCGAttributePropertyInputSelector FetchSelector;
		IPCGAttributeAccessor* FetchAccessor = nullptr;

	public:
		FAttributeGetter()
		{
		}

		FAttributeGetter(const FAttributeGetter& Other)
			: Component(Other.Component),
			  Axis(Other.Axis),
			  Field(Other.Field)
		{
		}

		virtual ~FAttributeGetter()
		{
			PCGEX_DELETE(FetchAccessor)
			FAttributeGetter<T>::Cleanup();
		}

		virtual EPCGMetadataTypes GetType() { return EPCGMetadataTypes::Unknown; }

		EPCGExTransformComponent Component = EPCGExTransformComponent::Position;
		bool bUseAxis = false;
		EPCGExAxis Axis = EPCGExAxis::Forward;
		EPCGExSingleField Field = EPCGExSingleField::X;

		FName FullName = NAME_None;

		TArray<T> Values;
		mutable T Min = T{};
		mutable T Max = T{};

		bool bEnabled = true;
		bool bValid = false;

		EPCGPointProperties PointProperty = EPCGPointProperties::Position;
		EPCGAttributePropertySelection Selection = EPCGAttributePropertySelection::Attribute;
		FPCGMetadataAttributeBase* Attribute = nullptr;

		bool IsUsable(int32 NumEntries) { return bEnabled && bValid && Values.Num() >= NumEntries; }

		FPCGExInputConfig Config;

		virtual void Cleanup()
		{
			Values.Empty();
		}

		bool InitForFetch(const PCGExData::FPointIO* PointIO)
		{
			ResetMinMax();
			bMinMaxDirty = true;
			bNormalized = false;

			bValid = false;
			const UPCGPointData* InData = PointIO->GetIn();

			TArray<FString> ExtraNames;
			FetchSelector = CopyAndFixLast(Config.Selector, InData, ExtraNames);
			if (!FetchSelector.IsValid()) { return false; }

			ProcessExtraNames(FetchSelector.GetName(), ExtraNames);
			Selection = FetchSelector.GetSelection();

			if (Selection == EPCGAttributePropertySelection::Attribute)
			{
				Attribute = InData->Metadata->GetMutableAttribute(FetchSelector.GetName());
				if (!Attribute) { return false; }

				PCGMetadataAttribute::CallbackWithRightType(
					Attribute->GetTypeId(),
					[&](auto DummyValue) -> void
					{
						using RawT = decltype(DummyValue);
						FetchAccessor = new FPCGAttributeAccessor<RawT>(static_cast<FPCGMetadataAttribute<RawT>*>(Attribute), InData->Metadata);
					});
			}

			return true;
		}

		/**
		 * Build and validate a property/attribute accessor for the selected
		 * @param PointIO
		 * @param Dump
		 * @param StartIndex
		 * @param Count
		 */
		bool Fetch(PCGExData::FPointIO* PointIO, TArray<T>& Dump, const int32 StartIndex, const int32 Count)
		{
			check(Dump.Num() == PointIO->GetNum(PCGExData::ESource::In)) // Dump target should be initialized at full length before using Fetch

			const UPCGPointData* InData = PointIO->GetIn();

			if (Selection == EPCGAttributePropertySelection::Attribute)
			{
				if (!FetchAccessor || !Attribute) { return false; }

				PCGMetadataAttribute::CallbackWithRightType(
					Attribute->GetTypeId(),
					[&](auto DummyValue) -> void
					{
						using RawT = decltype(DummyValue);
						TArray<RawT> RawValues;

						PCGEX_SET_NUM_UNINITIALIZED(RawValues, Count)

						FPCGAttributeAccessor<RawT>* Accessor = static_cast<FPCGAttributeAccessor<RawT>*>(FetchAccessor);
						IPCGAttributeAccessorKeys* Keys = PointIO->CreateInKeys();

						TArrayView<RawT> RawView(RawValues);
						Accessor->GetRange(RawView, StartIndex, *Keys, PCGEX_AAFLAG);

						for (int i = 0; i < Count; i++) { Dump[StartIndex + i] = Convert(RawValues[i]); }
						RawValues.Empty();
					});

				bValid = true;
			}
			else if (Selection == EPCGAttributePropertySelection::PointProperty)
			{
				const TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InData, FetchSelector);
				const TArray<FPCGPoint>& InPoints = InData->GetPoints();
				const int32 LastIndex = StartIndex + Count;
#define PCGEX_GET_BY_ACCESSOR(_ENUM, _ACCESSOR) case _ENUM: for (int i = StartIndex; i < LastIndex; i++) { Dump[i] = Convert(InPoints[i]._ACCESSOR); } break;

				switch (Config.Selector.GetPointProperty()) { PCGEX_FOREACH_POINTPROPERTY(PCGEX_GET_BY_ACCESSOR) }
#undef PCGEX_GET_BY_ACCESSOR
				bValid = true;
			}
			else
			{
				//TODO: Support extra properties
			}

			return bValid;
		}


		/**
		 * Build and validate a property/attribute accessor for the selected
		 * @param PointIO
		 * @param Dump
		 * @param bCaptureMinMax
		 * @param OutMin
		 * @param OutMax
		 */
		bool GrabAndDump(const PCGExData::FPointIO* PointIO, TArray<T>& Dump, const bool bCaptureMinMax, T& OutMin, T& OutMax)
		{
			ResetMinMax();
			bMinMaxDirty = !bCaptureMinMax;
			bNormalized = false;

			bValid = false;
			if (!bEnabled) { return false; }

			const UPCGPointData* InData = PointIO->GetIn();

			TArray<FString> ExtraNames;
			const FPCGAttributePropertyInputSelector Selector = CopyAndFixLast(Config.Selector, InData, ExtraNames);
			if (!Selector.IsValid()) { return false; }

			ProcessExtraNames(Selector.GetName(), ExtraNames);

			int32 NumPoints = PointIO->GetNum(PCGExData::ESource::In);
			Selection = Selector.GetSelection();
			if (Selection == EPCGAttributePropertySelection::Attribute)
			{
				Attribute = InData->Metadata->GetMutableAttribute(Selector.GetName());
				if (!Attribute) { return false; }

				PCGMetadataAttribute::CallbackWithRightType(
					Attribute->GetTypeId(),
					[&](auto DummyValue) -> void
					{
						using RawT = decltype(DummyValue);
						TArray<RawT> RawValues;

						PCGEX_SET_NUM_UNINITIALIZED(RawValues, NumPoints)
						PCGEX_SET_NUM_UNINITIALIZED(Dump, NumPoints)

						FPCGMetadataAttribute<RawT>* TypedAttribute = InData->Metadata->GetMutableTypedAttribute<RawT>(Selector.GetName());
						FPCGAttributeAccessor<RawT>* Accessor = new FPCGAttributeAccessor<RawT>(TypedAttribute, InData->Metadata);
						IPCGAttributeAccessorKeys* Keys = const_cast<PCGExData::FPointIO*>(PointIO)->CreateInKeys();
						TArrayView<RawT> View(RawValues);
						Accessor->GetRange(View, 0, *Keys, PCGEX_AAFLAG);

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
						delete Accessor;
					});

				bValid = true;
			}
			else if (Selection == EPCGAttributePropertySelection::PointProperty)
			{
				const TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InData, Selector);
				const TArray<FPCGPoint>& InPoints = InData->GetPoints();
				PCGEX_SET_NUM_UNINITIALIZED(Dump, NumPoints)
#define PCGEX_GET_BY_ACCESSOR(_ENUM, _ACCESSOR) case _ENUM:\
				if (bCaptureMinMax) { for (int i = 0; i < NumPoints; i++) {\
						T V = Convert(InPoints[i]._ACCESSOR); OutMin = PCGExMath::Min(V, OutMin); OutMax = PCGExMath::Max(V, OutMax); Dump[i] = V;\
					} } else { for (int i = 0; i < NumPoints; i++) { Dump[i] = Convert(InPoints[i]._ACCESSOR); } } break;

				switch (Config.Selector.GetPointProperty()) { PCGEX_FOREACH_POINTPROPERTY(PCGEX_GET_BY_ACCESSOR) }
#undef PCGEX_GET_BY_ACCESSOR
				bValid = true;
			}
			else
			{
				//TODO: Support extra properties
			}

			return bValid;
		}

		/**
		 * Build and validate a property/attribute accessor for the selected
		 * @param PointIO
		 * @param bCaptureMinMax 
		 */
		bool Grab(const PCGExData::FPointIO* PointIO, const bool bCaptureMinMax = false)
		{
			Cleanup();
			return GrabAndDump(PointIO, Values, bCaptureMinMax, Min, Max);
		}

		bool SoftGrab(const PCGExData::FPointIO* PointIO)
		{
			Cleanup();

			bNormalized = false;
			bValid = false;
			if (!bEnabled) { return false; }

			const UPCGPointData* InData = PointIO->GetIn();

			TArray<FString> ExtraNames;
			const FPCGAttributePropertyInputSelector Selector = CopyAndFixLast(Config.Selector, InData, ExtraNames);
			if (!Selector.IsValid()) { return false; }

			ProcessExtraNames(Config.Selector.GetName(), ExtraNames);

			Selection = Selector.GetSelection();
			if (Selection == EPCGAttributePropertySelection::Attribute)
			{
				Attribute = InData->Metadata->GetMutableAttribute(Selector.GetName());
				bValid = Attribute ? true : false;
			}
			else if (Selection == EPCGAttributePropertySelection::PointProperty)
			{
				PointProperty = Selector.GetPointProperty();
				bValid = true;
			}
			else { bValid = false; }

			return bValid;
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

		FORCEINLINE T SoftGet(const FPCGPoint& Point, const T& fallback)
		{
			// Note: This function is SUPER SLOW and should only be used for cherry picking

			if (!bValid) { return fallback; }

			if (Selection == EPCGAttributePropertySelection::Attribute)
			{
				return PCGMetadataAttribute::CallbackWithRightType(
					Attribute->GetTypeId(),
					[&](auto DummyValue) -> T
					{
						using RawT = decltype(DummyValue);
						FPCGMetadataAttribute<RawT>* TypedAttribute = static_cast<FPCGMetadataAttribute<RawT>*>(Attribute);
						return Convert(TypedAttribute->GetValueFromItemKey(Point.MetadataEntry));
					});
			}

			if (Selection == EPCGAttributePropertySelection::PointProperty)
			{
#define PCGEX_GET_BY_ACCESSOR(_ENUM, _ACCESSOR) case _ENUM: return Convert(Point._ACCESSOR); break;
				switch (PointProperty) { PCGEX_FOREACH_POINTPROPERTY(PCGEX_GET_BY_ACCESSOR) }
#undef PCGEX_GET_BY_ACCESSOR
			}

			return fallback;
		}

		FORCEINLINE T SafeGet(const int32 Index, const T& fallback) const { return (!bValid || !bEnabled) ? fallback : Values[Index]; }
		FORCEINLINE T operator[](int32 Index) const { return bValid ? Values[Index] : GetDefaultValue(); }

		virtual void Capture(const FPCGExInputConfig& InConfig) { Config = InConfig; }
		virtual void Capture(const FPCGAttributePropertyInputSelector& InConfig) { Capture(FPCGExInputConfig(InConfig)); }

	protected:
		virtual void ProcessExtraNames(const FName BaseName, const TArray<FString>& ExtraNames)
		{
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

			FullName = ExtraNames.IsEmpty() ? BaseName : FName(BaseName.ToString() + FString::Join(ExtraNames, TEXT(".")));
		}

		FORCEINLINE virtual T GetDefaultValue() const = 0;
		virtual void ResetMinMax() = 0;

#define  PCGEX_PRINT_VIRTUAL(_TYPE, _NAME, ...) FORCEINLINE virtual T Convert(const _TYPE Value) const { return GetDefaultValue(); };
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_PRINT_VIRTUAL)
	};

#pragma endregion

#pragma region attribute copy

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

	static void CopyValues(
		PCGExMT::FTaskManager* AsyncManager,
		FAttributeIdentity Identity,
		const PCGExData::FPointIO* Source,
		PCGExData::FPointIO* Target,
		const TArrayView<const int32>& SourceIndices,
		const int32 TargetIndex = 0)
	{
		PCGMetadataAttribute::CallbackWithRightType(
			static_cast<uint16>(Identity.UnderlyingType),
			[&](auto DummyValue) -> void
			{
				using T = decltype(DummyValue);
				TArray<T> RawValues;

				const FPCGMetadataAttribute<T>* SourceAttribute = Source->GetIn()->Metadata->GetConstTypedAttribute<T>(Identity.Name);
				TFAttributeWriter<T>* Writer = new TFAttributeWriter<T>(
					Identity.Name,
					SourceAttribute->GetValueFromItemKey(PCGInvalidEntryKey),
					SourceAttribute->AllowsInterpolation());

				Writer->BindAndGet(Target);

				const TArray<FPCGPoint>& SourcePoints = Source->GetIn()->GetPoints();
				const int32 NumIndices = SourceIndices.Num();
				for (int i = 0; i < NumIndices; i++)
				{
					Writer->Values[TargetIndex + i] = SourceAttribute->GetValueFromItemKey(SourcePoints[SourceIndices[i]].MetadataEntry);
				}

				PCGEX_ASYNC_WRITE_DELETE(AsyncManager, Writer);
				PCGEX_DELETE(Writer)
			});
	}

#pragma endregion

#pragma region Local Attribute Getter

	class PCGEXTENDEDTOOLKIT_API FLocalSingleFieldGetter : public FAttributeGetter<double>
	{
		virtual EPCGMetadataTypes GetType() override { return EPCGMetadataTypes::Double; }

	protected:
		virtual void ResetMinMax() override
		{
			Min = TNumericLimits<double>::Max();
			Max = TNumericLimits<double>::Min();
		}

		FORCEINLINE virtual double GetDefaultValue() const override { return 0; }

		FORCEINLINE virtual double Convert(const int32 Value) const override { return Value; }
		FORCEINLINE virtual double Convert(const int64 Value) const override { return static_cast<double>(Value); }
		FORCEINLINE virtual double Convert(const float Value) const override { return Value; }
		FORCEINLINE virtual double Convert(const double Value) const override { return Value; }

		FORCEINLINE virtual double Convert(const FVector2D Value) const override
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
				return Value.Length();
			}
		}

		FORCEINLINE virtual double Convert(const FVector Value) const override
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
				return Value.Length();
			}
		}

		FORCEINLINE virtual double Convert(const FVector4 Value) const override
		{
			switch (Field)
			{
			default:
			case EPCGExSingleField::X:
				return Value.X;
			case EPCGExSingleField::Y:
				return Value.Y;
			case EPCGExSingleField::Z:
				return Value.Z;
			case EPCGExSingleField::W:
				return Value.W;
			case EPCGExSingleField::Length:
				return FVector(Value).Length();
			}
		}

		FORCEINLINE virtual double Convert(const FQuat Value) const override
		{
			if (bUseAxis) { return Convert(PCGExMath::GetDirection(Value, Axis)); }
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
				return GetDefaultValue();
			}
		}

		FORCEINLINE virtual double Convert(const FTransform Value) const override
		{
			switch (Component)
			{
			default: ;
			case EPCGExTransformComponent::Position:
				return Convert(Value.GetLocation());
			case EPCGExTransformComponent::Rotation:
				return Convert(Value.GetRotation());
			case EPCGExTransformComponent::Scale:
				return Convert(Value.GetScale3D());
			}
		}

		FORCEINLINE virtual double Convert(const bool Value) const override { return Value; }
		FORCEINLINE virtual double Convert(const FRotator Value) const override { return Convert(FVector(Value.Pitch, Value.Yaw, Value.Roll)); }
		FORCEINLINE virtual double Convert(const FString Value) const override { return PCGExMath::ConvertStringToDouble(Value); }
		FORCEINLINE virtual double Convert(const FName Value) const override { return PCGExMath::ConvertStringToDouble(Value.ToString()); }
	};

	class PCGEXTENDEDTOOLKIT_API FLocalIntegerGetter final : public FAttributeGetter<int32>
	{
		virtual EPCGMetadataTypes GetType() override { return EPCGMetadataTypes::Integer32; }

	protected:
		virtual void ResetMinMax() override
		{
			Min = TNumericLimits<double>::Max();
			Max = TNumericLimits<double>::Min();
		}

		FORCEINLINE virtual int32 GetDefaultValue() const override { return 0; }

		FORCEINLINE virtual int32 Convert(const int32 Value) const override { return Value; }
		FORCEINLINE virtual int32 Convert(const int64 Value) const override { return static_cast<int32>(Value); }
		FORCEINLINE virtual int32 Convert(const float Value) const override { return Value; }
		FORCEINLINE virtual int32 Convert(const double Value) const override { return Value; }

		FORCEINLINE virtual int32 Convert(const FVector2D Value) const override
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
				return Value.Length();
			}
		}

		FORCEINLINE virtual int32 Convert(const FVector Value) const override
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
				return Value.Length();
			}
		}

		FORCEINLINE virtual int32 Convert(const FVector4 Value) const override
		{
			switch (Field)
			{
			default:
			case EPCGExSingleField::X:
				return Value.X;
			case EPCGExSingleField::Y:
				return Value.Y;
			case EPCGExSingleField::Z:
				return Value.Z;
			case EPCGExSingleField::W:
				return Value.W;
			case EPCGExSingleField::Length:
				return FVector(Value).Length();
			}
		}

		FORCEINLINE virtual int32 Convert(const FQuat Value) const override
		{
			if (bUseAxis) { return Convert(PCGExMath::GetDirection(Value, Axis)); }
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
				return GetDefaultValue();
			}
		}

		FORCEINLINE virtual int32 Convert(const FTransform Value) const override
		{
			switch (Component)
			{
			default: ;
			case EPCGExTransformComponent::Position:
				return Convert(Value.GetLocation());
			case EPCGExTransformComponent::Rotation:
				return Convert(Value.GetRotation());
			case EPCGExTransformComponent::Scale:
				return Convert(Value.GetScale3D());
			}
		}

		FORCEINLINE virtual int32 Convert(const bool Value) const override { return Value; }
		FORCEINLINE virtual int32 Convert(const FRotator Value) const override { return Convert(FVector(Value.Pitch, Value.Yaw, Value.Roll)); }
		FORCEINLINE virtual int32 Convert(const FString Value) const override { return PCGExMath::ConvertStringToDouble(Value); }
		FORCEINLINE virtual int32 Convert(const FName Value) const override { return PCGExMath::ConvertStringToDouble(Value.ToString()); }
	};

	class PCGEXTENDEDTOOLKIT_API FLocalBoolGetter final : public FAttributeGetter<bool>
	{
		virtual EPCGMetadataTypes GetType() override { return EPCGMetadataTypes::Boolean; }

	protected:
		virtual void ResetMinMax() override
		{
			Min = false;
			Max = true;
		}

		FORCEINLINE virtual bool GetDefaultValue() const override { return false; }

		FORCEINLINE virtual bool Convert(const int32 Value) const override { return Value > 0; }
		FORCEINLINE virtual bool Convert(const int64 Value) const override { return Value > 0; }
		FORCEINLINE virtual bool Convert(const float Value) const override { return Value > 0; }
		FORCEINLINE virtual bool Convert(const double Value) const override { return Value > 0; }

		FORCEINLINE virtual bool Convert(const FVector2D Value) const override
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
				return Value.Length() > 0;
			}
		}

		FORCEINLINE virtual bool Convert(const FVector Value) const override
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
				return Value.Length() > 0;
			}
		}

		FORCEINLINE virtual bool Convert(const FVector4 Value) const override
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
				return FVector(Value).Length() > 0;
			}
		}

		FORCEINLINE virtual bool Convert(const FQuat Value) const override
		{
			if (bUseAxis) { return Convert(PCGExMath::GetDirection(Value, Axis)); }
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
				return GetDefaultValue();
			}
		}

		FORCEINLINE virtual bool Convert(const FTransform Value) const override
		{
			switch (Component)
			{
			default: ;
			case EPCGExTransformComponent::Position:
				return Convert(Value.GetLocation());
			case EPCGExTransformComponent::Rotation:
				return Convert(Value.GetRotation());
			case EPCGExTransformComponent::Scale:
				return Convert(Value.GetScale3D());
			}
		}

		FORCEINLINE virtual bool Convert(const bool Value) const override { return Value; }
		FORCEINLINE virtual bool Convert(const FRotator Value) const override { return Convert(FVector(Value.Pitch, Value.Yaw, Value.Roll)); }
		FORCEINLINE virtual bool Convert(const FString Value) const override { return Value.Len() == 4; }
		FORCEINLINE virtual bool Convert(const FName Value) const override { return Value.ToString().Len() == 4; }
	};

	class PCGEXTENDEDTOOLKIT_API FLocalVectorGetter final : public FAttributeGetter<FVector>
	{
		virtual EPCGMetadataTypes GetType() override { return EPCGMetadataTypes::Vector; }

	protected:
		virtual void ResetMinMax() override
		{
			Min = FVector(TNumericLimits<double>::Max());
			Max = FVector(TNumericLimits<double>::Min());
		}

		FORCEINLINE virtual FVector GetDefaultValue() const override { return FVector::ZeroVector; }

		FORCEINLINE virtual FVector Convert(const bool Value) const override { return FVector(Value); }
		FORCEINLINE virtual FVector Convert(const int32 Value) const override { return FVector(Value); }
		FORCEINLINE virtual FVector Convert(const int64 Value) const override { return FVector(Value); }
		FORCEINLINE virtual FVector Convert(const float Value) const override { return FVector(Value); }
		FORCEINLINE virtual FVector Convert(const double Value) const override { return FVector(Value); }
		FORCEINLINE virtual FVector Convert(const FVector2D Value) const override { return FVector(Value.X, Value.Y, 0); }
		FORCEINLINE virtual FVector Convert(const FVector Value) const override { return Value; }
		FORCEINLINE virtual FVector Convert(const FVector4 Value) const override { return FVector(Value); }
		FORCEINLINE virtual FVector Convert(const FQuat Value) const override { return PCGExMath::GetDirection(Value, Axis); }

		FORCEINLINE virtual FVector Convert(const FTransform Value) const override
		{
			switch (Component)
			{
			default: ;
			case EPCGExTransformComponent::Position:
				return Convert(Value.GetLocation());
			case EPCGExTransformComponent::Rotation:
				return Convert(Value.GetRotation());
			case EPCGExTransformComponent::Scale:
				return Convert(Value.GetScale3D());
			}
		}

		FORCEINLINE virtual FVector Convert(const FRotator Value) const override { return Value.Vector(); }
	};

	class PCGEXTENDEDTOOLKIT_API FLocalToStringGetter final : public FAttributeGetter<FString>
	{
		virtual EPCGMetadataTypes GetType() override { return EPCGMetadataTypes::String; }

	protected:
		virtual void ResetMinMax() override
		{
			Min = TEXT("");
			Max = TEXT("");
		}

		FORCEINLINE virtual FString GetDefaultValue() const override { return ""; }

		FORCEINLINE virtual FString Convert(const bool Value) const override { return FString::Printf(TEXT("%s"), Value ? TEXT("true") : TEXT("false")); }
		FORCEINLINE virtual FString Convert(const int32 Value) const override { return FString::Printf(TEXT("%d"), Value); }
		FORCEINLINE virtual FString Convert(const int64 Value) const override { return FString::Printf(TEXT("%lld"), Value); }
		FORCEINLINE virtual FString Convert(const float Value) const override { return FString::Printf(TEXT("%f"), Value); }
		FORCEINLINE virtual FString Convert(const double Value) const override { return FString::Printf(TEXT("%lf"), Value); }
		FORCEINLINE virtual FString Convert(const FVector2D Value) const override { return *Value.ToString(); }
		FORCEINLINE virtual FString Convert(const FVector Value) const override { return *Value.ToString(); }
		FORCEINLINE virtual FString Convert(const FVector4 Value) const override { return *Value.ToString(); }
		FORCEINLINE virtual FString Convert(const FQuat Value) const override { return *Value.ToString(); }
		FORCEINLINE virtual FString Convert(const FTransform Value) const override { return *Value.ToString(); }
		FORCEINLINE virtual FString Convert(const FRotator Value) const override { return *Value.ToString(); }
		FORCEINLINE virtual FString Convert(const FString Value) const override { return *Value; }
		FORCEINLINE virtual FString Convert(const FName Value) const override { return *Value.ToString(); }

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
		FORCEINLINE virtual FString Convert(const FSoftClassPath Value) const override { return Value.ToString(); }
		FORCEINLINE virtual FString Convert(const FSoftObjectPath Value) const override { return Value.ToString(); }
#endif
	};

#pragma endregion

	static FAttributesInfos* GatherAttributeInfos(const FPCGContext* InContext, const FName InPinLabel, const FPCGExAttributeGatherDetails& InGatherDetails, bool bThrowError)
	{
		FAttributesInfos* OutInfos = new FAttributesInfos();
		TArray<FPCGTaggedData> TaggedDatas = InContext->InputData.GetInputsByPin(InPinLabel);

		bool bHasErrors = false;

		for (FPCGTaggedData& TaggedData : TaggedDatas)
		{
			const UPCGMetadata* Metadata = nullptr;

			if (const UPCGParamData* ParamData = Cast<UPCGParamData>(TaggedData.Data)) { Metadata = ParamData->Metadata; }
			else if (const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(TaggedData.Data)) { Metadata = SpatialData->Metadata; }

			if (!Metadata) { continue; }

			TSet<FName> Mismatch;
			const FAttributesInfos* Infos = FAttributesInfos::Get(Metadata);

			OutInfos->Append(Infos, InGatherDetails, Mismatch);

			if (bThrowError && !Mismatch.IsEmpty())
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("Some inputs share the same name but not the same type."));
				bHasErrors = true;
				break;
			}

			PCGEX_DELETE(Infos)
		}

		if (bHasErrors) { PCGEX_DELETE(OutInfos) }
		return OutInfos;
	}
}

#undef PCGEX_AAFLAG
