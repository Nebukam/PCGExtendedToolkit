// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExFilterFactoryProvider.h"
#include "Data/PCGExPointFilter.h"


#include "Paths/PCGExPaths.h"
#include "PCGExPolyPathFilterFactory.generated.h"

namespace PCGExPathInclusion
{
	class FHandler;
}

UENUM()
enum class EPCGExSplineSamplingIncludeMode : uint8
{
	All            = 0 UMETA(DisplayName = "All", ToolTip="Sample all inputs"),
	ClosedLoopOnly = 1 UMETA(DisplayName = "Closed loops only", ToolTip="Sample only closed loops"),
	OpenSplineOnly = 2 UMETA(DisplayName = "Open lines only", ToolTip="Sample only open lines"),
};

UENUM()
enum class EPCGExSplineCheckType : uint8
{
	IsInside       = 0 UMETA(DisplayName = "Is Inside", Tooltip="..."),
	IsInsideOrOn   = 1 UMETA(DisplayName = "Is Inside or On", Tooltip="..."),
	IsInsideAndOn  = 2 UMETA(DisplayName = "Is Inside and On", Tooltip="..."),
	IsOutside      = 3 UMETA(DisplayName = "Is Outside", Tooltip="..."),
	IsOutsideOrOn  = 4 UMETA(DisplayName = "Is Outside or On", Tooltip="..."),
	IsOutsideAndOn = 5 UMETA(DisplayName = "Is Outside and On", Tooltip="..."),
	IsOn           = 6 UMETA(DisplayName = "Is On", Tooltip="..."),
	IsNotOn        = 7 UMETA(DisplayName = "Is not On", Tooltip="..."),
};

UENUM()
enum class EPCGExSplineFilterPick : uint8
{
	Closest = 0 UMETA(DisplayName = "Closest", Tooltip="..."),
	All     = 1 UMETA(DisplayName = "All", Tooltip="...")
};

/**
 * 
 */
UCLASS(Abstract, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class UPCGExPolyPathFilterFactory : public UPCGExFilterFactoryData
{
	GENERATED_BODY()
	friend PCGExPathInclusion::FHandler;

public:
	virtual bool SupportsProxyEvaluation() const override { return true; } // TODO Change this one we support per-point tolerance from attribute

	TArray<TSharedPtr<PCGExPaths::IPath>> PolyPaths;
	TSharedPtr<PCGEx::FIndexedItemOctree> Octree;

	virtual bool Init(FPCGExContext* InContext) override;
	virtual bool WantsPreparation(FPCGExContext* InContext) override;
	virtual PCGExFactories::EPreparationResult Prepare(FPCGExContext* InContext, const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;

	TSharedPtr<PCGExPathInclusion::FHandler> CreateHandler() const;

	virtual void BeginDestroy() override;

protected:
	virtual FName GetInputLabel() const { return PCGExPaths::SourcePathsLabel; }

	virtual void InitConfig_Internal()
	{
	}

	double LocalFidelity = 50;
	double LocalExpansion = 0;
	double LocalExpansionZ = -1;
	FPCGExGeo2DProjectionDetails LocalProjection;
	EPCGExSplineSamplingIncludeMode LocalSampleInputs = EPCGExSplineSamplingIncludeMode::All;
	EPCGExWindingMutation WindingMutation = EPCGExWindingMutation::Unchanged;
	bool bScaleTolerance = false;

	TArray<FPCGTaggedData> TempTargets;
	TArray<TSharedPtr<PCGExPaths::IPath>> TempPolyPaths;
};

namespace PCGExPathInclusion
{
	enum EFlags : uint8
	{
		None    = 0,
		Inside  = 1 << 1,
		Outside = 1 << 2,
		On      = 1 << 3,
	};

	enum ESplineMatch : uint8
	{
		Any = 0,
		All,
		Skip
	};

#if WITH_EDITOR
	static FString ToString(const EPCGExSplineCheckType Check)
	{
		switch (Check)
		{
		default:
		case EPCGExSplineCheckType::IsInside: return TEXT("Is Inside");
		case EPCGExSplineCheckType::IsInsideOrOn: return TEXT("Is Inside or On");
		case EPCGExSplineCheckType::IsInsideAndOn: return TEXT("Is Outside and On");
		case EPCGExSplineCheckType::IsOutside: return TEXT("Is Outside");
		case EPCGExSplineCheckType::IsOutsideOrOn: return TEXT("Is Outside or On");
		case EPCGExSplineCheckType::IsOutsideAndOn: return TEXT("Is Outside and On");
		case EPCGExSplineCheckType::IsOn: return TEXT("Is On");
		case EPCGExSplineCheckType::IsNotOn: return TEXT("Is not On");
		}
	}
#endif

	class FHandler : public TSharedFromThis<FHandler>
	{
		const TArray<TSharedPtr<PCGExPaths::IPath>>* Paths;
		TSharedPtr<PCGEx::FIndexedItemOctree> Octree;
		EPCGExSplineCheckType Check = EPCGExSplineCheckType::IsInside;

		bool bFastCheck = false;
		bool bDistanceCheckOnly = false;

		EFlags GoodFlags = None;
		EFlags BadFlags = None;
		ESplineMatch FlagScope = Any;

	public:
		double Tolerance = MAX_dbl;
		double ToleranceSquared = MAX_dbl;
		bool bScaleTolerance = false;
		FVector ToleranceScaleFactor = FVector(1, 1, 1);

		explicit FHandler(const UPCGExPolyPathFilterFactory* InFactory);

		void Init(const EPCGExSplineCheckType InCheckType);

		FORCEINLINE bool TestFlags(const EFlags InFlags) const
		{
			bool bPass = (InFlags & BadFlags) == 0; // None of the bad flags
			if (bPass && FlagScope != Skip)
			{
				bPass =
					FlagScope == Any ?
						EnumHasAnyFlags(InFlags, GoodFlags) : // Any of the good flags
						EnumHasAllFlags(InFlags, GoodFlags);  // All of the good flags
			}
			return bPass;
		}

		EFlags GetInclusionFlags(const FVector& WorldPosition, int32& InclusionCount, const bool bClosestOnly) const;
	};
}
