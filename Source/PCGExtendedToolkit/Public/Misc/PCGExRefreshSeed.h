﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"


#include "PCGExRefreshSeed.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="metadata/refresh-seed"))
class UPCGExRefreshSeedSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(RefreshSeed, "Refresh Seed", "Refresh point seed based on position.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Generic; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->ColorMiscWrite); }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Base seed.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	int32 Base = 0;
};

struct FPCGExRefreshSeedContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExRefreshSeedElement;
};

class FPCGExRefreshSeedElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(RefreshSeed)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

class FPCGExRefreshSeedTask final : public PCGExMT::FPCGExIndexedTask
{
public:
	explicit FPCGExRefreshSeedTask(const int32 InPointIndex,
	                               const TSharedPtr<PCGExData::FPointIO>& InPointIO)
		: FPCGExIndexedTask(InPointIndex),
		  PointIO(InPointIO)
	{
	}

	const TSharedPtr<PCGExData::FPointIO> PointIO;

	virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
};
