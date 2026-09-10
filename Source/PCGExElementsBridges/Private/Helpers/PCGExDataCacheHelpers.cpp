// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Helpers/PCGExDataCacheHelpers.h"

#include "PCGContext.h"
#include "PCGModule.h"
#include "PCGGraphExecutionStateInterface.h"
#include "PCGWorldActor.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

#include "Core/PCGExContext.h"

#define LOCTEXT_NAMESPACE "PCGExDataCacheHelpers"

namespace PCGExDataCache
{
	void ResolveTargetActors(FPCGExContext* InContext, const EPCGExDataCacheTarget InTarget, const FPCGAttributePropertyInputSelector& InActorReferenceAttribute, TArray<AActor*>& OutActors)
	{
		check(IsInGameThread());
		check(InContext);

		// Pin data wins over the enum, like the engine's Add Component target pin.
		const TArray<FPCGTaggedData> TargetInputs = InContext->InputData.GetInputsByPin(TargetActorPinLabel);
		if (!TargetInputs.IsEmpty())
		{
			for (const FPCGTaggedData& TaggedData : TargetInputs)
			{
				if (!TaggedData.Data) { continue; }

				const FPCGAttributePropertyInputSelector Selector = InActorReferenceAttribute.CopyAndFixLast(TaggedData.Data);
				const TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(TaggedData.Data, Selector);
				const TUniquePtr<const IPCGAttributeAccessorKeys> Keys = PCGAttributeAccessorHelpers::CreateConstKeys(TaggedData.Data, Selector);

				if (!Accessor.IsValid() || !Keys.IsValid())
				{
					PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(LOCTEXT("MissingActorReferenceAttribute", "Target actor data does not have the attribute '{0}'."), FText::FromName(Selector.GetName())));
					continue;
				}

				const int32 NumKeys = Keys->GetNum();
				for (int32 i = 0; i < NumKeys; i++)
				{
					FSoftObjectPath Path;
					if (!Accessor->Get<FSoftObjectPath>(Path, i, *Keys, EPCGAttributeAccessorFlags::AllowBroadcastAndConstructible))
					{
						PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(LOCTEXT("InvalidActorReferenceAttribute", "Target actor attribute '{0}' is not an object path."), FText::FromName(Selector.GetName())));
						break;
					}

					UObject* Object = Path.ResolveObject();
					AActor* Actor = Cast<AActor>(Object);
					if (!Actor)
					{
						if (const UActorComponent* Component = Cast<UActorComponent>(Object)) { Actor = Component->GetOwner(); }
					}

					if (IsValid(Actor)) { OutActors.AddUnique(Actor); }
				}
			}

			return;
		}

		IPCGGraphExecutionSource* Source = InContext->ExecutionSource.Get();
		if (!Source) { return; }

		const IPCGGraphExecutionState& State = Source->GetExecutionState();
		AActor* Actor = nullptr;

		switch (InTarget)
		{
		case EPCGExDataCacheTarget::ExecutingActor:
			Actor = State.GetTypedTarget<AActor>();
			break;
		case EPCGExDataCacheTarget::OriginalActor:
			if (const IPCGGraphExecutionSource* Original = State.GetOriginalSource())
			{
				Actor = Original->GetExecutionState().GetTypedTarget<AActor>();
			}
			// A source with no original (non-component execution) is its own original.
			if (!Actor) { Actor = State.GetTypedTarget<AActor>(); }
			break;
		case EPCGExDataCacheTarget::WorldActor:
			Actor = PCGHelpers::GetPCGWorldActor(State.GetWorld());
			break;
		default:
			checkNoEntry();
			break;
		}

		if (IsValid(Actor)) { OutActors.Add(Actor); }
	}
}

#undef LOCTEXT_NAMESPACE
