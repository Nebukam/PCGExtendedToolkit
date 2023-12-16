// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "IPCGExDebug.h"
#include "Elements/PCGPointProcessingElementBase.h"

#include "PCGEx.h"

#include "PCGExDebugManager.generated.h"

/**
 * A Base node to process a set of point using GraphParams.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExDebugSettings : public UPCGSettings, public IPCGExDebugManager
{
	GENERATED_BODY()

public:
	UPCGExDebugSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	bool bCacheResult = false;
	PCGEX_NODE_INFOS(DebugSettings, "Debug Manager", "TOOLTIP_TEXT");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Debug; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorDebug; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

	virtual bool ShouldDrawNodeCompact() const override { return true; }

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin IPCGExDebugManager interface
public:
	int32 DebugNodeCount = 0;
	int32 CurrentPingCount = 0;

#if WITH_EDITOR
	virtual void PingFrom(const FPCGContext* Context, IPCGExDebug* DebugNode) override
	{
		const int32 CurrentDebugNodeCount = PCGExDebug::GetActiveDebugNodeCount(Context);
		if (CurrentDebugNodeCount != DebugNodeCount)
		{
			// If we reach this, it means the debug manager hasn't executed yet
			DebugNodeCount = CurrentDebugNodeCount;
			CurrentPingCount = 0;
		}

		if (CurrentPingCount == 0)
		{
			FlushPersistentDebugLines(PCGEx::GetWorld(Context));
			FlushDebugStrings(PCGEx::GetWorld(Context));
		}

		if (++CurrentPingCount >= DebugNodeCount) { CurrentPingCount = 0; }
	}

	virtual void ResetPing(const FPCGContext* Context) override
	{
		DebugNodeCount = 0;
		CurrentPingCount = 0;
		FlushPersistentDebugLines(PCGEx::GetWorld(Context));
		FlushDebugStrings(PCGEx::GetWorld(Context));
	}
#endif

	//~End IPCGExDebugManager interface
};

struct PCGEXTENDEDTOOLKIT_API FPCGExDebugContext : public FPCGContext
{
	friend class FPCGExDebugElement;

public:
};

class PCGEXTENDEDTOOLKIT_API FPCGExDebugElement : public FPCGPointProcessingElementBase
{
public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
