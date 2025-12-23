// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExFiltersSubSystem.h"

#include "Filters/Points/PCGExConstantFilter.h"

#if WITH_EDITOR
#include "Editor.h"
#include "ObjectTools.h"
#else
#include "Engine/Engine.h"
#include "Engine/World.h"
#endif

UPCGExFiltersSubSystem::UPCGExFiltersSubSystem()
	: Super()
{
}

void UPCGExFiltersSubSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	ConstantFilterFactory_TRUE = NewObject<UPCGExConstantFilterFactory>(this);
	ConstantFilterFactory_TRUE->Config.Value = true;

	ConstantFilterFactory_FALSE = NewObject<UPCGExConstantFilterFactory>(this);
	ConstantFilterFactory_FALSE->Config.Value = false;

	Super::Initialize(Collection);
}

void UPCGExFiltersSubSystem::Deinitialize()
{
	Super::Deinitialize();
}

UPCGExFiltersSubSystem* UPCGExFiltersSubSystem::GetSubsystemForCurrentWorld()
{
	UWorld* World = nullptr;

#if WITH_EDITOR
	if (GEditor)
	{
		if (GEditor->PlayWorld)
		{
			World = GEditor->PlayWorld;
		}
		else
		{
			World = GEditor->GetEditorWorldContext().World();
		}
	}
	else
#endif
		if (GEngine)
		{
			World = GEngine->GetCurrentPlayWorld();
		}

	return GetInstance(World);
}

UPCGExFiltersSubSystem* UPCGExFiltersSubSystem::GetInstance(UWorld* World)
{
	if (World)
	{
		return World->GetSubsystem<UPCGExFiltersSubSystem>();
	}
	return nullptr;
}

TSharedPtr<PCGExPointFilter::IFilter> UPCGExFiltersSubSystem::GetConstantFilter(const bool bValue) const
{
	if (bValue) { return ConstantFilterFactory_TRUE->CreateFilter(); }
	return ConstantFilterFactory_FALSE->CreateFilter();
}
