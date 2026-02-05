// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Fixtures/PCGExTestFixtures.h"

#include "PCGGraph.h"
#include "Data/PCGExData.h"
#include "UObject/Package.h"

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
		TestContext = MakeUnique<FTestContext>();
		TestContext->Initialize();
	}

	void FTestFixture::Teardown()
	{
		if (TestGraph)
		{
			TestGraph->MarkAsGarbage();
			TestGraph = nullptr;
		}

		TestContext.Reset();
	}

	bool FTestFixture::IsValid() const
	{
		return TestContext && TestContext->IsValid();
	}

	UWorld* FTestFixture::GetWorld() const
	{
		return TestContext ? TestContext->GetWorld() : nullptr;
	}

	AActor* FTestFixture::GetActor() const
	{
		return TestContext ? TestContext->GetActor() : nullptr;
	}

	UPCGComponent* FTestFixture::GetPCGComponent() const
	{
		return TestContext ? TestContext->GetPCGComponent() : nullptr;
	}

	FPCGExContext* FTestFixture::GetContext() const
	{
		return TestContext ? TestContext->GetContext() : nullptr;
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

	TSharedPtr<PCGExData::FFacade> FTestFixture::CreateFacade(int32 NumPoints, double Spacing)
	{
		if (!TestContext) { return nullptr; }
		return TestContext->CreateFacade(NumPoints, Spacing);
	}

	TSharedPtr<PCGExData::FFacade> FTestFixture::CreateGridFacade(
		const FVector& Origin,
		const FVector& Spacing,
		int32 CountX,
		int32 CountY,
		int32 CountZ)
	{
		if (!TestContext) { return nullptr; }
		return TestContext->CreateGridFacade(Origin, Spacing, CountX, CountY, CountZ);
	}

	TSharedPtr<PCGExData::FFacade> FTestFixture::CreateRandomFacade(
		const FBox& Bounds,
		int32 NumPoints,
		uint32 Seed)
	{
		if (!TestContext) { return nullptr; }
		return TestContext->CreateRandomFacade(Bounds, NumPoints, Seed);
	}
}
