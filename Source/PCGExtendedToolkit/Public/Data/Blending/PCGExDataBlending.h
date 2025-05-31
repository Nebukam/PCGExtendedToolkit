// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataFilter.h"

#include "PCGExDataBlending.generated.h"

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
MACRO(UnsignedHash)

struct FPCGExDistanceDetails;
struct FPCGExPointPointIntersectionDetails;

namespace PCGExGraph
{
	struct FGraphMetadataDetails;
}

namespace PCGExData
{
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

#define BOOKMARK_BLENDMODE // Bookmark a location in code that need to be updated when adding new blendmodes

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
	UnsignedHash     = 17 UMETA(DisplayName = "Hash (Unsigned)", ToolTip="Combine the values into a hash but sort the values first to create an order-independent hash."),
	Unset             = 200 UMETA(Hidden),
};

// This is a different blending list that makes more sense for AxB blending
// and also includes extra modes that don't make sense in regular multi-source data blending
UENUM(BlueprintType)
enum class EPCGExABBlendingType : uint8
{
	None             = 0 UMETA(Hidden, DisplayName = "None", ToolTip="No blending is applied, keep the original value."),
	Average          = 1 UMETA(DisplayName = "Average", ToolTip="(A + B) / 2"),
	Weight           = 2 UMETA(DisplayName = "Weight", ToolTip="(A + B) / Weight"),
	Multiply         = 3 UMETA(DisplayName = "Multiply", ToolTip="A * B"),
	Divide           = 4 UMETA(DisplayName = "Divide", ToolTip="A / B"),
	Min              = 5 UMETA(DisplayName = "Min", ToolTip="Min(A, B)"),
	Max              = 6 UMETA(DisplayName = "Max", ToolTip="Max(A, B)"),
	CopyTarget       = 7 UMETA(DisplayName = "Copy (Target)", ToolTip = "= B"),
	CopySource       = 8 UMETA(DisplayName = "Copy (Source)", ToolTip="= A"),
	Add              = 9 UMETA(DisplayName = "Add", ToolTip = "A + B"),
	Subtract         = 10 UMETA(DisplayName = "Subtract", ToolTip="A - B"),
	WeightedAdd      = 11 UMETA(DisplayName = "Weighted Add", ToolTip = "A + (B * Weight)"),
	WeightedSubtract = 12 UMETA(DisplayName = "Weighted Subtract", ToolTip="A - (B * Weight)"),
	Lerp             = 13 UMETA(DisplayName = "Lerp", ToolTip="Lerp(A, B, Weight)"),
	UnsignedMin      = 14 UMETA(DisplayName = "Unsigned Min", ToolTip="Min(A, B) * Sign"),
	UnsignedMax      = 15 UMETA(DisplayName = "Unsigned Max", ToolTip="Max(A, B) * Sign"),
	AbsoluteMin      = 16 UMETA(DisplayName = "Absolute Min", ToolTip="+Min(A, B)"),
	AbsoluteMax      = 17 UMETA(DisplayName = "Absolute Max", ToolTip="+Max(A, B)"),
	Hash             = 18 UMETA(DisplayName = "Hash", ToolTip="Hash(A, B)"),
	UnsignedHash     = 19 UMETA(DisplayName = "Hash (Unsigned)", ToolTip="Hash(Min(A, B), Max(A, B))"),
	Mod              = 20 UMETA(DisplayName = "Modulo (Simple)", ToolTip="FMod(A, cast(B))"),
	ModCW            = 21 UMETA(DisplayName = "Modulo (Component Wise)", ToolTip="FMod(A, B)")
};

#define PCGEX_FOREACH_PROXYBLENDMODE(MACRO)\
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
MACRO(ModCW)

namespace PCGExDataBlending
{
	const FName SourceOverridesBlendingOps = TEXT("Overrides : Blending");

	const FName SourceBlendingLabel = TEXT("Blend Ops");
	const FName OutputBlendingLabel = TEXT("Blend Op");

	struct FBlendingParam
	{
		FBlendingParam() = default;

		FPCGAttributePropertyInputSelector Selector;
		EPCGExABBlendingType Blending = EPCGExABBlendingType::None;
		bool bIsNewAttribute = false;

		void Select(const FString& Selection);

		// Conversion from old blendmode to extended list
		void SetBlending(EPCGExDataBlendingType InBlending);
	};
}

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAttributeBlendToTargetDetails : public FPCGExAttributeSourceToTargetDetails
{
	GENERATED_BODY()

	FPCGExAttributeBlendToTargetDetails() = default;

	/** BlendMode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayAfter="Source"))
	EPCGExDataBlendingType Blending = EPCGExDataBlendingType::None;
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
struct PCGEXTENDEDTOOLKIT_API FPCGExPropertiesBlendingDetails
{
	GENERATED_BODY()

	FPCGExPropertiesBlendingDetails() = default;
	explicit FPCGExPropertiesBlendingDetails(const EPCGExDataBlendingType InDefaultBlending);

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
struct PCGEXTENDEDTOOLKIT_API FPCGExBlendingDetails
{
	GENERATED_BODY()

	FPCGExBlendingDetails() = default;

	explicit FPCGExBlendingDetails(const EPCGExDataBlendingType InDefaultBlending);
	explicit FPCGExBlendingDetails(const EPCGExDataBlendingType InDefaultBlending, const EPCGExDataBlendingType InPositionBlending);
	explicit FPCGExBlendingDetails(const FPCGExPropertiesBlendingDetails& InDetails);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExAttributeFilter BlendingFilter = EPCGExAttributeFilter::All;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="BlendingFilter!=EPCGExAttributeFilter::All", EditConditionHides))
	TArray<FName> FilteredAttributes;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExDataBlendingType DefaultBlending = EPCGExDataBlendingType::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	FPCGExPointPropertyBlendingOverrides PropertiesOverrides;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	TMap<FName, EPCGExDataBlendingType> AttributesOverrides;

	FPCGExPropertiesBlendingDetails GetPropertiesBlendingDetails() const;

	bool CanBlend(const FName AttributeName) const;
	void Filter(TArray<PCGEx::FAttributeIdentity>& Identities) const;

	bool GetBlendingParam(FName InName, PCGExDataBlending::FBlendingParam& OutParam) const;
	void GetPointPropertyBlendingParams(TArray<PCGExDataBlending::FBlendingParam>& OutParams) const;

	// Create a list of attributes & property selector representing the data we want to blend
	// Takes the reference list from source, 
	void GetBlendingParams(
		const UPCGMetadata* SourceMetadata, UPCGMetadata* TargetMetadata,
		TArray<PCGExDataBlending::FBlendingParam>& OutParams,
		const bool bSkipProperties = false,
		const TSet<FName>* IgnoreAttributeSet = nullptr) const;

	void RegisterBuffersDependencies(
		FPCGExContext* InContext,
		PCGExData::FFacadePreloader& FacadePreloader,
		const TSet<FName>* IgnoredAttributes = nullptr) const;
};

namespace PCGExDataBlending
{
	PCGEXTENDEDTOOLKIT_API
	void AssembleBlendingDetails(
		const FPCGExPropertiesBlendingDetails& PropertiesBlending,
		const TMap<FName, EPCGExDataBlendingType>& PerAttributeBlending,
		const TSharedRef<PCGExData::FPointIO>& SourceIO,
		FPCGExBlendingDetails& OutDetails,
		TSet<FName>& OutMissingAttributes);

	PCGEXTENDEDTOOLKIT_API
	void AssembleBlendingDetails(
		const EPCGExDataBlendingType& DefaultBlending,
		const TArray<FName>& Attributes,
		const TSharedRef<PCGExData::FPointIO>& SourceIO,
		FPCGExBlendingDetails& OutDetails,
		TSet<FName>& OutMissingAttributes);

	PCGEXTENDEDTOOLKIT_API
	void GetFilteredIdentities(
		const UPCGMetadata* InMetadata,
		TArray<PCGEx::FAttributeIdentity>& OutIdentities,		
		const FPCGExBlendingDetails* InBlendingDetails= nullptr,
		const FPCGExCarryOverDetails* InCarryOverDetails= nullptr,
		const TSet<FName>* IgnoreAttributeSet = nullptr);
	
}
