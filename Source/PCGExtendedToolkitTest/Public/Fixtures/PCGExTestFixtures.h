// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGComponent.h"
#include "Fixtures/PCGExTestContext.h"

class UWorld;
class AActor;
class UPCGComponent;
class UPCGGraph;

namespace PCGExData
{
	class FFacade;
	class FPointIO;
}

namespace PCGExTest
{
	/**
	 * Test Fixture for PCGEx Tests
	 *
	 * Manages test world, actor, and PCG component lifecycle.
	 * Use this fixture for integration and functional tests that need
	 * a running world context.
	 *
	 * NOTE: For new tests, prefer using FTestContext or FScopedTestContext
	 * directly, which provides more functionality.
	 *
	 * Example Usage:
	 * @code
	 * TUniquePtr<FTestFixture> Fixture = MakeUnique<FTestFixture>();
	 * Fixture->Setup();
	 * // ... run tests ...
	 * Fixture->Teardown();
	 * @endcode
	 */
	class PCGEXTENDEDTOOLKITTEST_API FTestFixture
	{
	public:
		FTestFixture();
		~FTestFixture();

		/** Initialize test world, actor, and PCG component */
		void Setup();

		/** Cleanup all resources */
		void Teardown();

		/** Check if fixture is valid and ready for use */
		bool IsValid() const;

		/** Get the test world - valid after Setup() */
		UWorld* GetWorld() const;

		/** Get the test actor - valid after Setup() */
		AActor* GetActor() const;

		/** Get the PCG component - valid after Setup() */
		UPCGComponent* GetPCGComponent() const;

		/** Get the underlying FPCGExContext - valid after Setup() */
		FPCGExContext* GetContext() const;

		/** Get or create a PCG graph for testing */
		UPCGGraph* GetOrCreateGraph();

		/**
		 * Create a test facade with specified number of points
		 * Points are created with sequential positions along X axis
		 * @param NumPoints Number of points to create
		 * @param Spacing Distance between points (default 100 units)
		 */
		TSharedPtr<PCGExData::FFacade> CreateFacade(int32 NumPoints, double Spacing = 100.0);

		/**
		 * Create a facade with grid-positioned points
		 */
		TSharedPtr<PCGExData::FFacade> CreateGridFacade(
			const FVector& Origin,
			const FVector& Spacing,
			int32 CountX,
			int32 CountY,
			int32 CountZ = 1);

		/**
		 * Create a facade with randomly positioned points
		 */
		TSharedPtr<PCGExData::FFacade> CreateRandomFacade(
			const FBox& Bounds,
			int32 NumPoints,
			uint32 Seed = 12345u);

	private:
		TUniquePtr<FTestContext> TestContext;
		UPCGGraph* TestGraph = nullptr;

		// Prevent copying
		FTestFixture(const FTestFixture&) = delete;
		FTestFixture& operator=(const FTestFixture&) = delete;
	};
}
