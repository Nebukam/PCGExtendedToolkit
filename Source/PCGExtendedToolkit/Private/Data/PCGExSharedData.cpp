// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExSharedData.h"

#include "PCGComponent.h"
#include "PCGData.h"
#include "UPCGExSubSystem.h"

namespace PCGExData
{
	FSharedPCGComponent::FSharedPCGComponent(UPCGComponent* InSourceComponent)
	{
		check(InSourceComponent)
		SourceComponent = InSourceComponent;
		UID = SourceComponent->GetUniqueID();
	}

	FSharedPCGComponent::~FSharedPCGComponent()
	{
	}

	void FSharedPCGComponent::Release()
	{
		FWriteScopeLock WriteScopeLock(ManagementLock);
		
		if (bReleased) { return; }

		bReleased = true;
		SourceComponent->GetWorld()->GetSubsystem<UPCGExSubSystem>()->ReleaseSharedPCGComponent(SharedThis(this));

		TrackedData.Empty();
		
	}

	FSharedData::FSharedData(UPCGData* InSourceData)
	{
		check(InSourceData)
		SourceData = TStrongObjectPtr(InSourceData);
	}

	FSharedData::~FSharedData()
	{
		SourceData.Reset();
	}
}
