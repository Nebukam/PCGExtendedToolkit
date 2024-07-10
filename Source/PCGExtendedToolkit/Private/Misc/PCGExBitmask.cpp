// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExBitmask.h"

#include "PCGGraph.h"
#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"
#define PCGEX_NAMESPACE Bitmask

#pragma region UPCGSettings interface

UPCGExGlobalBitmaskManager* UPCGExGlobalBitmaskManager::Get()
{
	static UPCGExGlobalBitmaskManager* Instance = nullptr;
	if (!Instance)
	{
		Instance = NewObject<UPCGExGlobalBitmaskManager>();
		Instance->AddToRoot(); // Prevent garbage collection
	}
	return Instance;
}

UPCGParamData* UPCGExGlobalBitmaskManager::GetOrCreate(const int64 Bitmask)
{
	UPCGParamData** Data = Get()->SharedInstances.Find(Bitmask);
	if (Data) { return *Data; }

	UPCGParamData* NewBitmask = NewObject<UPCGParamData>();
	NewBitmask->Metadata->CreateAttribute<int64>(FName("Bitmask"), Bitmask, false, true);
	NewBitmask->Metadata->AddEntry();
	Get()->SharedInstances.Add(Bitmask, NewBitmask);
	return NewBitmask;
}

TArray<FPCGPinProperties> UPCGExBitmaskSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExBitmaskSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_PARAM(FName("Bitmask"), TEXT("Bitmask."), Required, {})
	return PinProperties;
}

FPCGElementPtr UPCGExBitmaskSettings::CreateElement() const { return MakeShared<FPCGExBitmaskElement>(); }

#pragma endregion

FPCGContext* FPCGExBitmaskElement::Initialize(
	const FPCGDataCollection& InputData,
	const TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGContext* Context = new FPCGContext();

	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;

	return Context;
}

bool FPCGExBitmaskElement::ExecuteInternal(FPCGContext* Context) const
{
	PCGEX_SETTINGS(Bitmask)

	UPCGParamData* BitmaskData;
	const int64 Bitmask = Settings->Bitmask.Get();

	if (Settings->bGlobalInstance)
	{
		BitmaskData = UPCGExGlobalBitmaskManager::GetOrCreate(Bitmask);
	}
	else
	{
		BitmaskData = NewObject<UPCGParamData>();
		BitmaskData->Metadata->CreateAttribute<int64>(FName("Bitmask"), Bitmask, false, true);
		BitmaskData->Metadata->AddEntry();
	}

	FPCGTaggedData& OutData = Context->OutputData.TaggedData.Emplace_GetRef();
	OutData.Pin = FName("Bitmask");
	OutData.Data = BitmaskData;

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
