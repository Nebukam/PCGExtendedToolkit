// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExPointsProcessor.h"
#include "PCGExSortPoints.h"

#include "PCGExModularSortPoints.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="misc/sort-points"))
class UPCGExModularSortPointsSettings : public UPCGExSortPointsBaseSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ModularSortPoints, "Sort Points", "Sort the source points according to specific rules.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Generic; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_OPTIN_NAME(MiscWrite); }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

public:
	virtual bool GetSortingRules(FPCGExContext* InContext, TArray<FPCGExSortRuleConfig>& OutRules) const override;
};
