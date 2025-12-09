// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGEx.h"
#include "PCGExOctree.h"
#include "Data/PCGExDataPreloader.h"
#include "Data/PCGExUnionData.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#include "PCGExSampling.generated.h"

// Declaration & use pair, boolean will be set by name validation
#define PCGEX_OUTPUT_DECL_TOGGLE(_NAME, _TYPE, _DEFAULT_VALUE) bool bWrite##_NAME = false;
#define PCGEX_OUTPUT_DECL(_NAME, _TYPE, _DEFAULT_VALUE) TSharedPtr<PCGExData::TBuffer<_TYPE>> _NAME##Writer;
#define PCGEX_OUTPUT_DECL_AND_TOGGLE(_NAME, _TYPE, _DEFAULT_VALUE) PCGEX_OUTPUT_DECL_TOGGLE(_NAME, _TYPE, _DEFAULT_VALUE) PCGEX_OUTPUT_DECL(_NAME, _TYPE, _DEFAULT_VALUE)

// Simply validate name from settings
#define PCGEX_OUTPUT_VALIDATE_NAME(_NAME, _TYPE, _DEFAULT_VALUE)\
Context->bWrite##_NAME = Settings->bWrite##_NAME; \
if(Context->bWrite##_NAME && !PCGEx::IsWritableAttributeName(Settings->_NAME##AttributeName))\
{ PCGE_LOG(Warning, GraphAndLog, FTEXT("Invalid output attribute name for " #_NAME )); Context->bWrite##_NAME = false; }

#define PCGEX_OUTPUT_INIT(_NAME, _TYPE, _DEFAULT_VALUE) if(Context->bWrite##_NAME){ _NAME##Writer = OutputFacade->GetWritable<_TYPE>(Settings->_NAME##AttributeName, _DEFAULT_VALUE, true, PCGExData::EBufferInit::Inherit); }
#define PCGEX_OUTPUT_VALUE(_NAME, _INDEX, _VALUE) if(_NAME##Writer){_NAME##Writer->SetValue(_INDEX, _VALUE); }

struct FPCGExDistanceDetails;

namespace PCGExDetails
{
	class FDistances;
}

namespace PCGExData
{
	class FFacade;
	class FMultiFacadePreloader;
}

struct FPCGExMatchingDetails;

namespace PCGExMatching
{
	class FMatchingScope;
	class FDataMatcher;
}

UENUM()
enum class EPCGExRangeType : uint8
{
	FullRange      = 0 UMETA(DisplayName = "Full Range", ToolTip="Normalize in the [0..1] range using [0..Max Value] range."),
	EffectiveRange = 1 UMETA(DisplayName = "Effective Range", ToolTip="Remap the input [Min..Max] range to [0..1]."),
};

UENUM()
enum class EPCGExSurfaceSource : uint8
{
	All             = 0 UMETA(DisplayName = "Any surface", ToolTip="Any surface within range will be tested"),
	ActorReferences = 1 UMETA(DisplayName = "Actor Reference", ToolTip="Only a list of actor surfaces will be included."),
};


UENUM()
enum class EPCGExSampleMethod : uint8
{
	WithinRange    = 0 UMETA(DisplayName = "All (Within range)", ToolTip="Use RangeMax = 0 to include all targets"),
	ClosestTarget  = 1 UMETA(DisplayName = "Closest Target", ToolTip="Picks & process the closest target only"),
	FarthestTarget = 2 UMETA(DisplayName = "Farthest Target", ToolTip="Picks & process the farthest target only"),
	BestCandidate  = 3 UMETA(DisplayName = "Best Candidate", ToolTip="Picks & process the best candidate based on sorting rules"),
};

UENUM()
enum class EPCGExSampleSource : uint8
{
	Source   = 0 UMETA(DisplayName = "In", ToolTip="Read value on main inputs"),
	Target   = 1 UMETA(DisplayName = "Target", ToolTip="Read value on target"),
	Constant = 2 UMETA(DisplayName = "Constant", ToolTip="Read constant", ActionIcon="Constant"),
};

UENUM()
enum class EPCGExAngleRange : uint8
{
	URadians               = 0 UMETA(DisplayName = "Radians (0..+PI)", ToolTip="0..+PI"),
	PIRadians              = 1 UMETA(DisplayName = "Radians (-PI..+PI)", ToolTip="-PI..+PI"),
	TAURadians             = 2 UMETA(DisplayName = "Radians (0..+TAU)", ToolTip="0..TAU"),
	UDegrees               = 3 UMETA(DisplayName = "Degrees (0..+180)", ToolTip="0..+180"),
	PIDegrees              = 4 UMETA(DisplayName = "Degrees (-180..+180)", ToolTip="-180..+180"),
	TAUDegrees             = 5 UMETA(DisplayName = "Degrees (0..+360)", ToolTip="0..+360"),
	NormalizedHalf         = 6 UMETA(DisplayName = "Normalized Half (0..180 -> 0..1)", ToolTip="0..180 -> 0..1"),
	Normalized             = 7 UMETA(DisplayName = "Normalized (0..+360 -> 0..1)", ToolTip="0..+360 -> 0..1"),
	InvertedNormalizedHalf = 8 UMETA(DisplayName = "Inv. Normalized Half (0..180 -> 1..0)", ToolTip="0..180 -> 1..0"),
	InvertedNormalized     = 9 UMETA(DisplayName = "Inv. Normalized (0..+360 -> 1..0)", ToolTip="0..+360 -> 1..0"),
};

UENUM()
enum class EPCGExSampleWeightMode : uint8
{
	Distance      = 0 UMETA(DisplayName = "Distance", ToolTip="Weight is computed using distance to targets"),
	Attribute     = 1 UMETA(DisplayName = "Attribute", ToolTip="Uses a fixed attribute value on the target as weight"),
	AttributeMult = 2 UMETA(DisplayName = "Att x Dist", ToolTip="Uses a fixed attribute value on the target as a multiplier to distance-based weight"),
};

UENUM(meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Component Flags"))
enum class EPCGExApplySampledComponentFlags : uint8
{
	None = 0,
	X    = 1 << 0 UMETA(DisplayName = "X", ToolTip="Apply X Component", ActionIcon="X"),
	Y    = 1 << 1 UMETA(DisplayName = "Y", ToolTip="Apply Y Component", ActionIcon="Y"),
	Z    = 1 << 2 UMETA(DisplayName = "Z", ToolTip="Apply Z Component", ActionIcon="Z"),
	All  = X | Y | Z UMETA(Hidden, DisplayName = "All", ToolTip="Apply all Component"),
};

ENUM_CLASS_FLAGS(EPCGExApplySampledComponentFlags)
using EPCGExApplySampledComponentFlagsBitmask = TEnumAsByte<EPCGExApplySampledComponentFlags>;

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExApplySamplingDetails
{
	GENERATED_BODY()

	FPCGExApplySamplingDetails() = default;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bApplyTransform = false;

	/** Which position components from the sampled transform should be applied to the point.  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName=" ├─ Position", EditCondition="bApplyTransform", EditConditionHides, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExApplySampledComponentFlags"))
	uint8 TransformPosition = 0;

	/** Which rotation components from the sampled transform should be applied to the point.  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName=" ├─ Rotation", EditCondition="bApplyTransform", EditConditionHides, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExApplySampledComponentFlags"))
	uint8 TransformRotation = 0;

	/** Which scale components from the sampled transform should be applied to the point.  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName=" └─ Scale", EditCondition="bApplyTransform", EditConditionHides, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExApplySampledComponentFlags"))
	uint8 TransformScale = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bApplyLookAt = false;

	/** Which position components from the sampled look at should be applied to the point.  */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName=" └─ Position", EditCondition="bApplyLookAt", EditConditionHides, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExApplySampledComponentFlags"))
	uint8 LookAtPosition = 0;

	/** Which rotation components from the sampled look at should be applied to the point.  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName=" └─ Rotation", EditCondition="bApplyLookAt", EditConditionHides, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExApplySampledComponentFlags"))
	uint8 LookAtRotation = 0;

	/** Which scale components from the sampled look at should be applied to the point.  */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName=" └─ Scale", EditCondition="bApplyLookAt", EditConditionHides, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExApplySampledComponentFlags"))
	uint8 LookAtScale = 0;

	int32 AppliedComponents = 0;
	TArray<int32> TrPosComponents;
	TArray<int32> TrRotComponents;
	TArray<int32> TrScaComponents;
	//TArray<int32> LkPosComponents;
	TArray<int32> LkRotComponents;
	//TArray<int32> LkScaComponents;

	bool WantsApply() const;

	void Init();

	void Apply(PCGExData::FMutablePoint& InPoint, const FTransform& InTransform, const FTransform& InLookAt);
};

namespace PCGExSampling
{
	const FName SourceIgnoreActorsLabel = TEXT("InIgnoreActors");
	const FName SourceActorReferencesLabel = TEXT("ActorReferences");
	const FName OutputSampledActorsLabel = TEXT("OutSampledActors");

	double GetAngle(const EPCGExAngleRange Mode, const FVector& A, const FVector& B);

	bool GetIncludedActors(const FPCGContext* InContext, const TSharedRef<PCGExData::FFacade>& InFacade, const FName ActorReferenceName, TMap<AActor*, int32>& OutActorSet);

	class PCGEXTENDEDTOOLKIT_API FSampingUnionData : public PCGExData::IUnionData
	{
	public:
		TMap<PCGExData::FElement, double> Weights;

		FSampingUnionData() = default;
		virtual ~FSampingUnionData() override = default;

		double WeightRange = -1;

		// Gather data into arrays and return the required iteration count
		virtual int32 ComputeWeights(const TArray<const UPCGBasePointData*>& Sources, const TSharedPtr<PCGEx::FIndexLookup>& IdxLookup, const PCGExData::FPoint& Target, const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails, TArray<PCGExData::FWeightedPoint>& OutWeightedPoints) const override;

		FORCEINLINE void AddWeighted_Unsafe(const PCGExData::FElement& Element, const double InWeight)
		{
			Add_Unsafe(Element.Index, Element.IO);
			Weights.Add(Element, InWeight);
		}

		void AddWeighted(const PCGExData::FElement& Element, const double InWeight);

		double GetWeightAverage() const;
		double GetSqrtWeightAverage() const;

		virtual void Reset() override
		{
			IUnionData::Reset();
			Weights.Reset();
		}
	};

	class FTargetsHandler : public TSharedFromThis<FTargetsHandler>
	{
	protected:
		TSharedPtr<PCGExOctree::FItemOctree> TargetsOctree;
		TArray<TSharedRef<PCGExData::FFacade>> TargetFacades;
		TArray<const PCGPointOctree::FPointOctree*> TargetOctrees;
		int32 MaxNumTargets = 0;

		TSharedPtr<PCGExDetails::FDistances> Distances;

	public:
		using FInitData = std::function<FBox(const TSharedPtr<PCGExData::FPointIO>&, const int32)>;
		using FFacadeRefIterator = std::function<void(const TSharedRef<PCGExData::FFacade>&, const int32)>;
		using FFacadeRefIteratorWithBreak = std::function<void(const TSharedRef<PCGExData::FFacade>&, const int32, bool&)>;
		using FPointIterator = std::function<void(const PCGExData::FPoint&)>;
		using FPointIteratorWithData = std::function<void(const PCGExData::FConstPoint&)>;
		using FTargetQuery = std::function<void(const PCGExOctree::FItem&)>;

		TSharedPtr<PCGExData::FMultiFacadePreloader> TargetsPreloader;
		TSharedPtr<PCGExMatching::FDataMatcher> DataMatcher;

		FTargetsHandler() = default;
		virtual ~FTargetsHandler() = default;

		const TArray<TSharedRef<PCGExData::FFacade>>& GetFacades() const { return TargetFacades; }
		int32 Num() const { return TargetFacades.Num(); }
		bool IsEmpty() const { return TargetFacades.IsEmpty(); }
		int32 GetMaxNumTargets() const { return MaxNumTargets; }

		int32 Init(FPCGExContext* InContext, const FName InPinLabel, FInitData&& InitFn);
		int32 Init(FPCGExContext* InContext, const FName InPinLabel);

		void SetDistances(const FPCGExDistanceDetails& InDetails);
		void SetDistances(const EPCGExDistance Source, const EPCGExDistance Target, const bool bOverlapIsZero);
		TSharedPtr<PCGExDetails::FDistances> GetDistances() const { return Distances; }

		void SetMatchingDetails(FPCGExContext* InContext, const FPCGExMatchingDetails* InDetails);
		bool PopulateIgnoreList(const TSharedPtr<PCGExData::FPointIO>& InDataCandidate, PCGExMatching::FMatchingScope& InMatchingScope, TSet<const UPCGData*>& OutIgnoreList) const;
		bool HandleUnmatchedOutput(const TSharedPtr<PCGExData::FFacade>& InFacade, const bool bForward = true) const;

		void ForEachPreloader(PCGExData::FMultiFacadePreloader::FPreloaderItCallback&& It) const;

		void ForEachTarget(FFacadeRefIterator&& It, const TSet<const UPCGData*>* Exclude = nullptr) const;
		bool ForEachTarget(FFacadeRefIteratorWithBreak&& It, const TSet<const UPCGData*>* Exclude = nullptr) const;
		void ForEachTargetPoint(FPointIterator&& It, const TSet<const UPCGData*>* Exclude = nullptr) const;
		void ForEachTargetPoint(FPointIteratorWithData&& It, const TSet<const UPCGData*>* Exclude = nullptr) const;

		void FindTargetsWithBoundsTest(const FBoxCenterAndExtent& QueryBounds, FTargetQuery&& Func, const TSet<const UPCGData*>* Exclude = nullptr) const;
		void FindElementsWithBoundsTest(const FBoxCenterAndExtent& QueryBounds, FPointIteratorWithData&& Func, const TSet<const UPCGData*>* Exclude = nullptr) const;

		bool FindClosestTarget(const PCGExData::FConstPoint& Probe, const FBoxCenterAndExtent& QueryBounds, PCGExData::FConstPoint& OutResult, double& OutDistSquared, const TSet<const UPCGData*>* Exclude = nullptr) const;
		void FindClosestTarget(const PCGExData::FConstPoint& Probe, PCGExData::FConstPoint& OutResult, double& OutDistSquared, const TSet<const UPCGData*>* Exclude = nullptr) const;
		void FindClosestTarget(const FVector& Probe, PCGExData::FConstPoint& OutResult, double& OutDistSquared, const TSet<const UPCGData*>* Exclude = nullptr) const;

		PCGExData::FConstPoint GetPoint(const int32 IO, const int32 Index) const;
		PCGExData::FConstPoint GetPoint(const PCGExData::FPoint& Point) const;

		double GetDistSquared(const PCGExData::FPoint& SourcePoint, const PCGExData::FPoint& TargetPoint) const;
		FVector GetSourceCenter(const PCGExData::FPoint& OriginPoint, const FVector& OriginLocation, const FVector& ToCenter) const;

		void StartLoading(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const TSharedPtr<PCGExMT::IAsyncHandleGroup>& InParentHandle = nullptr) const;
	};
}
