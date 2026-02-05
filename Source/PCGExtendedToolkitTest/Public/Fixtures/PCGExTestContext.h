// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExContext.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"

class UWorld;
class AActor;
class UPCGComponent;
class UPCGGraph;
class UPCGBasePointData;

namespace PCGExTest
{
	/**
	 * Test Context for PCGEx Tests
	 *
	 * Provides a fully initialized FPCGExContext suitable for testing PCGEx
	 * components that require a valid context, including:
	 * - FPointIO creation and initialization
	 * - FFacade creation with proper buffer support
	 * - Filter testing with real point data
	 *
	 * Lifecycle:
	 * @code
	 * // Create and initialize
	 * TUniquePtr<FTestContext> TestCtx = MakeUnique<FTestContext>();
	 * if (!TestCtx->Initialize()) { return false; }
	 *
	 * // Create test data
	 * TSharedPtr<PCGExData::FFacade> Facade = TestCtx->CreateFacade(100);
	 *
	 * // ... run tests ...
	 *
	 * // Cleanup is automatic on destruction
	 * @endcode
	 */
	class PCGEXTENDEDTOOLKITTEST_API FTestContext
	{
	public:
		FTestContext();
		~FTestContext();

		/**
		 * Initialize the test context
		 * Creates world, actor, PCG component, and context
		 * @return true if initialization succeeded
		 */
		bool Initialize();

		/**
		 * Cleanup all resources
		 * Called automatically on destruction, but can be called early
		 */
		void Cleanup();

		/** Check if context is valid and ready for use */
		bool IsValid() const;

		/** Get the PCGEx context - valid after Initialize() */
		FPCGExContext* GetContext() const { return Context; }

		/** Get the test world - valid after Initialize() */
		UWorld* GetWorld() const { return World; }

		/** Get the test actor - valid after Initialize() */
		AActor* GetActor() const { return TestActor; }

		/** Get the PCG component - valid after Initialize() */
		UPCGComponent* GetPCGComponent() const { return PCGComponent; }

		/**
		 * Create a new FPointIO with no input data
		 * Suitable for creating output-only point sets
		 * @param OutputPin Optional output pin name
		 * @param Index Optional index for the PointIO
		 */
		TSharedPtr<PCGExData::FPointIO> CreatePointIO(
			FName OutputPin = NAME_None,
			int32 Index = -1);

		/**
		 * Create a new FPointIO wrapping existing point data
		 * @param InData The point data to wrap
		 * @param OutputPin Optional output pin name
		 * @param Index Optional index for the PointIO
		 */
		TSharedPtr<PCGExData::FPointIO> CreatePointIO(
			const UPCGBasePointData* InData,
			FName OutputPin = NAME_None,
			int32 Index = -1);

		/**
		 * Create a facade with the specified number of points
		 * Points are initialized with sequential positions along X axis
		 * @param NumPoints Number of points to create
		 * @param Spacing Distance between points (default 100 units)
		 * @return Facade ready for use, or nullptr on failure
		 */
		TSharedPtr<PCGExData::FFacade> CreateFacade(
			int32 NumPoints,
			double Spacing = 100.0);

		/**
		 * Create a facade wrapping existing point data
		 * @param InData The point data to wrap
		 * @param InitOutput How to initialize the output (default: Forward, which sets Out=In)
		 *        Use Forward for read-only tests. Use New for tests that write to separate output.
		 *        Avoid Duplicate as it uses ManagedObjects which has lifecycle complexities.
		 * @return Facade ready for use, or nullptr on failure
		 */
		TSharedPtr<PCGExData::FFacade> CreateFacade(
			UPCGBasePointData* InData,
			PCGExData::EIOInit InitOutput = PCGExData::EIOInit::Forward);

		/**
		 * Create a facade with grid-positioned points
		 * @param Origin Starting corner of the grid
		 * @param Spacing Distance between points in each axis
		 * @param CountX Points in X direction
		 * @param CountY Points in Y direction
		 * @param CountZ Points in Z direction (default 1 for 2D grid)
		 */
		TSharedPtr<PCGExData::FFacade> CreateGridFacade(
			const FVector& Origin,
			const FVector& Spacing,
			int32 CountX,
			int32 CountY,
			int32 CountZ = 1);

		/**
		 * Create a facade with randomly positioned points
		 * @param Bounds Bounding box for random positions
		 * @param NumPoints Number of points to create
		 * @param Seed Random seed for reproducibility
		 */
		TSharedPtr<PCGExData::FFacade> CreateRandomFacade(
			const FBox& Bounds,
			int32 NumPoints,
			uint32 Seed = 12345u);

	private:
		FPCGExContext* Context = nullptr;
		UWorld* World = nullptr;
		AActor* TestActor = nullptr;
		UPCGComponent* PCGComponent = nullptr;

		// Internal helper to create point data
		UPCGBasePointData* CreatePointData(int32 NumPoints);
		UPCGBasePointData* CreateGridPointData(
			const FVector& Origin,
			const FVector& Spacing,
			int32 CountX,
			int32 CountY,
			int32 CountZ);
		UPCGBasePointData* CreateRandomPointData(
			const FBox& Bounds,
			int32 NumPoints,
			uint32 Seed);

		// Prevent copying
		FTestContext(const FTestContext&) = delete;
		FTestContext& operator=(const FTestContext&) = delete;
	};

	/**
	 * RAII wrapper for FTestContext
	 * Automatically initializes on construction and cleans up on destruction
	 *
	 * Example:
	 * @code
	 * FScopedTestContext ScopedCtx;
	 * if (!ScopedCtx.IsValid()) { return false; }
	 * auto Facade = ScopedCtx->CreateFacade(100);
	 * @endcode
	 */
	class PCGEXTENDEDTOOLKITTEST_API FScopedTestContext
	{
	public:
		FScopedTestContext();
		~FScopedTestContext() = default;

		bool IsValid() const { return Context && Context->IsValid(); }
		FTestContext* operator->() { return Context.Get(); }
		const FTestContext* operator->() const { return Context.Get(); }
		FTestContext* Get() { return Context.Get(); }

	private:
		TUniquePtr<FTestContext> Context;
	};

	/**
	 * Lightweight test data creator that doesn't require full context infrastructure.
	 * Use this for simple unit tests that only need point data without FPointIO/FFacade.
	 *
	 * Example:
	 * @code
	 * UPCGBasePointData* Data = FSimplePointDataFactory::CreateSequential(10, 100.0);
	 * // Use Data for testing...
	 * // Data is in transient package, will be GC'd automatically
	 * @endcode
	 */
	struct PCGEXTENDEDTOOLKITTEST_API FSimplePointDataFactory
	{
		/** Create point data with sequential positions along X axis */
		static UPCGBasePointData* CreateSequential(int32 NumPoints, double Spacing = 100.0);

		/** Create point data with grid positions */
		static UPCGBasePointData* CreateGrid(
			const FVector& Origin,
			const FVector& Spacing,
			int32 CountX,
			int32 CountY,
			int32 CountZ = 1);

		/** Create point data with random positions */
		static UPCGBasePointData* CreateRandom(
			const FBox& Bounds,
			int32 NumPoints,
			uint32 Seed = 12345u);

		/**
		 * Initialize metadata entries for all points in bulk.
		 * Call this on existing point data before setting attribute values.
		 * @param InData Point data to initialize entries for
		 * @param bConservative If true, only initialize entries that are invalid (default). If false, reinitialize all.
		 */
		static void InitializeMetadataEntries(UPCGBasePointData* InData, bool bConservative = true);
	};
}
