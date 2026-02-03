// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGComponent.h"

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

		/** Get the test world - valid after Setup() */
		UWorld* GetWorld() const { return World; }

		/** Get the test actor - valid after Setup() */
		AActor* GetActor() const { return TestActor; }

		/** Get the PCG component - valid after Setup() */
		UPCGComponent* GetPCGComponent() const { return PCGComponent; }

		/** Get or create a PCG graph for testing */
		UPCGGraph* GetOrCreateGraph();

		/**
		 * Create a test facade with specified number of points
		 * Points are created at origin with default transforms
		 */
		TSharedPtr<PCGExData::FFacade> CreateFacade(int32 NumPoints);

	private:
		UWorld* World = nullptr;
		AActor* TestActor = nullptr;
		UPCGComponent* PCGComponent = nullptr;
		UPCGGraph* TestGraph = nullptr;

		// Prevent copying
		FTestFixture(const FTestFixture&) = delete;
		FTestFixture& operator=(const FTestFixture&) = delete;
	};
}
