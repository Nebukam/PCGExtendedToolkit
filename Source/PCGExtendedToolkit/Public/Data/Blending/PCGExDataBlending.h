// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataFilter.h"

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

#define PCGEX_FOREACH_BLENDMODE(MACRO)\
MACRO(None) \
MACRO(Average) \
MACRO(Weight) \
MACRO(Min) \
MACRO(Max) \
MACRO(Copy) \
MACRO(Sum) \
MACRO(WeightedSum) \
MACRO(Lerp) \
MACRO(Subtract) \
MACRO(UnsignedMin) \
MACRO(UnsignedMax) \
MACRO(AbsoluteMin) \
MACRO(AbsoluteMax) \
MACRO(WeightedSubtract) \
MACRO(CopyOther)

UENUM()
enum class EPCGExBlendOver : uint8
{
	Distance = 0 UMETA(DisplayName = "Distance", ToolTip="Blend is based on distance over max distance"),
	Index    = 1 UMETA(DisplayName = "Count", ToolTip="Blend is based on index over total count"),
	Fixed    = 2 UMETA(DisplayName = "Fixed", ToolTip="Fixed blend lerp/weight value"),
};

namespace PCGExGraph
{
	struct FGraphMetadataDetails;
}

struct FPCGExDistanceDetails;
struct FPCGExPointPointIntersectionDetails;

namespace PCGExData
{
	class FUnionMetadata;
}

UENUM()
enum class EPCGExDataBlendingType : uint8
{
	None             = 0 UMETA(DisplayName = "None", ToolTip="No blending is applied, keep the original value."),
	Average          = 1 UMETA(DisplayName = "Average", ToolTip="Average all sampled values."),
	Weight           = 2 UMETA(DisplayName = "Weight", ToolTip="Weights based on distance to blend targets. If the results are unexpected, try 'Lerp' instead"),
	Min              = 3 UMETA(DisplayName = "Min", ToolTip="Component-wise MIN operation"),
	Max              = 4 UMETA(DisplayName = "Max", ToolTip="Component-wise MAX operation"),
	Copy             = 5 UMETA(DisplayName = "Copy", ToolTip = "Copy incoming data"),
	Sum              = 6 UMETA(DisplayName = "Sum", ToolTip = "Sum"),
	WeightedSum      = 7 UMETA(DisplayName = "Weighted Sum", ToolTip = "Sum of all the data, weighted"),
	Lerp             = 8 UMETA(DisplayName = "Lerp", ToolTip="Uses weight as lerp. If the results are unexpected, try 'Weight' instead."),
	Subtract         = 9 UMETA(DisplayName = "Subtract", ToolTip="Subtract."),
	UnsignedMin      = 10 UMETA(DisplayName = "Unsigned Min", ToolTip="Component-wise MIN on unsigned value, but keeps the sign on written data."),
	UnsignedMax      = 11 UMETA(DisplayName = "Unsigned Max", ToolTip="Component-wise MAX on unsigned value, but keeps the sign on written data."),
	AbsoluteMin      = 12 UMETA(DisplayName = "Absolute Min", ToolTip="Component-wise MIN of absolute value."),
	AbsoluteMax      = 13 UMETA(DisplayName = "Absolute Max", ToolTip="Component-wise MAX of absolute value."),
	WeightedSubtract = 14 UMETA(DisplayName = "Weighted Subtract", ToolTip="Substraction of all the data, weighted"),
	CopyOther        = 15 UMETA(DisplayName = "Copy (Other)", ToolTip="Same as copy, but uses the other value."),
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPointPropertyBlendingOverrides
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
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPropertiesBlendingDetails
{
	GENERATED_BODY()

	FPCGExPropertiesBlendingDetails()
	{
	}

	explicit FPCGExPropertiesBlendingDetails(const EPCGExDataBlendingType InDefaultBlending):
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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Density"))
	EPCGExDataBlendingType DensityBlending = EPCGExDataBlendingType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="BoundsMin"))
	EPCGExDataBlendingType BoundsMinBlending = EPCGExDataBlendingType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="BoundsMax"))
	EPCGExDataBlendingType BoundsMaxBlending = EPCGExDataBlendingType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Color"))
	EPCGExDataBlendingType ColorBlending = EPCGExDataBlendingType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Position"))
	EPCGExDataBlendingType PositionBlending = EPCGExDataBlendingType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Rotation"))
	EPCGExDataBlendingType RotationBlending = EPCGExDataBlendingType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Scale"))
	EPCGExDataBlendingType ScaleBlending = EPCGExDataBlendingType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Steepness"))
	EPCGExDataBlendingType SteepnessBlending = EPCGExDataBlendingType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Seed"))
	EPCGExDataBlendingType SeedBlending = EPCGExDataBlendingType::None;

#pragma endregion
};


USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBlendingDetails
{
	GENERATED_BODY()

	FPCGExBlendingDetails()
	{
	}

	explicit FPCGExBlendingDetails(const EPCGExDataBlendingType InDefaultBlending):
		DefaultBlending(InDefaultBlending)
	{
#define PCGEX_SET_DEFAULT_POINTPROPERTY(_TYPE, _NAME, _TYPENAME) PropertiesOverrides._NAME##Blending = InDefaultBlending;
		PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_SET_DEFAULT_POINTPROPERTY)
#undef PCGEX_SET_DEFAULT_POINTPROPERTY
	}

	explicit FPCGExBlendingDetails(const EPCGExDataBlendingType InDefaultBlending, const EPCGExDataBlendingType InPositionBlending):
		DefaultBlending(InDefaultBlending)
	{
#define PCGEX_SET_DEFAULT_POINTPROPERTY(_TYPE, _NAME, _TYPENAME) PropertiesOverrides._NAME##Blending = InDefaultBlending;
		PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_SET_DEFAULT_POINTPROPERTY)
#undef PCGEX_SET_DEFAULT_POINTPROPERTY

		PropertiesOverrides.bOverridePosition = true;
		PropertiesOverrides.PositionBlending = InPositionBlending;
	}

	explicit FPCGExBlendingDetails(const FPCGExPropertiesBlendingDetails& InDetails):
		DefaultBlending(InDetails.DefaultBlending)
	{
#define PCGEX_SET_DEFAULT_POINTPROPERTY(_TYPE, _NAME, _TYPENAME) PropertiesOverrides.bOverride##_NAME = InDetails._NAME##Blending != EPCGExDataBlendingType::None; PropertiesOverrides._NAME##Blending = InDetails._NAME##Blending;
		PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_SET_DEFAULT_POINTPROPERTY)
#undef PCGEX_SET_DEFAULT_POINTPROPERTY
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExAttributeFilter BlendingFilter = EPCGExAttributeFilter::All;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="BlendingFilter!=EPCGExAttributeFilter::All", EditConditionHides))
	TArray<FName> FilteredAttributes;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExDataBlendingType DefaultBlending = EPCGExDataBlendingType::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExPointPropertyBlendingOverrides PropertiesOverrides;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TMap<FName, EPCGExDataBlendingType> AttributesOverrides;

	FPCGExPropertiesBlendingDetails GetPropertiesBlendingDetails() const
	{
		FPCGExPropertiesBlendingDetails OutDetails;
#define PCGEX_SET_POINTPROPERTY(_TYPE, _NAME, _TYPENAME) OutDetails._NAME##Blending = PropertiesOverrides.bOverride##_NAME ? PropertiesOverrides._NAME##Blending : DefaultBlending;
		PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_SET_POINTPROPERTY)
#undef PCGEX_SET_POINTPROPERTY
		return OutDetails;
	}

	bool HasAnyBlending() const
	{
		return !FilteredAttributes.IsEmpty() || !GetPropertiesBlendingDetails().HasNoBlending();
	}

	bool CanBlend(const FName AttributeName) const
	{
		switch (BlendingFilter)
		{
		default: ;
		case EPCGExAttributeFilter::All:
			return true;
		case EPCGExAttributeFilter::Exclude:
			return !FilteredAttributes.Contains(AttributeName);
		case EPCGExAttributeFilter::Include:
			return FilteredAttributes.Contains(AttributeName);
		}
	}

	void Filter(TArray<PCGEx::FAttributeIdentity>& Identities) const
	{
		if (BlendingFilter == EPCGExAttributeFilter::All) { return; }
		for (int i = 0; i < Identities.Num(); i++)
		{
			if (!CanBlend(Identities[i].Name))
			{
				Identities.RemoveAt(i);
				i--;
			}
		}
	}

	void RegisterBuffersDependencies(
		FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade,
		PCGExData::FFacadePreloader& FacadePreloader,
		const TSet<FName>* IgnoredAttributes = nullptr) const
	{
		TSharedPtr<PCGEx::FAttributesInfos> Infos = PCGEx::FAttributesInfos::Get(InDataFacade->GetIn()->Metadata, IgnoredAttributes);
		Filter(Infos->Identities);
		for (const PCGEx::FAttributeIdentity& Identity : Infos->Identities) { FacadePreloader.Register(InContext, Identity); }
	}
};

namespace PCGExDataBlending
{
	const FName SourceOverridesBlendingOps = TEXT("Overrides : Blending");

	/**
	 * 
	 */
	class /*PCGEXTENDEDTOOLKIT_API*/ FDataBlendingOperationBase
	{
	public:
		virtual ~FDataBlendingOperationBase()
		{
		}

		virtual EPCGExDataBlendingType GetBlendingType() const = 0;

		void SetAttributeName(const FName InName) { AttributeName = InName; }
		FName GetAttributeName() const { return AttributeName; }

		virtual void PrepareForData(const TSharedPtr<PCGExData::FFacade>& InPrimaryFacade, const TSharedPtr<PCGExData::FFacade>& InSecondaryFacade, const PCGExData::ESource SecondarySource = PCGExData::ESource::In)
		{
			PrimaryData = InPrimaryFacade->Source->GetOut();
			SecondaryData = InSecondaryFacade->Source->GetData(SecondarySource);
		}

		virtual void PrepareForData(const TSharedPtr<PCGExData::FBufferBase>& InWriter, const TSharedPtr<PCGExData::FFacade>& InSecondaryFacade, const PCGExData::ESource SecondarySource = PCGExData::ESource::In)
		{
			PrimaryData = nullptr;
			SecondaryData = InSecondaryFacade->Source->GetData(SecondarySource);
		}

		virtual void SoftPrepareForData(const TSharedPtr<PCGExData::FFacade>& InPrimaryFacade, const TSharedPtr<PCGExData::FFacade>& InSecondaryFacade, const PCGExData::ESource SecondarySource = PCGExData::ESource::In)
		{
			PrimaryData = InPrimaryFacade->Source->GetOut();
			SecondaryData = InSecondaryFacade->Source->GetData(SecondarySource);
		}

		FORCEINLINE virtual bool GetRequiresPreparation() const { return false; }
		FORCEINLINE virtual bool GetRequiresFinalization() const { return false; }

		FORCEINLINE virtual void PrepareOperation(const int32 WriteIndex) const { PrepareRangeOperation(WriteIndex, 1); }

		FORCEINLINE virtual void Copy(const int32 WriteIndex, const int32 SecondaryReadIndex) const = 0;
		FORCEINLINE virtual void DoOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 WriteIndex, const double Weight, const int8 bFirstOperation) const
		{
			TArray<double> Weights = {Weight};
			DoRangeOperation(PrimaryReadIndex, SecondaryReadIndex, WriteIndex, Weights, bFirstOperation);
		}

		FORCEINLINE virtual void Copy(const int32 WriteIndex, const FPCGPoint& SrcPoint) const = 0;
		FORCEINLINE virtual void DoOperation(const int32 PrimaryReadIndex, const FPCGPoint& SrcPoint, const int32 WriteIndex, const double Weight, const int8 bFirstOperation) const = 0;
		FORCEINLINE virtual void CompleteOperation(const int32 WriteIndex, const int32 Count, const double TotalWeight) const
		{
			TArray<double> Weights = {TotalWeight};
			TArray<int32> Counts = {Count};
			CompleteRangeOperation(WriteIndex, Counts, Weights);
		}

		FORCEINLINE virtual void PrepareRangeOperation(const int32 StartIndex, const int32 Range) const = 0;
		FORCEINLINE virtual void DoRangeOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 StartIndex, const TArrayView<double>& Weights, const int8 bFirstOperation) const = 0;
		FORCEINLINE virtual void CompleteRangeOperation(const int32 StartIndex, const TArrayView<const int32>& Counts, const TArrayView<double>& TotalWeights) const = 0;

		// Soft ops

		FORCEINLINE virtual void PrepareOperation(const PCGMetadataEntryKey WriteKey) const = 0;

		FORCEINLINE virtual void Copy(const PCGMetadataEntryKey WriteKey, const PCGMetadataEntryKey SecondaryReadKey) const = 0;
		FORCEINLINE virtual void DoOperation(const PCGMetadataEntryKey PrimaryReadKey, const PCGMetadataEntryKey SecondaryReadKey, const PCGMetadataEntryKey WriteKey, const double Weight, const int8 bFirstOperation) const = 0;
		FORCEINLINE virtual void CompleteOperation(const PCGMetadataEntryKey WriteKey, const int32 Count, const double TotalWeight) const = 0;

	protected:
		bool bSupportInterpolation = true;
		FName AttributeName = NAME_None;
		UPCGPointData* PrimaryData = nullptr;
		const UPCGPointData* SecondaryData = nullptr;
	};

	template <typename T, EPCGExDataBlendingType BlendingType, bool bRequirePreparation = false, bool bRequireCompletion = false>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingOperation : public FDataBlendingOperationBase
	{
	protected:
		void Cleanup()
		{
			Reader = nullptr;
			Writer = nullptr;
		}

	public:
		virtual ~TDataBlendingOperation() override
		{
			Cleanup();
		}

		FORCEINLINE virtual bool GetRequiresPreparation() const override { return bRequirePreparation; }
		FORCEINLINE virtual bool GetRequiresFinalization() const override { return bRequireCompletion; }
		FORCEINLINE virtual EPCGExDataBlendingType GetBlendingType() const override { return BlendingType; };

		virtual void PrepareForData(const TSharedPtr<PCGExData::FBufferBase>& InWriter, const TSharedPtr<PCGExData::FFacade>& InSecondaryFacade, const PCGExData::ESource SecondarySource) override
		{
			Cleanup();

			FDataBlendingOperationBase::PrepareForData(InWriter, InSecondaryFacade, SecondarySource);

			Writer = StaticCastSharedPtr<PCGExData::TBuffer<T>>(InWriter);

			bSupportInterpolation = Writer->GetAllowsInterpolation();
			SourceAttribute = InSecondaryFacade->FindMutableAttribute<T>(AttributeName, SecondarySource);
		}

		virtual void PrepareForData(const TSharedPtr<PCGExData::FFacade>& InPrimaryFacade, const TSharedPtr<PCGExData::FFacade>& InSecondaryFacade, const PCGExData::ESource SecondarySource) override
		{
			Cleanup();

			FDataBlendingOperationBase::PrepareForData(InPrimaryFacade, InSecondaryFacade, SecondarySource);

			SourceAttribute = InSecondaryFacade->FindConstAttribute<T>(AttributeName, SecondarySource);

			if (SourceAttribute) { Writer = InPrimaryFacade->GetWritable<T>(SourceAttribute, PCGExData::EBufferInit::Inherit); }
			else { Writer = InPrimaryFacade->GetWritable<T>(AttributeName, T{}, true, PCGExData::EBufferInit::Inherit); }

			Reader = InSecondaryFacade->GetReadable<T>(AttributeName, SecondarySource); // Will return writer if sources ==

			bSupportInterpolation = Writer->GetAllowsInterpolation();
		}

		virtual void SoftPrepareForData(const TSharedPtr<PCGExData::FFacade>& InPrimaryFacade, const TSharedPtr<PCGExData::FFacade>& InSecondaryFacade, const PCGExData::ESource SecondarySource) override
		{
			Cleanup();

			FDataBlendingOperationBase::SoftPrepareForData(InPrimaryFacade, InSecondaryFacade, SecondarySource);

			SourceAttribute = InSecondaryFacade->FindConstAttribute<T>(AttributeName, SecondarySource);
			TargetAttribute = InPrimaryFacade->FindMutableAttribute<T>(AttributeName, PCGExData::ESource::Out);

			if (!TargetAttribute) { TargetAttribute = static_cast<FPCGMetadataAttribute<T>*>(InPrimaryFacade->GetOut()->Metadata->CopyAttribute(SourceAttribute, SourceAttribute->Name, false, false, false)); }

			check(TargetAttribute) // Something went wrong

			bSupportInterpolation = SourceAttribute->AllowsInterpolation();
		}

		virtual void PrepareRangeOperation(const int32 StartIndex, const int32 Range) const override
		{
			if constexpr (bRequirePreparation)
			{
				TArrayView<T> View = MakeArrayView(Writer->GetOutValues()->GetData() + StartIndex, Range);
				PrepareValuesRangeOperation(View, StartIndex);
			}
		}

		FORCEINLINE virtual void DoRangeOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 StartIndex, const TArrayView<double>& Weights, const int8 bFirstOperation) const override
		{
			TArrayView<T> View = MakeArrayView(Writer->GetOutValues()->GetData() + StartIndex, Weights.Num());
			DoValuesRangeOperation(PrimaryReadIndex, SecondaryReadIndex, View, Weights, bFirstOperation);
		}

		FORCEINLINE virtual void CompleteRangeOperation(const int32 StartIndex, const TArrayView<const int32>& Counts, const TArrayView<double>& TotalWeights) const override
		{
			if constexpr (bRequireCompletion)
			{
				TArrayView<T> View = MakeArrayView(Writer->GetOutValues()->GetData() + StartIndex, Counts.Num());
				CompleteValuesRangeOperation(StartIndex, View, Counts, TotalWeights);
			}
		}

		FORCEINLINE virtual void PrepareValuesRangeOperation(TArrayView<T>& Values, const int32 StartIndex) const
		{
			if constexpr (bRequirePreparation)
			{
				for (int i = 0; i < Values.Num(); i++) { SinglePrepare(Values[i]); }
			}
		}

		FORCEINLINE virtual void DoValuesRangeOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, TArrayView<T>& Values, const TArrayView<double>& Weights, const int8 bFirstOperation) const
		{
			if (!bSupportInterpolation)
			{
				const T B = Reader->Read(SecondaryReadIndex);
				for (int i = 0; i < Values.Num(); i++) { Values[i] = B; } // Raw copy value
			}
			else
			{
				const T A = Writer->GetMutable(PrimaryReadIndex);
				const T B = Reader->Read(SecondaryReadIndex);
				for (int i = 0; i < Values.Num(); i++) { Values[i] = SingleOperation(A, B, Weights[i]); }
			}
		}

		FORCEINLINE virtual void Copy(const int32 WriteIndex, const int32 SecondaryReadIndex) const override
		{
			Writer->GetMutable(WriteIndex) = Reader->Read(SecondaryReadIndex);
		}

		FORCEINLINE virtual void Copy(const int32 WriteIndex, const FPCGPoint& SrcPoint) const override
		{
			Writer->GetMutable(WriteIndex) = SourceAttribute ? SourceAttribute->GetValueFromItemKey(SrcPoint.MetadataEntry) : Writer->GetMutable(WriteIndex);
		}

		FORCEINLINE virtual void DoOperation(const int32 PrimaryReadIndex, const FPCGPoint& SrcPoint, const int32 WriteIndex, const double Weight, const int8 bFirstOperation) const override
		{
			const T A = Writer->GetMutable(PrimaryReadIndex);
			const T B = SourceAttribute ? SourceAttribute->GetValueFromItemKey(SrcPoint.MetadataEntry) : A;
			Writer->GetMutable(WriteIndex) = SingleOperation(A, B, Weight);
		}

		FORCEINLINE virtual void CompleteValuesRangeOperation(const int32 StartIndex, TArrayView<T>& Values, const TArrayView<const int32>& Counts, const TArrayView<double>& Weights) const
		{
			if constexpr (bRequireCompletion)
			{
				if (!bSupportInterpolation) { return; }
				for (int i = 0; i < Values.Num(); i++) { SingleComplete(Values[i], Counts[i], Weights[i]); }
			}
		}

		FORCEINLINE virtual void PrepareOperation(const PCGMetadataEntryKey WriteKey) const override
		{
			if constexpr (bRequirePreparation)
			{
				T Value = TargetAttribute->GetValueFromItemKey(WriteKey);
				SinglePrepare(Value);
				TargetAttribute->SetValue(WriteKey, Value);
			}
		};

		FORCEINLINE virtual void Copy(const PCGMetadataEntryKey WriteKey, const PCGMetadataEntryKey SecondaryReadKey) const override
		{
			TargetAttribute->SetValue(WriteKey, SourceAttribute->GetValueFromItemKey(SecondaryReadKey));
		};

		FORCEINLINE virtual void DoOperation(const PCGMetadataEntryKey PrimaryReadKey, const PCGMetadataEntryKey SecondaryReadKey, const PCGMetadataEntryKey WriteKey, const double Weight, const int8 bFirstOperation) const override
		{
			TargetAttribute->SetValue(WriteKey, SingleOperation(TargetAttribute->GetValueFromItemKey(PrimaryReadKey), SourceAttribute->GetValueFromItemKey(SecondaryReadKey), Weight));
		};

		FORCEINLINE virtual void CompleteOperation(const PCGMetadataEntryKey WriteKey, const int32 Count, const double TotalWeight) const override
		{
			if constexpr (bRequireCompletion)
			{
				T Value = TargetAttribute->GetValueFromItemKey(WriteKey);
				SingleComplete(Value, Count, TotalWeight);
				TargetAttribute->SetValue(WriteKey, Value);
			}
		};

		FORCEINLINE virtual void SinglePrepare(T& A) const { A = T{}; };

		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const = 0;

		FORCEINLINE virtual void SingleComplete(T& A, const int32 Count, const double Weight) const
		{
		};

	protected:
		const FPCGMetadataAttribute<T>* SourceAttribute = nullptr;
		FPCGMetadataAttribute<T>* TargetAttribute = nullptr;
		TSharedPtr<PCGExData::TBuffer<T>> Writer;
		TSharedPtr<PCGExData::TBuffer<T>> Reader;
	};

	template <typename T, EPCGExDataBlendingType BlendingType, bool bRequirePreparation = false, bool bRequireCompletion = false>
	class /*PCGEXTENDEDTOOLKIT_API*/ FDataBlendingOperationWithFirstInit : public TDataBlendingOperation<T, BlendingType, bRequirePreparation, bRequireCompletion>
	{
		FORCEINLINE virtual void DoValuesRangeOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, TArrayView<T>& Values, const TArrayView<double>& Weights, const int8 bFirstOperation) const override
		{
			if (bFirstOperation || !this->bSupportInterpolation)
			{
				const T B = this->Reader->Read(SecondaryReadIndex);
				for (int i = 0; i < Values.Num(); i++) { Values[i] = B; } // Raw copy value
			}
			else
			{
				T A = this->Writer->GetMutable(PrimaryReadIndex);
				const T B = this->Reader->Read(SecondaryReadIndex);
				for (int i = 0; i < Values.Num(); i++) { Values[i] = this->SingleOperation(A, B, Weights[i]); }
			}
		}

		FORCEINLINE virtual void DoOperation(const int32 PrimaryReadIndex, const FPCGPoint& SrcPoint, const int32 WriteIndex, const double Weight, const int8 bFirstOperation) const override
		{
			const T A = this->Writer->GetMutable(PrimaryReadIndex);
			const T B = this->SourceAttribute ? this->SourceAttribute->GetValueFromItemKey(SrcPoint.MetadataEntry) : A;

			if (bFirstOperation)
			{
				this->Writer->GetMutable(WriteIndex) = B;
				return;
			}

			this->Writer->GetMutable(WriteIndex) = this->SingleOperation(A, B, Weight);
		}
	};

	static void AssembleBlendingDetails(
		const FPCGExPropertiesBlendingDetails& PropertiesBlending,
		const TMap<FName, EPCGExDataBlendingType>& PerAttributeBlending,
		const TSharedRef<PCGExData::FPointIO>& SourceIO,
		FPCGExBlendingDetails& OutDetails,
		TSet<FName>& OutMissingAttributes)
	{
		const TSharedPtr<PCGEx::FAttributesInfos> AttributesInfos = PCGEx::FAttributesInfos::Get(SourceIO->GetIn()->Metadata);

		OutDetails = FPCGExBlendingDetails(PropertiesBlending);
		OutDetails.BlendingFilter = EPCGExAttributeFilter::Include;

		TArray<FName> SourceAttributesList;
		PerAttributeBlending.GetKeys(SourceAttributesList);

		AttributesInfos->FindMissing(SourceAttributesList, OutMissingAttributes);

		for (const FName& Id : SourceAttributesList)
		{
			if (OutMissingAttributes.Contains(Id)) { continue; }

			OutDetails.AttributesOverrides.Add(Id, *PerAttributeBlending.Find(Id));
			OutDetails.FilteredAttributes.Add(Id);
		}
	}

	static void AssembleBlendingDetails(
		const EPCGExDataBlendingType& DefaultBlending,
		const TSet<FName>& Attributes,
		const TSharedRef<PCGExData::FPointIO>& SourceIO,
		FPCGExBlendingDetails& OutDetails,
		TSet<FName>& OutMissingAttributes)
	{
		const TSharedPtr<PCGEx::FAttributesInfos> AttributesInfos = PCGEx::FAttributesInfos::Get(SourceIO->GetIn()->Metadata);
		OutDetails = FPCGExBlendingDetails(FPCGExPropertiesBlendingDetails(EPCGExDataBlendingType::None));
		OutDetails.BlendingFilter = EPCGExAttributeFilter::Include;

		AttributesInfos->FindMissing(Attributes, OutMissingAttributes);

		for (const FName& Id : Attributes)
		{
			if (OutMissingAttributes.Contains(Id)) { continue; }

			OutDetails.AttributesOverrides.Add(Id, DefaultBlending);
			OutDetails.FilteredAttributes.Add(Id);
		}
	}
}
