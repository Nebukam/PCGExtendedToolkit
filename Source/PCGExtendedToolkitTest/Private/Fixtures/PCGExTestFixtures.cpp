// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Fixtures/PCGExTestFixtures.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "Data/PCGExData.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

namespace PCGExTest
{
	FTestFixture::FTestFixture()
	{
	}

	FTestFixture::~FTestFixture()
	{
		Teardown();
	}

	void FTestFixture::Setup()
	{
		// Use the editor world if available, otherwise create a minimal world
#if WITH_EDITOR
		if (GEditor && GEditor->GetEditorWorldContext().World())
		{
			World = GEditor->GetEditorWorldContext().World();
		}
		else
#endif
		{
			// Create a minimal test world
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("PCGExTestWorld"));
		}

		if (!World)
		{
			return;
		}

		// Create a test actor
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = FName(TEXT("PCGExTestActor"));
		TestActor = World->SpawnActor<AActor>(SpawnParams);

		if (TestActor)
		{
			// Create a root component with valid bounds (required for PCG component registration)
			UBoxComponent* RootBox = NewObject<UBoxComponent>(TestActor, TEXT("RootComponent"));
			RootBox->SetBoxExtent(FVector(1000.0f)); // 1000 unit box
			RootBox->SetWorldLocation(FVector::ZeroVector);
			TestActor->SetRootComponent(RootBox);
			RootBox->RegisterComponent();

			// Create PCG component on the actor
			PCGComponent = NewObject<UPCGComponent>(TestActor, TEXT("PCGExTestComponent"));
			if (PCGComponent)
			{
				PCGComponent->RegisterComponent();
			}
		}
	}

	void FTestFixture::Teardown()
	{
		if (TestGraph)
		{
			TestGraph = nullptr;
		}

		if (PCGComponent)
		{
			PCGComponent->UnregisterComponent();
			PCGComponent = nullptr;
		}

		if (TestActor)
		{
			TestActor->Destroy();
			TestActor = nullptr;
		}

		// Only destroy worlds we created
#if WITH_EDITOR
		if (World && (!GEditor || World != GEditor->GetEditorWorldContext().World()))
#else
		if (World)
#endif
		{
			World->DestroyWorld(false);
		}

		World = nullptr;
	}

	UPCGGraph* FTestFixture::GetOrCreateGraph()
	{
		if (!TestGraph)
		{
			TestGraph = NewObject<UPCGGraph>();
		}
		return TestGraph;
	}

	TSharedPtr<PCGExData::FFacade> FTestFixture::CreateFacade(int32 NumPoints)
	{
		// Note: Creating a facade requires a FPointIO which needs a proper context
		// For unit tests that don't need full facades, use the point data helpers instead
		// This is a placeholder for integration tests that set up full contexts
		return nullptr;
	}
}
