// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "PCGEx.h"
#include "PCGExMath.h"
#include "PCGExPointIO.h"
#include "Metadata/Accessors/PCGAttributeAccessor.h"

#include "PCGExAttributeHelpers.generated.h"

#pragma region Input Descriptors

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputDescriptor
{
	GENERATED_BODY()

	FPCGExInputDescriptor()
	{
	}

	explicit FPCGExInputDescriptor(const FPCGAttributePropertyInputSelector& InSelector)
	{
		Selector.ImportFromOtherSelector(InSelector);
	}

	explicit FPCGExInputDescriptor(const FPCGExInputDescriptor& Other)
		: Attribute(Other.Attribute)
	{
		Selector.ImportFromOtherSelector(Other.Selector);
	}

	explicit FPCGExInputDescriptor(const FName InName)
	{
		Selector.Update(InName.ToString());
	}

public:
	virtual ~FPCGExInputDescriptor()
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
		FName Name = NAME_None;
		EPCGMetadataTypes UnderlyingType = EPCGMetadataTypes::Unknown;

		FAttributeIdentity(const FName InName, const EPCGMetadataTypes InUnderlyingType)
			: Name(InName), UnderlyingType(InUnderlyingType)
		{
		}

		FString GetDisplayName() const { return FString(Name.ToString() + FString::Printf(TEXT("( %d )"), UnderlyingType)); }
		bool operator==(const FAttributeIdentity& Other) const { return Name == Other.Name; }

		static void Get(const UPCGMetadata* InMetadata, TArray<FAttributeIdentity>& OutIdentities);
		static void Get(const UPCGMetadata* InMetadata, TArray<FName>& OutNames, TMap<FName, FAttributeIdentity>& OutIdentities);
	};

	struct FAttributesInfos
	{
		TArray<FAttributeIdentity> Identities;
		TArray<FPCGMetadataAttributeBase*> Attributes;
		bool Contains(FName AttributeName, EPCGMetadataTypes Type);
		bool Contains(FName AttributeName);
		FAttributeIdentity* Find(FName AttributeName);

		~FAttributesInfos()
		{
			Identities.Empty();
			Attributes.Empty();
		}

		static FAttributesInfos* Get(const UPCGMetadata* InMetadata);
	};

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
			Attribute = static_cast<FPCGMetadataAttribute<T>*>(InAttribute);
			Accessor = MakeUnique<FPCGAttributeAccessor<T>>(Attribute, InData->Metadata);
			NumEntries = InKeys->GetNum();
			Keys = InKeys;
		}

		T GetDefaultValue() const { return Attribute->GetValue(PCGInvalidEntryKey); }
		bool GetAllowsInterpolation() const { return Attribute->AllowsInterpolation(); }

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
			OutValues.SetNumUninitialized(Count == -1 ? NumEntries - Index : Count, true);
			TArrayView<T> View(OutValues);
			return Accessor->GetRange(View, Index, InKeys ? *InKeys : *Keys, PCGEX_AAFLAG);
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
	class PCGEXTENDEDTOOLKIT_API FAttributeAccessor : public FAttributeAccessorBase<T>
	{
	public:
		FAttributeAccessor(const UPCGPointData* InData, FPCGMetadataAttributeBase* InAttribute, FPCGAttributeAccessorKeysPoints* InKeys)
			: FAttributeAccessorBase<T>(InData, InAttribute, InKeys)
		{
		}

		FAttributeAccessor(UPCGPointData* InData, FPCGMetadataAttributeBase* InAttribute)
			: FAttributeAccessorBase<T>()
		{
			this->Flush();
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
			PCGExData::FPointIO& InPointIO, FName AttributeName,
			const T& DefaultValue = T{}, bool bAllowsInterpolation = true, bool bOverrideParent = true, bool bOverwriteIfTypeMismatch = true)
		{
			UPCGPointData* InData = InPointIO.GetOut();
			FPCGMetadataAttribute<T>* InAttribute = InData->Metadata->FindOrCreateAttribute(
				AttributeName, DefaultValue,
				bAllowsInterpolation, bOverrideParent, bOverwriteIfTypeMismatch);

			return new FAttributeAccessor<T>(InData, InAttribute, InPointIO.CreateOutKeys());
		}
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FConstAttributeAccessor : public FAttributeAccessorBase<T>
	{
	public:
		FConstAttributeAccessor(const UPCGPointData* InData, FPCGMetadataAttributeBase* InAttribute, FPCGAttributeAccessorKeysPoints* InKeys)
			: FAttributeAccessorBase<T>(InData, InAttribute, InKeys)
		{
		}

		FConstAttributeAccessor(const UPCGPointData* InData, FPCGMetadataAttributeBase* InAttribute): FAttributeAccessorBase<T>()
		{
			this->Flush();
			this->Attribute = static_cast<FPCGMetadataAttribute<T>*>(InAttribute);
			this->Accessor = MakeUnique<FPCGAttributeAccessor<T>>(this->Attribute, InData->Metadata);

			this->InternalKeys = new FPCGAttributeAccessorKeysPoints(InData->GetPoints());

			this->NumEntries = this->InternalKeys->GetNum();
			this->Keys = this->InternalKeys;
		}

		static FConstAttributeAccessor* Find(const PCGExData::FPointIO& InPointIO, const FName AttributeName)
		{
			const UPCGPointData* InData = InPointIO.GetIn();
			if (FPCGMetadataAttributeBase* InAttribute = InData->Metadata->GetMutableAttribute(AttributeName))
			{
				return new FConstAttributeAccessor(InData, InAttribute, InPointIO.GetInKeys());
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

		T GetDefaultValue() const { return Accessor->GetDefaultValue(); }
		bool GetAllowsInterpolation() const { return Accessor->GetAllowsInterpolation(); }

		void SetNum(int32 Num) { Values.SetNumZeroed(Num); }
		virtual bool Bind(PCGExData::FPointIO& PointIO) = 0;

		T operator[](int32 Index) const { return this->Values[Index]; }

		bool IsValid() { return Accessor != nullptr; }

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

		virtual bool Bind(PCGExData::FPointIO& PointIO) override
		{
			PCGEX_DELETE(this->Accessor)
			this->Accessor = FAttributeAccessor<T>::FindOrCreate(
				PointIO, this->Name, DefaultValue,
				bAllowsInterpolation, bOverrideParent, bOverwriteIfTypeMismatch);
			this->UnderlyingType = PointIO.GetOut()->Metadata->GetConstAttribute(this->Name)->GetTypeId();
			return true;
		}

		bool BindAndGet(PCGExData::FPointIO& PointIO)
		{
			if (Bind(PointIO))
			{
				this->SetNum(PointIO.GetOutNum());
				this->Accessor->GetRange(this->Values);
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
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API TFAttributeReader final : public FAttributeIOBase<T>
	{
	public:
		explicit TFAttributeReader(const FName& InName)
			: FAttributeIOBase<T>(InName)
		{
		}

		virtual bool Bind(PCGExData::FPointIO& PointIO) override
		{
			PCGEX_DELETE(this->Accessor)
			this->Accessor = FConstAttributeAccessor<T>::Find(PointIO, this->Name);
			if (!this->Accessor) { return false; }
			this->SetNum(PointIO.GetNum());
			this->Accessor->GetRange(this->Values);
			this->UnderlyingType = PointIO.GetIn()->Metadata->GetConstAttribute(this->Name)->GetTypeId();
			return true;
		}
	};

#pragma endregion

#pragma region Local Attribute Inputs

	template <typename T>
	struct PCGEXTENDEDTOOLKIT_API FAttributeGetter
	{
	protected:
		bool bMinMaxDirty = true;
		bool bNormalized = false;

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
			FAttributeGetter<T>::Cleanup();
		}

		EPCGExTransformComponent Component = EPCGExTransformComponent::Position;
		bool bUseAxis = false;
		EPCGExAxis Axis = EPCGExAxis::Forward;
		EPCGExSingleField Field = EPCGExSingleField::X;

		TArray<T> Values;
		mutable T Min = T{};
		mutable T Max = T{};

		bool bEnabled = true;
		bool bValid = false;

		EPCGPointProperties PointProperty = EPCGPointProperties::Position;
		EPCGAttributePropertySelection Selection = EPCGAttributePropertySelection::Attribute;
		FPCGMetadataAttributeBase* Attribute = nullptr;

		bool IsUsable(int32 NumEntries) { return bEnabled && bValid && Values.Num() >= NumEntries; }

		FPCGExInputDescriptor Descriptor;

		virtual void Cleanup()
		{
			Values.Empty();
		}

		/**
		 * Build and validate a property/attribute accessor for the selected
		 * @param PointIO
		 * @param bCaptureMinMax 
		 */
		bool Grab(const PCGExData::FPointIO& PointIO, const bool bCaptureMinMax = false)
		{
			Cleanup();

			ResetMinMax();
			bMinMaxDirty = !bCaptureMinMax;
			bNormalized = false;

			bValid = false;
			if (!bEnabled) { return false; }

			const UPCGPointData* InData = PointIO.GetIn();

			TArray<FString> ExtraNames;
			const FPCGAttributePropertyInputSelector Selector = CopyAndFixLast(Descriptor.Selector, InData, ExtraNames);
			if (!Selector.IsValid()) { return false; }

			ProcessExtraNames(ExtraNames);

			int32 NumPoints = PointIO.GetNum();
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

						RawValues.SetNumUninitialized(NumPoints);
						Values.SetNumUninitialized(NumPoints);

						FPCGMetadataAttribute<RawT>* TypedAttribute = InData->Metadata->GetMutableTypedAttribute<RawT>(Selector.GetName());
						FPCGAttributeAccessor<RawT>* Accessor = new FPCGAttributeAccessor<RawT>(TypedAttribute, InData->Metadata);
						IPCGAttributeAccessorKeys* Keys = const_cast<PCGExData::FPointIO&>(PointIO).CreateInKeys();
						TArrayView<RawT> View(RawValues);
						Accessor->GetRange(View, 0, *Keys, PCGEX_AAFLAG);

						if (bCaptureMinMax)
						{
							for (int i = 0; i < NumPoints; i++)
							{
								T V = Convert(RawValues[i]);
								Min = PCGExMath::Min(V, Min);
								Max = PCGExMath::Max(V, Max);
								Values[i] = V;
							}
						}
						else
						{
							for (int i = 0; i < NumPoints; i++) { Values[i] = Convert(RawValues[i]); }
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
				Values.SetNumUninitialized(NumPoints);
#define PCGEX_GET_BY_ACCESSOR(_ENUM, _ACCESSOR) case _ENUM:\
				if (bCaptureMinMax) { for (int i = 0; i < NumPoints; i++) {\
						T V = Convert(InPoints[i]._ACCESSOR); Min = PCGExMath::Min(V, Min); Max = PCGExMath::Max(V, Max); Values[i] = V;\
					} } else { for (int i = 0; i < NumPoints; i++) { Values[i] = Convert(InPoints[i]._ACCESSOR); } } break;

				switch (Descriptor.Selector.GetPointProperty()) { PCGEX_FOREACH_POINTPROPERTY(PCGEX_GET_BY_ACCESSOR) }
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
		 */
		bool SoftGrab(const PCGExData::FPointIO& PointIO)
		{
			Cleanup();

			bNormalized = false;
			bValid = false;
			if (!bEnabled) { return false; }

			const UPCGPointData* InData = PointIO.GetIn();

			TArray<FString> ExtraNames;
			const FPCGAttributePropertyInputSelector Selector = CopyAndFixLast(Descriptor.Selector, InData, ExtraNames);
			if (!Selector.IsValid()) { return false; }

			ProcessExtraNames(ExtraNames);

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

		T SoftGet(const FPCGPoint& Point, const T& fallback)
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

		T SafeGet(const int32 Index, const T& fallback) const { return (!bValid || !bEnabled) ? fallback : Values[Index]; }
		T operator[](int32 Index) const { return bValid ? Values[Index] : GetDefaultValue(); }

		virtual void Capture(const FPCGExInputDescriptor& InDescriptor) { Descriptor = InDescriptor; }
		virtual void Capture(const FPCGAttributePropertyInputSelector& InDescriptor) { Capture(FPCGExInputDescriptor(InDescriptor)); }

	protected:
		virtual void ProcessExtraNames(const TArray<FString>& ExtraNames)
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
		}

		virtual T GetDefaultValue() const = 0;
		virtual void ResetMinMax() = 0;

#define  PCGEX_PRINT_VIRTUAL(_TYPE, _NAME, ...) virtual T Convert(const _TYPE Value) const { return GetDefaultValue(); };
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_PRINT_VIRTUAL)
	};

#pragma endregion

#pragma region attribute copy

	static void CopyPoints(
		const PCGExData::FPointIO& Source,
		const PCGExData::FPointIO& Target,
		const TArrayView<int32>& SourceIndices,
		const int32 TargetIndex = 0,
		const bool bKeepSourceMetadataEntry = false)
	{
		const int32 NumIndices = SourceIndices.Num();
		const TArray<FPCGPoint>& SourcePoints = Source.GetIn()->GetPoints();
		TArray<FPCGPoint>& TargetPoints = Target.GetOut()->GetMutablePoints();

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
		FAttributeIdentity Identity,
		const PCGExData::FPointIO& Source,
		PCGExData::FPointIO& Target,
		const TArrayView<int32>& SourceIndices,
		const int32 TargetIndex = 0)
	{
		PCGMetadataAttribute::CallbackWithRightType(
			static_cast<uint16>(Identity.UnderlyingType),
			[&](auto DummyValue) -> void
			{
				using T = decltype(DummyValue);
				TArray<T> RawValues;

				const FPCGMetadataAttribute<T>* SourceAttribute = Source.GetIn()->Metadata->GetConstTypedAttribute<T>(Identity.Name);
				TFAttributeWriter<T>* Writer = new TFAttributeWriter<T>(
					Identity.Name,
					SourceAttribute->GetValueFromItemKey(PCGInvalidEntryKey),
					SourceAttribute->AllowsInterpolation());

				Writer->BindAndGet(Target);

				const TArray<FPCGPoint>& SourcePoints = Source.GetIn()->GetPoints();
				const int32 NumIndices = SourceIndices.Num();
				for (int i = 0; i < NumIndices; i++)
				{
					Writer->Values[TargetIndex + i] = SourceAttribute->GetValueFromItemKey(SourcePoints[SourceIndices[i]].MetadataEntry);
				}

				Writer->Write();
				PCGEX_DELETE(Writer)
			});
	}

#pragma endregion

#pragma region Local Attribute Getter

	struct PCGEXTENDEDTOOLKIT_API FLocalSingleFieldGetter : public FAttributeGetter<double>
	{
	protected:
		virtual void ResetMinMax() override
		{
			Min = TNumericLimits<double>::Max();
			Max = TNumericLimits<double>::Min();
		}

		virtual double GetDefaultValue() const override { return 0; }

		virtual double Convert(const int32 Value) const override { return Value; }
		virtual double Convert(const int64 Value) const override { return static_cast<double>(Value); }
		virtual double Convert(const float Value) const override { return Value; }
		virtual double Convert(const double Value) const override { return Value; }

		virtual double Convert(const FVector2D Value) const override
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

		virtual double Convert(const FVector Value) const override
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

		virtual double Convert(const FVector4 Value) const override
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

		virtual double Convert(const FQuat Value) const override
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

		virtual double Convert(const FTransform Value) const override
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

		virtual double Convert(const bool Value) const override { return Value; }
		virtual double Convert(const FRotator Value) const override { return Convert(FVector(Value.Pitch, Value.Yaw, Value.Roll)); }
		virtual double Convert(const FString Value) const override { return PCGExMath::ConvertStringToDouble(Value); }
		virtual double Convert(const FName Value) const override { return PCGExMath::ConvertStringToDouble(Value.ToString()); }
	};

	struct PCGEXTENDEDTOOLKIT_API FLocalIntegerGetter : public FAttributeGetter<int32>
	{
	protected:
		virtual void ResetMinMax() override
		{
			Min = TNumericLimits<double>::Max();
			Max = TNumericLimits<double>::Min();
		}

		virtual int32 GetDefaultValue() const override { return 0; }

		virtual int32 Convert(const int32 Value) const override { return Value; }
		virtual int32 Convert(const int64 Value) const override { return static_cast<int32>(Value); }
		virtual int32 Convert(const float Value) const override { return Value; }
		virtual int32 Convert(const double Value) const override { return Value; }

		virtual int32 Convert(const FVector2D Value) const override
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

		virtual int32 Convert(const FVector Value) const override
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

		virtual int32 Convert(const FVector4 Value) const override
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

		virtual int32 Convert(const FQuat Value) const override
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

		virtual int32 Convert(const FTransform Value) const override
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

		virtual int32 Convert(const bool Value) const override { return Value; }
		virtual int32 Convert(const FRotator Value) const override { return Convert(FVector(Value.Pitch, Value.Yaw, Value.Roll)); }
		virtual int32 Convert(const FString Value) const override { return PCGExMath::ConvertStringToDouble(Value); }
		virtual int32 Convert(const FName Value) const override { return PCGExMath::ConvertStringToDouble(Value.ToString()); }
	};

	struct PCGEXTENDEDTOOLKIT_API FLocalBoolGetter : public FAttributeGetter<bool>
	{
	protected:
		virtual void ResetMinMax() override
		{
			Min = false;
			Max = true;
		}

		virtual bool GetDefaultValue() const override { return false; }

		virtual bool Convert(const int32 Value) const override { return Value > 0; }
		virtual bool Convert(const int64 Value) const override { return Value > 0; }
		virtual bool Convert(const float Value) const override { return Value > 0; }
		virtual bool Convert(const double Value) const override { return Value > 0; }

		virtual bool Convert(const FVector2D Value) const override
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

		virtual bool Convert(const FVector Value) const override
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

		virtual bool Convert(const FVector4 Value) const override
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

		virtual bool Convert(const FQuat Value) const override
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

		virtual bool Convert(const FTransform Value) const override
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

		virtual bool Convert(const bool Value) const override { return Value; }
		virtual bool Convert(const FRotator Value) const override { return Convert(FVector(Value.Pitch, Value.Yaw, Value.Roll)); }
		virtual bool Convert(const FString Value) const override { return Value.Len() == 4; }
		virtual bool Convert(const FName Value) const override { return Value.ToString().Len() == 4; }
	};

	struct PCGEXTENDEDTOOLKIT_API FLocalVectorGetter : public FAttributeGetter<FVector>
	{
	protected:
		virtual void ResetMinMax() override
		{
			Min = FVector(TNumericLimits<double>::Max());
			Max = FVector(TNumericLimits<double>::Min());
		}

		virtual FVector GetDefaultValue() const override { return FVector::ZeroVector; }

		virtual FVector Convert(const bool Value) const override { return FVector(Value); }
		virtual FVector Convert(const int32 Value) const override { return FVector(Value); }
		virtual FVector Convert(const int64 Value) const override { return FVector(Value); }
		virtual FVector Convert(const float Value) const override { return FVector(Value); }
		virtual FVector Convert(const double Value) const override { return FVector(Value); }
		virtual FVector Convert(const FVector2D Value) const override { return FVector(Value.X, Value.Y, 0); }
		virtual FVector Convert(const FVector Value) const override { return Value; }
		virtual FVector Convert(const FVector4 Value) const override { return FVector(Value); }
		virtual FVector Convert(const FQuat Value) const override { return PCGExMath::GetDirection(Value, Axis); }

		virtual FVector Convert(const FTransform Value) const override
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

		virtual FVector Convert(const FRotator Value) const override { return Value.Vector(); }
	};

	struct PCGEXTENDEDTOOLKIT_API FLocalToStringGetter : public FAttributeGetter<FString>
	{
	protected:
		virtual void ResetMinMax() override
		{
			Min = TEXT("");
			Max = TEXT("");
		}

		virtual FString GetDefaultValue() const override { return ""; }

		virtual FString Convert(const bool Value) const override { return FString::Printf(TEXT("%s"), Value ? TEXT("true") : TEXT("false")); }
		virtual FString Convert(const int32 Value) const override { return FString::Printf(TEXT("%d"), Value); }
		virtual FString Convert(const int64 Value) const override { return FString::Printf(TEXT("%lld"), Value); }
		virtual FString Convert(const float Value) const override { return FString::Printf(TEXT("%f"), Value); }
		virtual FString Convert(const double Value) const override { return FString::Printf(TEXT("%lf"), Value); }
		virtual FString Convert(const FVector2D Value) const override { return *Value.ToString(); }
		virtual FString Convert(const FVector Value) const override { return *Value.ToString(); }
		virtual FString Convert(const FVector4 Value) const override { return *Value.ToString(); }
		virtual FString Convert(const FQuat Value) const override { return *Value.ToString(); }
		virtual FString Convert(const FTransform Value) const override { return *Value.ToString(); }
		virtual FString Convert(const FRotator Value) const override { return *Value.ToString(); }
		virtual FString Convert(const FString Value) const override { return *Value; }
		virtual FString Convert(const FName Value) const override { return *Value.ToString(); }
		
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
		virtual FString Convert(const FSoftClassPath Value) const override { return Value.ToString(); }
		virtual FString Convert(const FSoftObjectPath Value) const override { return Value.ToString(); }
#endif
		
	};

#pragma endregion
}

#undef PCGEX_AAFLAG
