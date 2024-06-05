// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGEx.h"
#include "PCGExMT.h"
#include "PCGExSettings.h"
#include "Data/PCGExAttributeHelpers.h"

#include "PCGExDataBlending.generated.h"

#define PCGEX_FOREACH_BLEND_POINTPROPERTY(MACRO)\
MACRO(float, Density, Float) \
MACRO(FVector, BoundsMin, Vector) \
MACRO(FVector, BoundsMax, Vector) \
MACRO(FVector4, Color, Vector4) \
MACRO(FVector, Position, Vector) \
MACRO(FQuat, Rotation, Quaternion) \
MACRO(FVector, Scale, Vector) \
MACRO(float, Steepness, Float) \
MACRO(int32, Seed, Integer32)

#define PCGEX_FOREACH_BLENDINIT_POINTPROPERTY(MACRO)\
MACRO(float, Density, Float, Density) \
MACRO(FVector, BoundsMin, Vector, BoundsMin) \
MACRO(FVector, BoundsMax, Vector, BoundsMax) \
MACRO(FVector4, Color, Vector4, Color) \
MACRO(FVector, Position, Vector,Transform.GetLocation()) \
MACRO(FQuat, Rotation, Quaternion,Transform.GetRotation()) \
MACRO(FVector, Scale, Vector, Transform.GetScale3D()) \
MACRO(float, Steepness, Float,Steepness) \
MACRO(int32, Seed, Integer32,Seed)

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Blend Over Mode"))
enum class EPCGExBlendOver : uint8
{
	Distance UMETA(DisplayName = "Distance", ToolTip="Blend is based on distance over max distance"),
	Index UMETA(DisplayName = "Count", ToolTip="Blend is based on index over total count"),
	Fixed UMETA(DisplayName = "Fixed", ToolTip="Fixed blend lerp/weight value"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Blending Filter"))
enum class EPCGExBlendingFilter : uint8
{
	All UMETA(DisplayName = "All", ToolTip="All attributes"),
	Exclude UMETA(DisplayName = "Exclude", ToolTip="Exclude listed attributes"),
	Include UMETA(DisplayName = "Include", ToolTip="Only listed attributes"),
};

namespace PCGExGraph
{
	struct FGraphMetadataSettings;
}

struct FPCGExDistanceSettings;
struct FPCGExPointPointIntersectionSettings;

namespace PCGExData
{
	struct FIdxCompoundList;
}

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Data Blending Type"))
enum class EPCGExDataBlendingType : uint8
{
	None        = 0 UMETA(DisplayName = "None", ToolTip="No blending is applied, keep the original value."),
	Average     = 1 UMETA(DisplayName = "Average", ToolTip="Average all sampled values."),
	Weight      = 2 UMETA(DisplayName = "Weight", ToolTip="Weights based on distance to blend targets. If the results are unexpected, try 'Lerp' instead"),
	Min         = 3 UMETA(DisplayName = "Min", ToolTip="Component-wise MIN operation"),
	Max         = 4 UMETA(DisplayName = "Max", ToolTip="Component-wise MAX operation"),
	Copy        = 5 UMETA(DisplayName = "Copy", ToolTip = "Copy incoming data"),
	Sum         = 6 UMETA(DisplayName = "Sum", ToolTip = "Sum of all the data"),
	WeightedSum = 7 UMETA(DisplayName = "Weighted Sum", ToolTip = "Sum of all the data, weighted"),
	Lerp        = 8 UMETA(DisplayName = "Lerp", ToolTip="Uses weight as lerp. If the results are unexpected, try 'Weight' instead."),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExForwardSettings
{
	GENERATED_BODY()

	FPCGExForwardSettings()
	{
	}

	/** Is forwarding enabled. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bEnabled = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bEnabled"))
	EPCGExBlendingFilter FilterAttributes = EPCGExBlendingFilter::Include;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bEnabled && FilterAttributes!=EPCGExBlendingFilter::All", EditConditionHides))
	TArray<FName> FilteredAttributes;

	bool CanProcess(const FName AttributeName) const
	{
		switch (FilterAttributes)
		{
		default: ;
		case EPCGExBlendingFilter::All:
			return true;
		case EPCGExBlendingFilter::Exclude:
			return !FilteredAttributes.Contains(AttributeName);
		case EPCGExBlendingFilter::Include:
			return FilteredAttributes.Contains(AttributeName);
		}
	}

	void Filter(TArray<PCGEx::FAttributeIdentity>& Identities) const
	{
		if (FilterAttributes == EPCGExBlendingFilter::All) { return; }
		for (int i = 0; i < Identities.Num(); i++)
		{
			if (!CanProcess(Identities[i].Name))
			{
				Identities.RemoveAt(i);
				i--;
			}
		}
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExPointPropertyBlendingOverrides
{
	GENERATED_BODY()

#pragma region Property overrides

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bOverrideDensity = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Density", EditCondition="bOverrideDensity"))
	EPCGExDataBlendingType DensityBlending = EPCGExDataBlendingType::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bOverrideBoundsMin = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="BoundsMin", EditCondition="bOverrideBoundsMin"))
	EPCGExDataBlendingType BoundsMinBlending = EPCGExDataBlendingType::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bOverrideBoundsMax = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="BoundsMax", EditCondition="bOverrideBoundsMax"))
	EPCGExDataBlendingType BoundsMaxBlending = EPCGExDataBlendingType::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bOverrideColor = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Color", EditCondition="bOverrideColor"))
	EPCGExDataBlendingType ColorBlending = EPCGExDataBlendingType::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bOverridePosition = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Position", EditCondition="bOverridePosition"))
	EPCGExDataBlendingType PositionBlending = EPCGExDataBlendingType::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bOverrideRotation = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Rotation", EditCondition="bOverrideRotation"))
	EPCGExDataBlendingType RotationBlending = EPCGExDataBlendingType::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bOverrideScale = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Scale", EditCondition="bOverrideScale"))
	EPCGExDataBlendingType ScaleBlending = EPCGExDataBlendingType::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bOverrideSteepness = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Steepness", EditCondition="bOverrideSteepness"))
	EPCGExDataBlendingType SteepnessBlending = EPCGExDataBlendingType::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bOverrideSeed = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Seed", EditCondition="bOverrideSeed"))
	EPCGExDataBlendingType SeedBlending = EPCGExDataBlendingType::Average;

#pragma endregion
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExPropertiesBlendingSettings
{
	GENERATED_BODY()

	FPCGExPropertiesBlendingSettings()
	{
	}

	explicit FPCGExPropertiesBlendingSettings(const EPCGExDataBlendingType InDefaultBlending):
		DefaultBlending(InDefaultBlending)
	{
#define PCGEX_SET_DEFAULT_POINTPROPERTY(_TYPE, _NAME, _TYPENAME) _NAME##Blending = InDefaultBlending;
		PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_SET_DEFAULT_POINTPROPERTY)
#undef PCGEX_SET_DEFAULT_POINTPROPERTY
	}

	bool HasNoBlending() const
	{
		return DensityBlending == EPCGExDataBlendingType::None &&
			BoundsMinBlending == EPCGExDataBlendingType::None &&
			BoundsMaxBlending == EPCGExDataBlendingType::None &&
			ColorBlending == EPCGExDataBlendingType::None &&
			PositionBlending == EPCGExDataBlendingType::None &&
			RotationBlending == EPCGExDataBlendingType::None &&
			ScaleBlending == EPCGExDataBlendingType::None &&
			SteepnessBlending == EPCGExDataBlendingType::None &&
			SeedBlending == EPCGExDataBlendingType::None;
	}

	void GetNonNoneBlendings(TArray<FName>& OutNames) const
	{
#define PCGEX_SET_DEFAULT_POINTPROPERTY(_TYPE, _NAME, _TYPENAME) if(_NAME##Blending != EPCGExDataBlendingType::None){ OutNames.Add(#_NAME);}
		PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_SET_DEFAULT_POINTPROPERTY)
#undef PCGEX_SET_DEFAULT_POINTPROPERTY
	}

	EPCGExDataBlendingType DefaultBlending = EPCGExDataBlendingType::Average;

#pragma region Property overrides

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Density", EditCondition="bOverrideDensity"))
	EPCGExDataBlendingType DensityBlending = EPCGExDataBlendingType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="BoundsMin", EditCondition="bOverrideBoundsMin"))
	EPCGExDataBlendingType BoundsMinBlending = EPCGExDataBlendingType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="BoundsMax", EditCondition="bOverrideBoundsMax"))
	EPCGExDataBlendingType BoundsMaxBlending = EPCGExDataBlendingType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Color", EditCondition="bOverrideColor"))
	EPCGExDataBlendingType ColorBlending = EPCGExDataBlendingType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Position", EditCondition="bOverridePosition"))
	EPCGExDataBlendingType PositionBlending = EPCGExDataBlendingType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Rotation", EditCondition="bOverrideRotation"))
	EPCGExDataBlendingType RotationBlending = EPCGExDataBlendingType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Scale", EditCondition="bOverrideScale"))
	EPCGExDataBlendingType ScaleBlending = EPCGExDataBlendingType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Steepness", EditCondition="bOverrideSteepness"))
	EPCGExDataBlendingType SteepnessBlending = EPCGExDataBlendingType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Seed", EditCondition="bOverrideSeed"))
	EPCGExDataBlendingType SeedBlending = EPCGExDataBlendingType::None;

#pragma endregion
};


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExBlendingSettings
{
	GENERATED_BODY()

	FPCGExBlendingSettings()
	{
	}

	explicit FPCGExBlendingSettings(const EPCGExDataBlendingType InDefaultBlending):
		DefaultBlending(InDefaultBlending)
	{
#define PCGEX_SET_DEFAULT_POINTPROPERTY(_TYPE, _NAME, _TYPENAME) PropertiesOverrides._NAME##Blending = InDefaultBlending;
		PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_SET_DEFAULT_POINTPROPERTY)
#undef PCGEX_SET_DEFAULT_POINTPROPERTY
	}

	explicit FPCGExBlendingSettings(const FPCGExPropertiesBlendingSettings& InPropertiesBlendingSettings):
		DefaultBlending(InPropertiesBlendingSettings.DefaultBlending)
	{
#define PCGEX_SET_DEFAULT_POINTPROPERTY(_TYPE, _NAME, _TYPENAME) PropertiesOverrides.bOverride##_NAME = InPropertiesBlendingSettings._NAME##Blending != EPCGExDataBlendingType::None; PropertiesOverrides._NAME##Blending = InPropertiesBlendingSettings._NAME##Blending;
		PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_SET_DEFAULT_POINTPROPERTY)
#undef PCGEX_SET_DEFAULT_POINTPROPERTY
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExBlendingFilter BlendingFilter = EPCGExBlendingFilter::All;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="BlendingFilter!=EPCGExBlendingFilter::All", EditConditionHides))
	TArray<FName> FilteredAttributes;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExDataBlendingType DefaultBlending = EPCGExDataBlendingType::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Setting)
	FPCGExPointPropertyBlendingOverrides PropertiesOverrides;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TMap<FName, EPCGExDataBlendingType> AttributesOverrides;

	FPCGExPropertiesBlendingSettings GetPropertiesBlendingSettings() const
	{
		FPCGExPropertiesBlendingSettings OutSettings;
#define PCGEX_SET_POINTPROPERTY(_TYPE, _NAME, _TYPENAME) OutSettings._NAME##Blending = PropertiesOverrides.bOverride##_NAME ? PropertiesOverrides._NAME##Blending : DefaultBlending;
		PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_SET_POINTPROPERTY)
#undef PCGEX_SET_POINTPROPERTY
		return OutSettings;
	}

	bool CanBlend(const FName AttributeName) const
	{
		switch (BlendingFilter)
		{
		default: ;
		case EPCGExBlendingFilter::All:
			return true;
		case EPCGExBlendingFilter::Exclude:
			return !FilteredAttributes.Contains(AttributeName);
		case EPCGExBlendingFilter::Include:
			return FilteredAttributes.Contains(AttributeName);
		}
	}

	void Filter(TArray<PCGEx::FAttributeIdentity>& Identities) const
	{
		if (BlendingFilter == EPCGExBlendingFilter::All) { return; }
		for (int i = 0; i < Identities.Num(); i++)
		{
			if (!CanBlend(Identities[i].Name))
			{
				Identities.RemoveAt(i);
				i--;
			}
		}
	}
};

namespace PCGExDataBlending
{
	class PCGEXTENDEDTOOLKIT_API FDataForwardHandler
	{
		const FPCGExForwardSettings* Settings = nullptr;
		const PCGExData::FPointIO* SourceIO = nullptr;
		TArray<PCGEx::FAttributeIdentity> Identities;

	public:
		~FDataForwardHandler();
		explicit FDataForwardHandler(const FPCGExForwardSettings* InSettings, const PCGExData::FPointIO* InSourceIO);
		void Forward(int32 SourceIndex, PCGExData::FPointIO* Target);
	};


	/**
	 * 
	 */
	class PCGEXTENDEDTOOLKIT_API FDataBlendingOperationBase
	{
	public:
		virtual ~FDataBlendingOperationBase();

		virtual EPCGExDataBlendingType GetBlendingType() const = 0;

		void SetAttributeName(const FName InName) { AttributeName = InName; }
		FName GetAttributeName() const { return AttributeName; }

		virtual void PrepareForData(PCGExData::FPointIO& InPrimaryData, const PCGExData::FPointIO& InSecondaryData, const PCGExData::ESource SecondarySource = PCGExData::ESource::In);
		virtual void PrepareForData(PCGEx::FAAttributeIO* InWriter, const PCGExData::FPointIO& InSecondaryData, const PCGExData::ESource SecondarySource = PCGExData::ESource::In);

		FORCEINLINE virtual bool GetIsInterpolation() const;
		FORCEINLINE virtual bool GetRequiresPreparation() const;
		FORCEINLINE virtual bool GetRequiresFinalization() const;

		FORCEINLINE virtual void PrepareOperation(const int32 WriteIndex) const;
		FORCEINLINE virtual void DoOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 WriteIndex, const double Weight, const bool bFirstOperation) const;
		FORCEINLINE virtual void DoOperation(const int32 PrimaryReadIndex, const FPCGPoint& SrcPoint, const int32 WriteIndex, const double Weight, const bool bFirstOperation) const = 0;
		FORCEINLINE virtual void FinalizeOperation(const int32 WriteIndex, const int32 Count, const double TotalWeight) const;

		FORCEINLINE virtual void PrepareRangeOperation(const int32 StartIndex, const int32 Range) const = 0;
		FORCEINLINE virtual void DoRangeOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 StartIndex, const TArrayView<double>& Weights, const bool bFirstOperation) const = 0;
		FORCEINLINE virtual void FinalizeRangeOperation(const int32 StartIndex, const TArrayView<int32>& Counts, const TArrayView<double>& TotalWeights) const = 0;

		virtual void ResetToDefault(int32 WriteIndex) const;
		virtual void ResetRangeToDefault(int32 StartIndex, int32 Count) const;

		virtual void Write() = 0;

	protected:
		bool bOwnsWriter = true;
		bool bInterpolationAllowed = true;
		FName AttributeName = NAME_None;
		TSet<int32>* InitializedIndices = nullptr;
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingOperation : public FDataBlendingOperationBase
	{
	protected:
		void Cleanup()
		{
			TypedAttribute = nullptr;
			if (Reader == Writer) { Reader = nullptr; }
			if (bOwnsWriter) { PCGEX_DELETE(Writer) }

			PCGEX_DELETE(Reader)

			bOwnsWriter = false;
		}

	public:
		virtual ~FDataBlendingOperation() override
		{
			Cleanup();
		}

		virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::None; };

		virtual void PrepareForData(PCGEx::FAAttributeIO* InWriter, const PCGExData::FPointIO& InSecondaryData, const PCGExData::ESource SecondarySource) override
		{
			Cleanup();
			bOwnsWriter = false;
			Writer = static_cast<PCGEx::TFAttributeWriter<T>*>(InWriter);

			bInterpolationAllowed = Writer->GetAllowsInterpolation();

			FPCGMetadataAttributeBase* Attribute = InSecondaryData.GetData(SecondarySource)->Metadata->GetMutableAttribute(AttributeName);
			if (Attribute && Attribute->GetTypeId() == Writer->UnderlyingType) { TypedAttribute = static_cast<FPCGMetadataAttribute<T>*>(Attribute); }
			else { TypedAttribute = nullptr; }

			FDataBlendingOperationBase::PrepareForData(InWriter, InSecondaryData, SecondarySource);
		}

		virtual void PrepareForData(PCGExData::FPointIO& InPrimaryData, const PCGExData::FPointIO& InSecondaryData, const PCGExData::ESource SecondarySource) override
		{
			Cleanup();
			bOwnsWriter = true;

			FPCGMetadataAttributeBase* Attribute = InSecondaryData.GetData(SecondarySource)->Metadata->GetMutableAttribute(AttributeName);

			Writer = new PCGEx::TFAttributeWriter<T>(AttributeName, T{}, Attribute ? Attribute->AllowsInterpolation() : true);
			Writer->BindAndGet(InPrimaryData);

			if (&InPrimaryData == &InSecondaryData && SecondarySource == PCGExData::ESource::Out)
			{
				Reader = Writer;
			}
			else
			{
				Reader = new PCGEx::TFAttributeReader<T>(AttributeName);
				Reader->Bind(const_cast<PCGExData::FPointIO&>(InSecondaryData));
			}

			bInterpolationAllowed = Writer->GetAllowsInterpolation() && Reader->GetAllowsInterpolation();

			if (Attribute && Attribute->GetTypeId() == Writer->UnderlyingType) { TypedAttribute = static_cast<FPCGMetadataAttribute<T>*>(Attribute); }
			else { TypedAttribute = nullptr; }

			FDataBlendingOperationBase::PrepareForData(InPrimaryData, InSecondaryData, SecondarySource);
		}

		virtual void PrepareRangeOperation(const int32 StartIndex, const int32 Range) const override
		{
			TArrayView<T> View = MakeArrayView(Writer->Values.GetData() + StartIndex, Range);
			PrepareValuesRangeOperation(View, StartIndex);
		}

		FORCEINLINE virtual void DoRangeOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 StartIndex, const TArrayView<double>& Weights, const bool bFirstOperation) const override
		{
			TArrayView<T> View = MakeArrayView(Writer->Values.GetData() + StartIndex, Weights.Num());
			DoValuesRangeOperation(PrimaryReadIndex, SecondaryReadIndex, View, Weights, bFirstOperation);
		}

		FORCEINLINE virtual void FinalizeRangeOperation(const int32 StartIndex, const TArrayView<int32>& Counts, const TArrayView<double>& TotalWeights) const override
		{
			TArrayView<T> View = MakeArrayView(Writer->Values.GetData() + StartIndex, Counts.Num());
			FinalizeValuesRangeOperation(View, Counts, TotalWeights);
		}

		FORCEINLINE virtual void PrepareValuesRangeOperation(TArrayView<T>& Values, const int32 StartIndex) const
		{
			for (int i = 0; i < Values.Num(); i++) { SinglePrepare(Values[i]); }
		}

		FORCEINLINE virtual void DoValuesRangeOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, TArrayView<T>& Values, const TArrayView<double>& Weights, const bool bFirstOperation) const
		{
			if (!bInterpolationAllowed && GetIsInterpolation())
			{
				const T B = (*Reader)[SecondaryReadIndex];
				for (int i = 0; i < Values.Num(); i++) { Values[i] = B; } // Raw copy value
			}
			else
			{
				const T A = (*Writer)[PrimaryReadIndex];
				const T B = (*Reader)[SecondaryReadIndex];
				for (int i = 0; i < Values.Num(); i++) { Values[i] = SingleOperation(A, B, Weights[i]); }
			}
		}

		FORCEINLINE virtual void DoOperation(const int32 PrimaryReadIndex, const FPCGPoint& SrcPoint, const int32 WriteIndex, const double Weight, const bool bFirstOperation) const override
		{
			const T A = (*Writer)[PrimaryReadIndex];
			const T B = TypedAttribute ? TypedAttribute->GetValueFromItemKey(SrcPoint.MetadataEntry) : A;
			(*Writer)[WriteIndex] = SingleOperation(A, B, Weight);
		}

		FORCEINLINE virtual void FinalizeValuesRangeOperation(TArrayView<T>& Values, const TArrayView<int32>& Counts, const TArrayView<double>& Weights) const
		{
			if (!bInterpolationAllowed) { return; }
			for (int i = 0; i < Values.Num(); i++) { SingleFinalize(Values[i], Counts[i], Weights[i]); }
		}

		FORCEINLINE virtual void SinglePrepare(T& A) const
		{
		};

		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const = 0;

		FORCEINLINE virtual void SingleFinalize(T& A, const int32 Count, const double Weight) const
		{
		};

		FORCEINLINE virtual void ResetToDefault(const int32 WriteIndex) const override { ResetRangeToDefault(WriteIndex, 1); }

		virtual void ResetRangeToDefault(const int32 StartIndex, const int32 Count) const override
		{
			const T DefaultValue = Writer->GetDefaultValue();
			for (int i = 0; i < Count; i++) { Writer->Values[StartIndex + i] = DefaultValue; }
		}

		FORCEINLINE virtual T GetPrimaryValue(const int32 Index) const { return (*Writer)[Index]; }
		FORCEINLINE virtual T GetSecondaryValue(const int32 Index) const { return (*Reader)[Index]; }

		virtual void Write() override { Writer->Write(); }

	protected:
		FPCGMetadataAttribute<T>* TypedAttribute = nullptr;
		PCGEx::TFAttributeWriter<T>* Writer = nullptr;
		PCGEx::FAttributeIOBase<T>* Reader = nullptr;
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingOperationWithFirstInit : public FDataBlendingOperation<T>
	{
		FORCEINLINE virtual void DoValuesRangeOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, TArrayView<T>& Values, const TArrayView<double>& Weights, const bool bFirstOperation) const override
		{
			if (bFirstOperation || !this->bInterpolationAllowed && this->GetIsInterpolation())
			{
				const T B = (*this->Reader)[SecondaryReadIndex];
				for (int i = 0; i < Values.Num(); i++) { Values[i] = B; } // Raw copy value
			}
			else
			{
				T A = (*this->Writer)[PrimaryReadIndex];
				const T B = (*this->Reader)[SecondaryReadIndex];
				for (int i = 0; i < Values.Num(); i++) { Values[i] = this->SingleOperation(A, B, Weights[i]); }
			}
		}

		FORCEINLINE virtual void DoOperation(const int32 PrimaryReadIndex, const FPCGPoint& SrcPoint, const int32 WriteIndex, const double Weight, const bool bFirstOperation) const override
		{
			const T A = (*this->Writer)[PrimaryReadIndex];
			const T B = this->TypedAttribute ? this->TypedAttribute->GetValueFromItemKey(SrcPoint.MetadataEntry) : A;

			if (bFirstOperation)
			{
				(*this->Writer)[WriteIndex] = B;
				return;
			}

			(*this->Writer)[WriteIndex] = this->SingleOperation(A, B, Weight);
		}
	};

	static void AssembleBlendingSettings(
		const FPCGExPropertiesBlendingSettings& PropertiesBlending,
		const TMap<FName, EPCGExDataBlendingType>& PerAttributeBlending,
		const PCGExData::FPointIO& SourceIO,
		FPCGExBlendingSettings& OutSettings,
		TSet<FName>& OutMissingAttributes)
	{
		PCGEx::FAttributesInfos* AttributesInfos = PCGEx::FAttributesInfos::Get(SourceIO.GetIn()->Metadata);
		OutSettings = FPCGExBlendingSettings(PropertiesBlending);
		OutSettings.BlendingFilter = EPCGExBlendingFilter::Include;

		TArray<FName> SourceAttributesList;
		PerAttributeBlending.GetKeys(SourceAttributesList);

		AttributesInfos->FindMissing(SourceAttributesList, OutMissingAttributes);

		for (const FName& Id : SourceAttributesList)
		{
			if (OutMissingAttributes.Contains(Id)) { continue; }

			OutSettings.AttributesOverrides.Add(Id, *PerAttributeBlending.Find(Id));
			OutSettings.FilteredAttributes.Add(Id);
		}

		PCGEX_DELETE(AttributesInfos)
	}

	static void AssembleBlendingSettings(
		const EPCGExDataBlendingType& DefaultBlending,
		const TSet<FName>& Attributes,
		const PCGExData::FPointIO& SourceIO,
		FPCGExBlendingSettings& OutSettings,
		TSet<FName>& OutMissingAttributes)
	{
		PCGEx::FAttributesInfos* AttributesInfos = PCGEx::FAttributesInfos::Get(SourceIO.GetIn()->Metadata);
		OutSettings = FPCGExBlendingSettings(FPCGExPropertiesBlendingSettings(EPCGExDataBlendingType::None));
		OutSettings.BlendingFilter = EPCGExBlendingFilter::Include;

		AttributesInfos->FindMissing(Attributes, OutMissingAttributes);

		for (const FName& Id : Attributes)
		{
			if (OutMissingAttributes.Contains(Id)) { continue; }

			OutSettings.AttributesOverrides.Add(Id, DefaultBlending);
			OutSettings.FilteredAttributes.Add(Id);
		}

		PCGEX_DELETE(AttributesInfos)
	}
}

namespace PCGExDataBlendingTask
{
	class PCGEXTENDEDTOOLKIT_API FBlendCompoundedIO : public FPCGExNonAbandonableTask
	{
	public:
		FBlendCompoundedIO(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		                   PCGExData::FPointIO* InTargetIO,
		                   FPCGExBlendingSettings* InBlendingSettings,
		                   PCGExData::FIdxCompoundList* InCompoundList,
		                   const FPCGExDistanceSettings& InDistSettings,
		                   PCGExGraph::FGraphMetadataSettings* InMetadataSettings = nullptr) :
			FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			TargetIO(InTargetIO),
			BlendingSettings(InBlendingSettings),
			CompoundList(InCompoundList),
			DistSettings(InDistSettings),
			MetadataSettings(InMetadataSettings)
		{
		}

		PCGExData::FPointIO* TargetIO = nullptr;
		FPCGExBlendingSettings* BlendingSettings = nullptr;
		PCGExData::FIdxCompoundList* CompoundList = nullptr;
		FPCGExDistanceSettings DistSettings;
		PCGExGraph::FGraphMetadataSettings* MetadataSettings = nullptr;

		virtual bool ExecuteTask() override;
	};

	class PCGEXTENDEDTOOLKIT_API FWriteFuseMetadata : public FPCGExNonAbandonableTask
	{
	public:
		FWriteFuseMetadata(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		                   PCGExGraph::FGraphMetadataSettings* InMetadataSettings,
		                   PCGExData::FIdxCompoundList* InCompoundList) :
			FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			MetadataSettings(InMetadataSettings),
			CompoundList(InCompoundList)
		{
		}

		PCGExGraph::FGraphMetadataSettings* MetadataSettings = nullptr;
		PCGExData::FIdxCompoundList* CompoundList = nullptr;

		virtual bool ExecuteTask() override;
	};
}
