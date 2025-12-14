// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Data/PCGExDataFilter.h"
#include "Details/PCGExDetailsAttributes.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/PCGMetadataCommon.h"

#include "PCGExBlending.generated.h"

UENUM(BlueprintType)
enum class EPCGExBlendingType : uint8
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
	UnsignedHash     = 17 UMETA(DisplayName = "Hash (Unsigned)", ToolTip="Combine the values into a hash but sort the values first to create an order-independent hash."),
	WeightNormalize  = 18 UMETA(DisplayName = "Weight (Normalize)", ToolTip="Weights based on distance to blend targets and force normalized."),
	Unset            = 200 UMETA(Hidden),
};

#define PCGEX_FOREACH_BLEND_POINTPROPERTY(MACRO)\
MACRO(Density, float, float) \
MACRO(BoundsMin,FVector, FVector) \
MACRO(BoundsMax, FVector, FVector) \
MACRO(Color,FVector4, FVector4) \
MACRO(Position,FTransform, FVector) \
MACRO(Rotation, FTransform, FQuat) \
MACRO(Scale, FTransform, FVector) \
MACRO(Steepness,float, float) \
MACRO(Seed,int32, int32)

#define PCGEX_FOREACH_DATABLENDMODE(MACRO)\
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
MACRO(UnsignedHash) \
MACRO(WeightNormalize) 

// This is a different blending list that makes more sense for AxB blending
// and also includes extra modes that don't make sense in regular multi-source data blending
UENUM(BlueprintType)
enum class EPCGExABBlendingType : uint8
{
	None             = 0 UMETA(Hidden, DisplayName = "None", ToolTip="No blending is applied, keep the original value.", ActionIcon="PCGEx.Pin.OUT_BlendOp"),
	Average          = 1 UMETA(DisplayName = "Average", ToolTip="(A + B) / 2", ActionIcon="PCGEx.Pin.OUT_BlendOp", SearchHints = "Average"),
	Weight           = 2 UMETA(DisplayName = "Weight", ToolTip="(A + B) / Weight. Values are normalize if weight > 1", ActionIcon="PCGEx.Pin.OUT_BlendOp", SearchHints = "Weight"),
	Multiply         = 3 UMETA(DisplayName = "Multiply", ToolTip="A * B", ActionIcon="PCGEx.Pin.OUT_BlendOp", SearchHints = "*"),
	Divide           = 4 UMETA(DisplayName = "Divide", ToolTip="A / B", ActionIcon="PCGEx.Pin.OUT_BlendOp", SearchHints = "Average"),
	Min              = 5 UMETA(DisplayName = "Min", ToolTip="Min(A, B)", ActionIcon="PCGEx.Pin.OUT_BlendOp"),
	Max              = 6 UMETA(DisplayName = "Max", ToolTip="Max(A, B)", ActionIcon="PCGEx.Pin.OUT_BlendOp"),
	CopyTarget       = 7 UMETA(DisplayName = "Copy (Target)", ToolTip = "= B", ActionIcon="PCGEx.Pin.OUT_BlendOp"),
	CopySource       = 8 UMETA(DisplayName = "Copy (Source)", ToolTip="= A", ActionIcon="PCGEx.Pin.OUT_BlendOp"),
	Add              = 9 UMETA(DisplayName = "Add", ToolTip = "A + B", ActionIcon="PCGEx.Pin.OUT_BlendOp", SearchHints = "+"),
	Subtract         = 10 UMETA(DisplayName = "Subtract", ToolTip="A - B", ActionIcon="PCGEx.Pin.OUT_BlendOp", SearchHints = "-"),
	WeightedAdd      = 11 UMETA(DisplayName = "Weighted Add", ToolTip = "A + (B * Weight)", ActionIcon="PCGEx.Pin.OUT_BlendOp"),
	WeightedSubtract = 12 UMETA(DisplayName = "Weighted Subtract", ToolTip="A - (B * Weight)", ActionIcon="PCGEx.Pin.OUT_BlendOp"),
	Lerp             = 13 UMETA(DisplayName = "Lerp", ToolTip="Lerp(A, B, Weight)", ActionIcon="PCGEx.Pin.OUT_BlendOp"),
	UnsignedMin      = 14 UMETA(DisplayName = "Unsigned Min", ToolTip="Min(A, B) * Sign", ActionIcon="PCGEx.Pin.OUT_BlendOp"),
	UnsignedMax      = 15 UMETA(DisplayName = "Unsigned Max", ToolTip="Max(A, B) * Sign", ActionIcon="PCGEx.Pin.OUT_BlendOp"),
	AbsoluteMin      = 16 UMETA(DisplayName = "Absolute Min", ToolTip="+Min(A, B)", ActionIcon="PCGEx.Pin.OUT_BlendOp"),
	AbsoluteMax      = 17 UMETA(DisplayName = "Absolute Max", ToolTip="+Max(A, B)", ActionIcon="PCGEx.Pin.OUT_BlendOp"),
	Hash             = 18 UMETA(DisplayName = "Hash", ToolTip="Hash(A, B)", ActionIcon="PCGEx.Pin.OUT_BlendOp"),
	UnsignedHash     = 19 UMETA(DisplayName = "Hash (Unsigned)", ToolTip="Hash(Min(A, B), Max(A, B))", ActionIcon="PCGEx.Pin.OUT_BlendOp"),
	Mod              = 20 UMETA(DisplayName = "Modulo (Simple)", ToolTip="FMod(A, cast(B))", ActionIcon="PCGEx.Pin.OUT_BlendOp", SearchHints = "%"),
	ModCW            = 21 UMETA(DisplayName = "Modulo (Component Wise)", ToolTip="FMod(A, B)", ActionIcon="PCGEx.Pin.OUT_BlendOp", SearchHints = "%"),
	WeightNormalize  = 22 UMETA(DisplayName = "Weight (Always Normalize)", ToolTip="(A + B) / Weight. Always normalize final values.", ActionIcon="PCGEx.Pin.OUT_BlendOp", SearchHints = "Weight"),
};

#define PCGEX_FOREACH_AB_BLENDMODE(MACRO)\
MACRO(None) \
MACRO(Average) \
MACRO(Weight) \
MACRO(Multiply) \
MACRO(Divide) \
MACRO(Min) \
MACRO(Max) \
MACRO(CopyTarget) \
MACRO(CopySource) \
MACRO(Add) \
MACRO(Subtract) \
MACRO(WeightedAdd) \
MACRO(WeightedSubtract) \
MACRO(Lerp) \
MACRO(UnsignedMin) \
MACRO(UnsignedMax) \
MACRO(AbsoluteMin) \
MACRO(AbsoluteMax) \
MACRO(Hash) \
MACRO(UnsignedHash) \
MACRO(Mod) \
MACRO(ModCW) \
MACRO(WeightNormalize)

struct FPCGPinProperties;
enum class EPCGPinStatus : uint8;
struct FPCGExDistanceDetails;
struct FPCGExPointPointIntersectionDetails;

namespace PCGExGraph
{
	struct FGraphMetadataDetails;
}

namespace PCGExData
{
	class FFacadePreloader;
	class FUnionMetadata;
}

UENUM()
enum class EPCGExBlendOver : uint8
{
	Distance = 0 UMETA(DisplayName = "Distance", ToolTip="Blend is based on distance over max distance"),
	Index    = 1 UMETA(DisplayName = "Count", ToolTip="Blend is based on index over total count"),
	Fixed    = 2 UMETA(DisplayName = "Fixed", ToolTip="Fixed blend lerp/weight value"),
};

UENUM()
enum class EPCGExBlendingInterface : uint8
{
	Individual = 0 UMETA(DisplayName = "Individual", ToolTip="Uses individual blend operation subnodes to get full control. Best if you're looking to pick only a few specific things."),
	Monolithic = 1 UMETA(DisplayName = "Monolithic", ToolTip="Blend attributes & properties using monolithic settings. Best if you want to grab everything, or only select things to leave out."),
};

namespace PCGExBlending
{
	const FName SourceOverridesBlendingOps = TEXT("Overrides : Blending");

	const FName SourceBlendingLabel = TEXT("Blend Ops");
	const FName OutputBlendingLabel = TEXT("Blend Op");

	PCGEXTENDEDTOOLKIT_API void ConvertBlending(const EPCGExBlendingType From, EPCGExABBlendingType& OutTo);

	PCGEXTENDEDTOOLKIT_API void DeclareBlendOpsInputs(TArray<FPCGPinProperties>& PinProperties, const EPCGPinStatus InStatus, EPCGExBlendingInterface Interface = EPCGExBlendingInterface::Individual);

	struct FBlendingParam
	{
		FBlendingParam() = default;

		FPCGAttributeIdentifier Identifier;
		FPCGAttributePropertyInputSelector Selector;
		EPCGExABBlendingType Blending = EPCGExABBlendingType::None;
		bool bIsNewAttribute = false;

		void SelectFromString(const FString& Selection);
		void Select(const FPCGAttributeIdentifier& InIdentifier);

		// Conversion from old blendmode to extended list
		void SetBlending(EPCGExBlendingType InBlending);
	};
}

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAttributeBlendToTargetDetails : public FPCGExAttributeSourceToTargetDetails
{
	GENERATED_BODY()

	FPCGExAttributeBlendToTargetDetails() = default;

	/** BlendMode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayAfter="Source"))
	EPCGExBlendingType Blending = EPCGExBlendingType::None;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExPointPropertyBlendingOverrides
{
	GENERATED_BODY()

#pragma region Property overrides

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bOverrideDensity = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Density", EditCondition="bOverrideDensity"))
	EPCGExBlendingType DensityBlending = EPCGExBlendingType::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bOverrideBoundsMin = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="BoundsMin", EditCondition="bOverrideBoundsMin"))
	EPCGExBlendingType BoundsMinBlending = EPCGExBlendingType::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bOverrideBoundsMax = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="BoundsMax", EditCondition="bOverrideBoundsMax"))
	EPCGExBlendingType BoundsMaxBlending = EPCGExBlendingType::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bOverrideColor = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Color", EditCondition="bOverrideColor"))
	EPCGExBlendingType ColorBlending = EPCGExBlendingType::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bOverridePosition = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Position", EditCondition="bOverridePosition"))
	EPCGExBlendingType PositionBlending = EPCGExBlendingType::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bOverrideRotation = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Rotation", EditCondition="bOverrideRotation"))
	EPCGExBlendingType RotationBlending = EPCGExBlendingType::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bOverrideScale = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Scale", EditCondition="bOverrideScale"))
	EPCGExBlendingType ScaleBlending = EPCGExBlendingType::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bOverrideSteepness = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Steepness", EditCondition="bOverrideSteepness"))
	EPCGExBlendingType SteepnessBlending = EPCGExBlendingType::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bOverrideSeed = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Seed", EditCondition="bOverrideSeed"))
	EPCGExBlendingType SeedBlending = EPCGExBlendingType::Average;

#pragma endregion
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExPropertiesBlendingDetails
{
	GENERATED_BODY()

	FPCGExPropertiesBlendingDetails() = default;
	explicit FPCGExPropertiesBlendingDetails(const EPCGExBlendingType InDefaultBlending);

	EPCGExBlendingType DefaultBlending = EPCGExBlendingType::Average;

#pragma region Property overrides

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Density"))
	EPCGExBlendingType DensityBlending = EPCGExBlendingType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="BoundsMin"))
	EPCGExBlendingType BoundsMinBlending = EPCGExBlendingType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="BoundsMax"))
	EPCGExBlendingType BoundsMaxBlending = EPCGExBlendingType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Color"))
	EPCGExBlendingType ColorBlending = EPCGExBlendingType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Position"))
	EPCGExBlendingType PositionBlending = EPCGExBlendingType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Rotation"))
	EPCGExBlendingType RotationBlending = EPCGExBlendingType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Scale"))
	EPCGExBlendingType ScaleBlending = EPCGExBlendingType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Steepness"))
	EPCGExBlendingType SteepnessBlending = EPCGExBlendingType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Seed"))
	EPCGExBlendingType SeedBlending = EPCGExBlendingType::None;

#pragma endregion
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExBlendingDetails
{
	GENERATED_BODY()

	FPCGExBlendingDetails() = default;

	explicit FPCGExBlendingDetails(const EPCGExBlendingType InDefaultBlending);
	explicit FPCGExBlendingDetails(const EPCGExBlendingType InDefaultBlending, const EPCGExBlendingType InPositionBlending);
	explicit FPCGExBlendingDetails(const FPCGExPropertiesBlendingDetails& InDetails);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExAttributeFilter BlendingFilter = EPCGExAttributeFilter::All;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="BlendingFilter != EPCGExAttributeFilter::All", EditConditionHides))
	TArray<FName> FilteredAttributes;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExBlendingType DefaultBlending = EPCGExBlendingType::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	FPCGExPointPropertyBlendingOverrides PropertiesOverrides;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	TMap<FName, EPCGExBlendingType> AttributesOverrides;

	FPCGExPropertiesBlendingDetails GetPropertiesBlendingDetails() const;

	bool CanBlend(const FName AttributeName) const;
	void Filter(TArray<PCGEx::FAttributeIdentity>& Identities) const;

	bool GetBlendingParam(const FPCGAttributeIdentifier& InIdentifer, PCGExBlending::FBlendingParam& OutParam) const;
	void GetPointPropertyBlendingParams(TArray<PCGExBlending::FBlendingParam>& OutParams) const;

	// Create a list of attributes & property selector representing the data we want to blend
	// Takes the reference list from source, 
	void GetBlendingParams(const UPCGMetadata* SourceMetadata, UPCGMetadata* TargetMetadata, TArray<PCGExBlending::FBlendingParam>& OutParams, TArray<FPCGAttributeIdentifier>& OutAttributeIdentifiers, const bool bSkipProperties = false, const TSet<FName>* IgnoreAttributeSet = nullptr) const;

	void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader, const TSet<FName>* IgnoredAttributes = nullptr) const;
};

namespace PCGExBlending
{
	PCGEXTENDEDTOOLKIT_API void AssembleBlendingDetails(const FPCGExPropertiesBlendingDetails& PropertiesBlending, const TMap<FName, EPCGExBlendingType>& PerAttributeBlending, const TSharedRef<PCGExData::FPointIO>& SourceIO, FPCGExBlendingDetails& OutDetails, TSet<FName>& OutMissingAttributes);

	PCGEXTENDEDTOOLKIT_API void AssembleBlendingDetails(const FPCGExPropertiesBlendingDetails& PropertiesBlending, const TMap<FName, EPCGExBlendingType>& PerAttributeBlending, const TArray<TSharedRef<PCGExData::FFacade>>& InSources, FPCGExBlendingDetails& OutDetails, TSet<FName>& OutMissingAttributes);

	PCGEXTENDEDTOOLKIT_API void AssembleBlendingDetails(const EPCGExBlendingType& DefaultBlending, const TArray<FName>& Attributes, const TSharedRef<PCGExData::FPointIO>& SourceIO, FPCGExBlendingDetails& OutDetails, TSet<FName>& OutMissingAttributes);

	PCGEXTENDEDTOOLKIT_API void AssembleBlendingDetails(const EPCGExBlendingType& DefaultBlending, const TArray<FName>& Attributes, const TArray<TSharedRef<PCGExData::FFacade>>& InSources, FPCGExBlendingDetails& OutDetails, TSet<FName>& OutMissingAttributes);

	PCGEXTENDEDTOOLKIT_API void GetFilteredIdentities(const UPCGMetadata* InMetadata, TArray<PCGEx::FAttributeIdentity>& OutIdentities, const FPCGExBlendingDetails* InBlendingDetails = nullptr, const FPCGExCarryOverDetails* InCarryOverDetails = nullptr, const TSet<FName>* IgnoreAttributeSet = nullptr);
}
