// Copyright 2025 Timothé Lapetite and contributors
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
MACRO(CopyOther) \
MACRO(Hash) \
MACRO(UnsignedHash)

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

UENUM(BlueprintType)
enum class EPCGExDataBlendingType : uint8
{
	None             = 0 UMETA(DisplayName = "None", ToolTip="No blending is applied, keep the original value."),
	Average          = 1 UMETA(DisplayName = "Average", ToolTip="Average all sampled values."),
	Weight           = 2 UMETA(DisplayName = "Weight", ToolTip="Weights based on distance to blend targets. If the results are unexpected, try 'Lerp' instead"),
	Min              = 3 UMETA(DisplayName = "Min", ToolTip="Component-wise MIN operation"),
	Max              = 4 UMETA(DisplayName = "Max", ToolTip="Component-wise MAX operation"),
	Copy             = 5 UMETA(DisplayName = "Copy (Target)", ToolTip = "Copy target data (second value)"),
	Sum              = 6 UMETA(DisplayName = "Sum", ToolTip = "Sum"),
	WeightedSum      = 7 UMETA(DisplayName = "Weighted Sum", ToolTip = "Sum of all the data, weighted"),
	Lerp             = 8 UMETA(DisplayName = "Lerp", ToolTip="Uses weight as lerp. If the results are unexpected, try 'Weight' instead."),
	Subtract         = 9 UMETA(DisplayName = "Subtract", ToolTip="Subtract."),
	UnsignedMin      = 10 UMETA(DisplayName = "Unsigned Min", ToolTip="Component-wise MIN on unsigned value, but keeps the sign on written data."),
	UnsignedMax      = 11 UMETA(DisplayName = "Unsigned Max", ToolTip="Component-wise MAX on unsigned value, but keeps the sign on written data."),
	AbsoluteMin      = 12 UMETA(DisplayName = "Absolute Min", ToolTip="Component-wise MIN of absolute value."),
	AbsoluteMax      = 13 UMETA(DisplayName = "Absolute Max", ToolTip="Component-wise MAX of absolute value."),
	WeightedSubtract = 14 UMETA(DisplayName = "Weighted Subtract", ToolTip="Substraction of all the data, weighted"),
	CopyOther        = 15 UMETA(DisplayName = "Copy (Source)", ToolTip="Copy source data (first value)"),
	Hash             = 16 UMETA(DisplayName = "Hash", ToolTip="Combine the values into a hash"),
	UnsignedHash     = 17 UMETA(DisplayName = "Hash (Unsigned)", ToolTip="Combine the values into a hash but sort the values first to create an order-independent hash.")
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAttributeBlendToTargetDetails : public FPCGExAttributeSourceToTargetDetails
{
	GENERATED_BODY()

	FPCGExAttributeBlendToTargetDetails() = default;

	/** BlendMode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayAfter="Source"))
	EPCGExDataBlendingType Blending = EPCGExDataBlendingType::None;
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

	FPCGExPropertiesBlendingDetails() = default;

	explicit FPCGExPropertiesBlendingDetails(const EPCGExDataBlendingType InDefaultBlending);

	bool HasNoBlending() const;

	void GetNonNoneBlendings(TArray<FName>& OutNames) const;

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

	FPCGExBlendingDetails() = default;

	explicit FPCGExBlendingDetails(const EPCGExDataBlendingType InDefaultBlending);
	explicit FPCGExBlendingDetails(const EPCGExDataBlendingType InDefaultBlending, const EPCGExDataBlendingType InPositionBlending);
	explicit FPCGExBlendingDetails(const FPCGExPropertiesBlendingDetails& InDetails);

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

	FPCGExPropertiesBlendingDetails GetPropertiesBlendingDetails() const;

	bool HasAnyBlending() const;

	bool CanBlend(const FName AttributeName) const;

	void Filter(TArray<PCGEx::FAttributeIdentity>& Identities) const;

	void RegisterBuffersDependencies(
		FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade,
		PCGExData::FFacadePreloader& FacadePreloader,
		const TSet<FName>* IgnoredAttributes = nullptr) const;
};

namespace PCGExDataBlending
{
	const FName SourceOverridesBlendingOps = TEXT("Overrides : Blending");

	const FName SourceBlendingLabel = TEXT("Blendings");
	const FName OutputBlendingLabel = TEXT("Blending");

	/**
	 * 
	 */
	class /*PCGEXTENDEDTOOLKIT_API*/ FDataBlendingProcessorBase
	{
	public:
		virtual ~FDataBlendingProcessorBase()
		{
		}

		virtual EPCGExDataBlendingType GetBlendingType() const = 0;

		void SetAttributeName(const FName InName) { AttributeName = InName; }
		FName GetAttributeName() const { return AttributeName; }

		virtual void PrepareForData(const TSharedPtr<PCGExData::FFacade>& InPrimaryFacade, const TSharedPtr<PCGExData::FFacade>& InSecondaryFacade, const PCGExData::ESource SecondarySource = PCGExData::ESource::In);

		virtual void PrepareForData(const TSharedPtr<PCGExData::FBufferBase>& InWriter, const TSharedPtr<PCGExData::FFacade>& InSecondaryFacade, const PCGExData::ESource SecondarySource = PCGExData::ESource::In);

		virtual void SoftPrepareForData(const TSharedPtr<PCGExData::FFacade>& InPrimaryFacade, const TSharedPtr<PCGExData::FFacade>& InSecondaryFacade, const PCGExData::ESource SecondarySource = PCGExData::ESource::In);

		virtual bool GetRequiresPreparation() const { return false; }
		virtual bool GetRequiresFinalization() const { return false; }

		virtual void PrepareOperation(const int32 WriteIndex) const { PrepareRangeOperation(WriteIndex, 1); }

		virtual void Copy(const int32 WriteIndex, const int32 SecondaryReadIndex) const = 0;

		virtual void DoOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 WriteIndex, const double Weight, const int8 bFirstOperation) const;

		virtual void Copy(const int32 WriteIndex, const FPCGPoint& SrcPoint) const = 0;
		virtual void DoOperation(const int32 PrimaryReadIndex, const FPCGPoint& SrcPoint, const int32 WriteIndex, const double Weight, const int8 bFirstOperation) const = 0;

		virtual void CompleteOperation(const int32 WriteIndex, const int32 Count, const double TotalWeight) const;

		virtual void PrepareRangeOperation(const int32 StartIndex, const int32 Range) const = 0;
		virtual void DoRangeOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 StartIndex, const TArrayView<double>& Weights, const int8 bFirstOperation) const = 0;
		virtual void CompleteRangeOperation(const int32 StartIndex, const TArrayView<const int32>& Counts, const TArrayView<double>& TotalWeights) const = 0;

		// Soft ops

		virtual void PrepareOperation(const PCGMetadataEntryKey WriteKey) const = 0;

		virtual void Copy(const PCGMetadataEntryKey WriteKey, const PCGMetadataEntryKey SecondaryReadKey) const = 0;
		virtual void DoOperation(const PCGMetadataEntryKey PrimaryReadKey, const PCGMetadataEntryKey SecondaryReadKey, const PCGMetadataEntryKey WriteKey, const double Weight, const int8 bFirstOperation) const = 0;
		virtual void CompleteOperation(const PCGMetadataEntryKey WriteKey, const int32 Count, const double TotalWeight) const = 0;

	protected:
		bool bSupportInterpolation = true;
		FName AttributeName = NAME_None;
		UPCGPointData* PrimaryData = nullptr;
		const UPCGPointData* SecondaryData = nullptr;
	};

	template <typename T, EPCGExDataBlendingType BlendingType, bool bRequirePreparation = false, bool bRequireCompletion = false>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingProcessor : public FDataBlendingProcessorBase
	{
	protected:
		void Cleanup()
		{
			Reader = nullptr;
			Writer = nullptr;
		}

	public:
		virtual ~TDataBlendingProcessor() override
		{
			Cleanup();
		}

		virtual bool GetRequiresPreparation() const override { return bRequirePreparation; }
		virtual bool GetRequiresFinalization() const override { return bRequireCompletion; }
		virtual EPCGExDataBlendingType GetBlendingType() const override { return BlendingType; };

		virtual void PrepareForData(const TSharedPtr<PCGExData::FBufferBase>& InWriter, const TSharedPtr<PCGExData::FFacade>& InSecondaryFacade, const PCGExData::ESource SecondarySource) override
		{
			Cleanup();

			FDataBlendingProcessorBase::PrepareForData(InWriter, InSecondaryFacade, SecondarySource);

			Writer = StaticCastSharedPtr<PCGExData::TBuffer<T>>(InWriter);

			bSupportInterpolation = Writer->GetAllowsInterpolation();
			SourceAttribute = InSecondaryFacade->FindMutableAttribute<T>(AttributeName, SecondarySource);
		}

		virtual void PrepareForData(const TSharedPtr<PCGExData::FFacade>& InPrimaryFacade, const TSharedPtr<PCGExData::FFacade>& InSecondaryFacade, const PCGExData::ESource SecondarySource) override
		{
			Cleanup();

			FDataBlendingProcessorBase::PrepareForData(InPrimaryFacade, InSecondaryFacade, SecondarySource);

			SourceAttribute = InSecondaryFacade->FindConstAttribute<T>(AttributeName, SecondarySource);

			if (SourceAttribute) { Writer = InPrimaryFacade->GetWritable<T>(SourceAttribute, PCGExData::EBufferInit::Inherit); }
			else { Writer = InPrimaryFacade->GetWritable<T>(AttributeName, T{}, true, PCGExData::EBufferInit::Inherit); }

			Reader = InSecondaryFacade->GetReadable<T>(AttributeName, SecondarySource); // Will return writer if sources ==

			bSupportInterpolation = Writer->GetAllowsInterpolation();
		}

		virtual void SoftPrepareForData(const TSharedPtr<PCGExData::FFacade>& InPrimaryFacade, const TSharedPtr<PCGExData::FFacade>& InSecondaryFacade, const PCGExData::ESource SecondarySource) override
		{
			Cleanup();

			FDataBlendingProcessorBase::SoftPrepareForData(InPrimaryFacade, InSecondaryFacade, SecondarySource);

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

		virtual void DoRangeOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 StartIndex, const TArrayView<double>& Weights, const int8 bFirstOperation) const override
		{
			TArrayView<T> View = MakeArrayView(Writer->GetOutValues()->GetData() + StartIndex, Weights.Num());
			DoValuesRangeOperation(PrimaryReadIndex, SecondaryReadIndex, View, Weights, bFirstOperation);
		}

		virtual void CompleteRangeOperation(const int32 StartIndex, const TArrayView<const int32>& Counts, const TArrayView<double>& TotalWeights) const override
		{
			if constexpr (bRequireCompletion)
			{
				TArrayView<T> View = MakeArrayView(Writer->GetOutValues()->GetData() + StartIndex, Counts.Num());
				CompleteValuesRangeOperation(StartIndex, View, Counts, TotalWeights);
			}
		}

		virtual void PrepareValuesRangeOperation(TArrayView<T>& Values, const int32 StartIndex) const
		{
			if constexpr (bRequirePreparation)
			{
				for (int i = 0; i < Values.Num(); i++) { SinglePrepare(Values[i]); }
			}
		}

		virtual void DoValuesRangeOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, TArrayView<T>& Values, const TArrayView<double>& Weights, const int8 bFirstOperation) const
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

		virtual void Copy(const int32 WriteIndex, const int32 SecondaryReadIndex) const override
		{
			Writer->GetMutable(WriteIndex) = Reader->Read(SecondaryReadIndex);
		}

		virtual void Copy(const int32 WriteIndex, const FPCGPoint& SrcPoint) const override
		{
			Writer->GetMutable(WriteIndex) = SourceAttribute ? SourceAttribute->GetValueFromItemKey(SrcPoint.MetadataEntry) : Writer->GetMutable(WriteIndex);
		}

		virtual void DoOperation(const int32 PrimaryReadIndex, const FPCGPoint& SrcPoint, const int32 WriteIndex, const double Weight, const int8 bFirstOperation) const override
		{
			const T A = Writer->GetMutable(PrimaryReadIndex);
			const T B = SourceAttribute ? SourceAttribute->GetValueFromItemKey(SrcPoint.MetadataEntry) : A;
			Writer->GetMutable(WriteIndex) = SingleOperation(A, B, Weight);
		}

		virtual void CompleteValuesRangeOperation(const int32 StartIndex, TArrayView<T>& Values, const TArrayView<const int32>& Counts, const TArrayView<double>& Weights) const
		{
			if constexpr (bRequireCompletion)
			{
				if (!bSupportInterpolation) { return; }
				for (int i = 0; i < Values.Num(); i++) { SingleComplete(Values[i], Counts[i], Weights[i]); }
			}
		}

		virtual void PrepareOperation(const PCGMetadataEntryKey WriteKey) const override
		{
			if constexpr (bRequirePreparation)
			{
				T Value = TargetAttribute->GetValueFromItemKey(WriteKey);
				SinglePrepare(Value);
				TargetAttribute->SetValue(WriteKey, Value);
			}
		};

		virtual void Copy(const PCGMetadataEntryKey WriteKey, const PCGMetadataEntryKey SecondaryReadKey) const override
		{
			TargetAttribute->SetValue(WriteKey, SourceAttribute->GetValueFromItemKey(SecondaryReadKey));
		};

		virtual void DoOperation(const PCGMetadataEntryKey PrimaryReadKey, const PCGMetadataEntryKey SecondaryReadKey, const PCGMetadataEntryKey WriteKey, const double Weight, const int8 bFirstOperation) const override
		{
			TargetAttribute->SetValue(WriteKey, SingleOperation(TargetAttribute->GetValueFromItemKey(PrimaryReadKey), SourceAttribute->GetValueFromItemKey(SecondaryReadKey), Weight));
		};

		virtual void CompleteOperation(const PCGMetadataEntryKey WriteKey, const int32 Count, const double TotalWeight) const override
		{
			if constexpr (bRequireCompletion)
			{
				T Value = TargetAttribute->GetValueFromItemKey(WriteKey);
				SingleComplete(Value, Count, TotalWeight);
				TargetAttribute->SetValue(WriteKey, Value);
			}
		};

		virtual void SinglePrepare(T& A) const { A = T{}; };

		virtual T SingleOperation(T A, T B, double Weight) const = 0;

		virtual void SingleComplete(T& A, const int32 Count, const double Weight) const
		{
		};

	protected:
		const FPCGMetadataAttribute<T>* SourceAttribute = nullptr;
		FPCGMetadataAttribute<T>* TargetAttribute = nullptr;
		TSharedPtr<PCGExData::TBuffer<T>> Writer;
		TSharedPtr<PCGExData::TBuffer<T>> Reader;
	};

	template <typename T, EPCGExDataBlendingType BlendingType, bool bRequirePreparation = false, bool bRequireCompletion = false>
	class /*PCGEXTENDEDTOOLKIT_API*/ FDataBlendingProcessorWithFirstInit : public TDataBlendingProcessor<T, BlendingType, bRequirePreparation, bRequireCompletion>
	{
		virtual void DoValuesRangeOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, TArrayView<T>& Values, const TArrayView<double>& Weights, const int8 bFirstOperation) const override
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

		virtual void DoOperation(const int32 PrimaryReadIndex, const FPCGPoint& SrcPoint, const int32 WriteIndex, const double Weight, const int8 bFirstOperation) const override
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

	void AssembleBlendingDetails(
		const FPCGExPropertiesBlendingDetails& PropertiesBlending,
		const TMap<FName, EPCGExDataBlendingType>& PerAttributeBlending,
		const TSharedRef<PCGExData::FPointIO>& SourceIO,
		FPCGExBlendingDetails& OutDetails,
		TSet<FName>& OutMissingAttributes);

	void AssembleBlendingDetails(
		const EPCGExDataBlendingType& DefaultBlending,
		const TArray<FName>& Attributes,
		const TSharedRef<PCGExData::FPointIO>& SourceIO,
		FPCGExBlendingDetails& OutDetails,
		TSet<FName>& OutMissingAttributes);
}
