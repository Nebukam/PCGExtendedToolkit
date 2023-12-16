// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGGraph.h"

#include "IPCGExDebug.generated.h"

UINTERFACE(MinimalAPI)
class UPCGExDebug : public UInterface
{
	GENERATED_BODY()
};

class PCGEXTENDEDTOOLKIT_API IPCGExDebug
{
	GENERATED_BODY()

public:
	virtual bool IsDebugEnabled() const = 0;
};

/// 

UINTERFACE(MinimalAPI)
class UPCGExDebugManager : public UInterface
{
	GENERATED_BODY()
};

class PCGEXTENDEDTOOLKIT_API IPCGExDebugManager
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual void PingFrom(const FPCGContext* Context, IPCGExDebug* DebugNode) = 0;
	virtual void ResetPing(const FPCGContext* Context) = 0;
#endif
};


namespace PCGExDebug
{
	static bool NotifyExecute(const FPCGContext* Context)
	{
		const IPCGExDebug* Originator = Cast<IPCGExDebug>(Context->Node->GetSettingsInterface());
		if (!Originator) { return false; }

		IPCGExDebug* MutableOriginator = const_cast<IPCGExDebug*>(Originator);
		int32 ManagerCount = 0;
		const TArray<UPCGNode*>& Nodes = Context->SourceComponent->GetGraph()->GetNodes();
		for (const UPCGNode* Node : Nodes)
		{
			const IPCGExDebugManager* DebugNodeManager = Cast<IPCGExDebugManager>(Node->GetSettingsInterface());
			if (!DebugNodeManager) { continue; }
			ManagerCount++;
			const_cast<IPCGExDebugManager*>(DebugNodeManager)->PingFrom(Context, MutableOriginator);
		}

		if (ManagerCount > 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("There are multiple PCGEx DebugManager in your graph -- this can cause unexpected behaviors."))
		}
		else if (ManagerCount == 0)
		{
			UE_LOG(LogTemp, Error, TEXT("There is no PCGEx DebugManager in your graph -- add one to make sure PCGEx Debug node can work."))
		}

		return ManagerCount > 0;
	}

	static int32 GetActiveDebugNodeCount(const FPCGContext* Context)
	{
		const TArray<UPCGNode*>& Nodes = Context->SourceComponent->GetGraph()->GetNodes();
		int32 DebugNodeCount = 0;

		for (const UPCGNode* Node : Nodes)
		{
			const IPCGExDebug* DebugNode = Cast<IPCGExDebug>(Node->GetSettingsInterface());
			if (!DebugNode || !DebugNode->IsDebugEnabled()) { continue; }
			DebugNodeCount++;
		}

		return DebugNodeCount;
	}
}
