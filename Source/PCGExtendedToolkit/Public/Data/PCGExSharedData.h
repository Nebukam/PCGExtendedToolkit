// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExMacros.h"

struct FPCGExContext;
class UPCGData;
class UPCGComponent;

namespace PCGExData
{
	class FSharedData : public TSharedFromThis<FSharedData>
	{
	protected:
		UPCGComponent* SourceComponent = nullptr;
		TStrongObjectPtr<UPCGData> SourceData = nullptr;

	public:
		explicit FSharedData(UPCGData* InSourceData);
		~FSharedData();
	};

	class FSharedPCGComponent : public TSharedFromThis<FSharedPCGComponent>
	{
		FRWLock ManagementLock;
		TArray<TSharedPtr<FSharedData>> TrackedData;

	protected:
		bool bReleased = false;
		uint32 UID = 0;
		UPCGComponent* SourceComponent = nullptr;

	public:
		explicit FSharedPCGComponent(UPCGComponent* InSourceComponent);
		~FSharedPCGComponent();

		void Release();

		uint32 GetUID() const { return UID; }
	};
}
