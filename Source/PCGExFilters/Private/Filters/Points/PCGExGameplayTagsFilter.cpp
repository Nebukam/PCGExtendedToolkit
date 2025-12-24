// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Filters/Points/PCGExGameplayTagsFilter.h"

#include "PCGExLog.h"
#include "GameFramework/Actor.h"
#include "PropertyPathHelpers.h"
#include "Containers/PCGExManagedObjects.h"
#include "Data/PCGExData.h"

#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

TSharedPtr<PCGExPointFilter::IFilter> UPCGExGameplayTagsFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FGameplayTagsFilter>(this);
}

bool UPCGExGameplayTagsFilterFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	PCGEX_VALIDATE_NAME_CONSUMABLE_C(InContext, Config.ActorReference)

	return true;
}

bool PCGExPointFilter::FGameplayTagsFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
{
	if (!IFilter::Init(InContext, InPointDataFacade)) { return false; }

	PropertyPath = FCachedPropertyPath(TypedFilterFactory->Config.PropertyPath);
	if (!PropertyPath.IsValid())
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Invalid PropertyPath."));
		return false;
	}

	PathSegments.Reserve(PropertyPath.GetNumSegments());
	for (int i = 0; i < PropertyPath.GetNumSegments(); i++) { PathSegments.Add(PropertyPath.GetSegment(i).Name.ToString()); }

	ActorReferences = PointDataFacade->GetBroadcaster<FSoftObjectPath>(TypedFilterFactory->Config.ActorReference, true);
	if (!ActorReferences)
	{
		PCGEX_LOG_INVALID_ATTR_HANDLED_C(InContext, Actor Reference, TypedFilterFactory->Config.ActorReference)
		return false;
	}

	return true;
}

bool PCGExPointFilter::FGameplayTagsFilter::Test(const int32 PointIndex) const
{
	AActor* TargetActor = TSoftObjectPtr<AActor>(ActorReferences->Read(PointIndex)).Get();
	if (!TargetActor) { return TypedFilterFactory->Config.bFallbackMissingActor; }

	const FCachedPropertyPath Path = FCachedPropertyPath(PathSegments);
	FGameplayTagContainer TagContainer;
	FProperty* Property = nullptr;

	if (!PropertyPathHelpers::GetPropertyValue(TargetActor, Path, TagContainer, Property))
	{
		if (!TypedFilterFactory->Config.bQuietMissingPropertyWarning)
		{
			UE_LOG(LogPCGEx, Warning, TEXT("GameplayTags filter could not resolve target property : \"%s\"."), *TypedFilterFactory->Config.PropertyPath);
		}
		return TypedFilterFactory->Config.bFallbackPropertyPath;
	}
	return TypedFilterFactory->Config.TagQuery.Matches(TagContainer);
}

PCGEX_CREATE_FILTER_FACTORY(GameplayTags)

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
