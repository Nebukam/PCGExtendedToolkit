// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGEx.h"
#include "PCGExMT.h"
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

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Blend Over Mode"))
enum class EPCGExBlendOver : uint8
{
	Distance UMETA(DisplayName = "Distance", ToolTip="Blend is based on distance over max distance"),
	Index UMETA(DisplayName = "Count", ToolTip="Blend is based on index over total count"),
	Fixed UMETA(DisplayName = "Fixed", ToolTip="Fixed blend lerp/weight value"),
};

namespace PCGExGraph
{
	struct FGraphMetadataDetails;
}

struct FPCGExDistanceDetails;
struct FPCGExPointPointIntersectionDetails;

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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Setting)
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
};

namespace PCGExDataBlending
{
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

		virtual void PrepareForData(PCGExData::FFacade* InPrimaryFacade, PCGExData::FFacade* InSecondaryFacade, const PCGExData::ESource SecondarySource = PCGExData::ESource::In)
		{
		}

		virtual void PrepareForData(PCGEx::FAttributeIOBase* InWriter, PCGExData::FFacade* InSecondaryFacade, const PCGExData::ESource SecondarySource = PCGExData::ESource::In)
		{
		}

		FORCEINLINE virtual bool GetIsInterpolation() const { return false; }
		FORCEINLINE virtual bool GetRequiresPreparation() const { return false; }
		FORCEINLINE virtual bool GetRequiresFinalization() const { return false; }

		FORCEINLINE virtual void PrepareOperation(const int32 WriteIndex) const { PrepareRangeOperation(WriteIndex, 1); }
		FORCEINLINE virtual void DoOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 WriteIndex, const double Weight, const bool bFirstOperation) const
		{
			TArray<double> Weights = {Weight};
			DoRangeOperation(PrimaryReadIndex, SecondaryReadIndex, WriteIndex, Weights, bFirstOperation);
		}

		FORCEINLINE virtual void DoOperation(const int32 PrimaryReadIndex, const FPCGPoint& SrcPoint, const int32 WriteIndex, const double Weight, const bool bFirstOperation) const = 0;
		FORCEINLINE virtual void FinalizeOperation(const int32 WriteIndex, const int32 Count, const double TotalWeight) const
		{
			TArray<double> Weights = {TotalWeight};
			TArray<int32> Counts = {Count};
			FinalizeRangeOperation(WriteIndex, Counts, Weights);
		}

		FORCEINLINE virtual void PrepareRangeOperation(const int32 StartIndex, const int32 Range) const = 0;
		FORCEINLINE virtual void DoRangeOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 StartIndex, const TArrayView<double>& Weights, const bool bFirstOperation) const = 0;
		FORCEINLINE virtual void FinalizeRangeOperation(const int32 StartIndex, const TArrayView<const int32>& Counts, const TArrayView<double>& TotalWeights) const = 0;

	protected:
		bool bDoInterpolation = true;
		FName AttributeName = NAME_None;
	};

	template <typename T>
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

		virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::None; };

		virtual void PrepareForData(PCGEx::FAttributeIOBase* InWriter, PCGExData::FFacade* InSecondaryFacade, const PCGExData::ESource SecondarySource) override
		{
			Cleanup();
			Writer = static_cast<PCGEx::TAttributeWriter<T>*>(InWriter);

			bDoInterpolation = Writer->GetAllowsInterpolation() && GetIsInterpolation();
			TypedAttribute = InSecondaryFacade->FindMutableAttribute<T>(AttributeName, SecondarySource);

			FDataBlendingOperationBase::PrepareForData(InWriter, InSecondaryFacade, SecondarySource);
		}

		virtual void PrepareForData(PCGExData::FFacade* InPrimaryFacade, PCGExData::FFacade* InSecondaryFacade, const PCGExData::ESource SecondarySource) override
		{
			Cleanup();

			TypedAttribute = InSecondaryFacade->FindMutableAttribute<T>(AttributeName, SecondarySource);

			if (TypedAttribute) { Writer = InPrimaryFacade->GetWriter<T>(TypedAttribute, false); }
			else { Writer = InPrimaryFacade->GetWriter<T>(AttributeName, T{}, true, false); }

			Reader = InSecondaryFacade->GetReader<T>(AttributeName, SecondarySource); // Will return writer is sources ==

			bDoInterpolation = Writer->GetAllowsInterpolation() && GetIsInterpolation();

			FDataBlendingOperationBase::PrepareForData(InPrimaryFacade, InSecondaryFacade, SecondarySource);
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

		FORCEINLINE virtual void FinalizeRangeOperation(const int32 StartIndex, const TArrayView<const int32>& Counts, const TArrayView<double>& TotalWeights) const override
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
			if (!bDoInterpolation)
			{
				const T B = Reader->Values[SecondaryReadIndex];
				for (int i = 0; i < Values.Num(); i++) { Values[i] = B; } // Raw copy value
			}
			else
			{
				const T A = Writer->Values[PrimaryReadIndex];
				const T B = Reader->Values[SecondaryReadIndex];
				for (int i = 0; i < Values.Num(); i++) { Values[i] = SingleOperation(A, B, Weights[i]); }
			}
		}

		FORCEINLINE virtual void DoOperation(const int32 PrimaryReadIndex, const FPCGPoint& SrcPoint, const int32 WriteIndex, const double Weight, const bool bFirstOperation) const override
		{
			const T A = Writer->Values[PrimaryReadIndex];
			const T B = TypedAttribute ? TypedAttribute->GetValueFromItemKey(SrcPoint.MetadataEntry) : A;
			Writer->Values[WriteIndex] = SingleOperation(A, B, Weight);
		}

		FORCEINLINE virtual void FinalizeValuesRangeOperation(TArrayView<T>& Values, const TArrayView<const int32>& Counts, const TArrayView<double>& Weights) const
		{
			if (!bDoInterpolation) { return; }
			for (int i = 0; i < Values.Num(); i++) { SingleFinalize(Values[i], Counts[i], Weights[i]); }
		}

		FORCEINLINE virtual void SinglePrepare(T& A) const
		{
		};

		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const = 0;

		FORCEINLINE virtual void SingleFinalize(T& A, const int32 Count, const double Weight) const
		{
		};

	protected:
		FPCGMetadataAttribute<T>* TypedAttribute = nullptr;
		PCGEx::TAttributeWriter<T>* Writer = nullptr;
		PCGEx::TAttributeIO<T>* Reader = nullptr;
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ FDataBlendingOperationWithFirstInit : public TDataBlendingOperation<T>
	{
		FORCEINLINE virtual void DoValuesRangeOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, TArrayView<T>& Values, const TArrayView<double>& Weights, const bool bFirstOperation) const override
		{
			if (bFirstOperation || !this->bDoInterpolation)
			{
				const T B = this->Reader->Values[SecondaryReadIndex];
				for (int i = 0; i < Values.Num(); i++) { Values[i] = B; } // Raw copy value
			}
			else
			{
				T A = this->Writer->Values[PrimaryReadIndex];
				const T B = this->Reader->Values[SecondaryReadIndex];
				for (int i = 0; i < Values.Num(); i++) { Values[i] = this->SingleOperation(A, B, Weights[i]); }
			}
		}

		FORCEINLINE virtual void DoOperation(const int32 PrimaryReadIndex, const FPCGPoint& SrcPoint, const int32 WriteIndex, const double Weight, const bool bFirstOperation) const override
		{
			const T A = this->Writer->Values[PrimaryReadIndex];
			const T B = this->TypedAttribute ? this->TypedAttribute->GetValueFromItemKey(SrcPoint.MetadataEntry) : A;

			if (bFirstOperation)
			{
				this->Writer->Values[WriteIndex] = B;
				return;
			}

			this->Writer->Values[WriteIndex] = this->SingleOperation(A, B, Weight);
		}
	};

	static void AssembleBlendingDetails(
		const FPCGExPropertiesBlendingDetails& PropertiesBlending,
		const TMap<FName, EPCGExDataBlendingType>& PerAttributeBlending,
		const PCGExData::FPointIO* SourceIO,
		FPCGExBlendingDetails& OutDetails,
		TSet<FName>& OutMissingAttributes)
	{
		PCGEx::FAttributesInfos* AttributesInfos = PCGEx::FAttributesInfos::Get(SourceIO->GetIn()->Metadata);
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

		PCGEX_DELETE(AttributesInfos)
	}

	static void AssembleBlendingDetails(
		const EPCGExDataBlendingType& DefaultBlending,
		const TSet<FName>& Attributes,
		const PCGExData::FPointIO* SourceIO,
		FPCGExBlendingDetails& OutDetails,
		TSet<FName>& OutMissingAttributes)
	{
		PCGEx::FAttributesInfos* AttributesInfos = PCGEx::FAttributesInfos::Get(SourceIO->GetIn()->Metadata);
		OutDetails = FPCGExBlendingDetails(FPCGExPropertiesBlendingDetails(EPCGExDataBlendingType::None));
		OutDetails.BlendingFilter = EPCGExAttributeFilter::Include;

		AttributesInfos->FindMissing(Attributes, OutMissingAttributes);

		for (const FName& Id : Attributes)
		{
			if (OutMissingAttributes.Contains(Id)) { continue; }

			OutDetails.AttributesOverrides.Add(Id, DefaultBlending);
			OutDetails.FilteredAttributes.Add(Id);
		}

		PCGEX_DELETE(AttributesInfos)
	}
}
