// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Fixtures/PCGExTestFixtures.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "Data/PCGExData.h"
#include "UObject/Package.h"

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

		// Create a test actor with transient flag to avoid save prompts
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = FName(TEXT("PCGExTestActor"));
		SpawnParams.ObjectFlags = RF_Transient;  // Mark as transient
		TestActor = World->SpawnActor<AActor>(SpawnParams);

		if (TestActor)
		{
			// Create a root component with valid bounds (required for PCG component registration)
			// Use RF_Transient to avoid save prompts
			UBoxComponent* RootBox = NewObject<UBoxComponent>(TestActor, TEXT("RootComponent"), RF_Transient);
			RootBox->SetBoxExtent(FVector(1000.0f)); // 1000 unit box
			RootBox->SetWorldLocation(FVector::ZeroVector);
			TestActor->SetRootComponent(RootBox);
			RootBox->RegisterComponent();

			// Create PCG component on the actor with transient flag
			PCGComponent = NewObject<UPCGComponent>(TestActor, TEXT("PCGExTestComponent"), RF_Transient);
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
			TestGraph->MarkAsGarbage();
			TestGraph = nullptr;
		}

		if (PCGComponent)
		{
			PCGComponent->UnregisterComponent();
			PCGComponent->MarkAsGarbage();
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
			// Create in transient package to avoid save prompts
			TestGraph = NewObject<UPCGGraph>(GetTransientPackage(), NAME_None, RF_Transient);
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
