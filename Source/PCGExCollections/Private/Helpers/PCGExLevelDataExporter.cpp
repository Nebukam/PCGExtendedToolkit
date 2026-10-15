// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Helpers/PCGExLevelDataExporter.h"

#include "PCGExAssemblyRoot.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "LevelUtils.h"

#if WITH_EDITOR
#include "Editor/GroupActor.h"
#endif

#pragma region FPCGExLevelExportSource

FPCGExLevelExportSource FPCGExLevelExportSource::FromWorld(UWorld* InWorld)
{
	FPCGExLevelExportSource Source;
	Source.World = InWorld;
	if (InWorld && InWorld->PersistentLevel)
	{
		// A level streamed into another world (a Level Instance edited in place) carries the instance transform; export in level space.
		if (InWorld->PersistentLevel->bAlreadyMovedActors)
		{
			if (const ULevelStreaming* StreamingLevel = FLevelUtils::FindStreamingLevel(InWorld->PersistentLevel))
			{
				Source.Frame = StreamingLevel->LevelTransform;
			}
		}

		Source.Actors.Reserve(InWorld->PersistentLevel->Actors.Num());
		for (AActor* Actor : InWorld->PersistentLevel->Actors)
		{
			if (Actor)
			{
				Source.Actors.Add(Actor);
			}
		}
	}
	return Source;
}

FPCGExLevelExportSource FPCGExLevelExportSource::FromActorSubtree(AActor* InRoot)
{
	FPCGExLevelExportSource Source;
	if (!InRoot)
	{
		return Source;
	}

	Source.World = InRoot->GetWorld();
	Source.Root = InRoot;

	const IPCGExAssemblyRoot* AssemblyRoot = Cast<IPCGExAssemblyRoot>(InRoot);
	Source.Frame = AssemblyRoot ? AssemblyRoot->GetAssemblyFrame() : InRoot->GetActorTransform();

	// Depth-first over attach children and group members. A pruned node takes its whole subtree with it; a node
	// the root keeps is content even when it is a bare grouping pivot -- the content filter decides what becomes a point.
	TArray<AActor*> Stack;
	Stack.Add(InRoot);

	// Group membership can reach an actor that attachment reaches too.
	TSet<const AActor*> Visited;
	Visited.Add(InRoot);

	TArray<AActor*> Children;
#if WITH_EDITOR
	TArray<AActor*> GroupMembers;
#endif
	while (!Stack.IsEmpty())
	{
		const AActor* Node = Stack.Pop(EAllowShrinking::No);

		Children.Reset();
		Node->GetAttachedActors(Children, /*bResetArray=*/true, /*bRecursivelyIncludeAttachedActors=*/false);

#if WITH_EDITOR
		// Group members and subgroups are not attached to their group; their world transforms already carry its placement.
		if (const AGroupActor* Group = Cast<AGroupActor>(Node))
		{
			Group->GetAllChildren(GroupMembers, /*bRecurse=*/false);
			Children.Append(GroupMembers);
		}
#endif

		for (AActor* Child : Children)
		{
			if (!Child || (AssemblyRoot && !AssemblyRoot->IsAssemblyContent(Child)))
			{
				continue;
			}

			bool bAlreadyVisited = false;
			Visited.Add(Child, &bAlreadyVisited);
			if (bAlreadyVisited)
			{
				continue;
			}

			Source.Actors.Add(Child);
			Stack.Add(Child);
		}
	}

	return Source;
}

#pragma endregion
