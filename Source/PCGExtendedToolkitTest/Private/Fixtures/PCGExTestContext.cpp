// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Fixtures/PCGExTestContext.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "Data/PCGPointArrayData.h"
#include "UObject/Package.h"
#include "Helpers/PCGExTestHelpers.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

#pragma region FTestContext

namespace PCGExTest
{
	FTestContext::FTestContext()
	{
	}

	FTestContext::~FTestContext()
	{
		Cleanup();
	}

	bool FTestContext::Initialize()
	{
#if WITH_EDITOR
		if (!GEditor)
		{
			// Tests require editor context
			return false;
		}

		World = GEditor->GetEditorWorldContext().World();
		if (!World)
		{
			return false;
		}

		// Spawn actor in editor world
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = MakeUniqueObjectName(World, AActor::StaticClass(), FName(TEXT("PCGExTestActor")));
		SpawnParams.bHideFromSceneOutliner = true;
		SpawnParams.bTemporaryEditorActor = true;
		SpawnParams.ObjectFlags = RF_Transient;
		TestActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);

		if (!TestActor) { return false; }

		// Add root component
		USceneComponent* RootComp = NewObject<USceneComponent>(TestActor, TEXT("RootComponent"), RF_Transient);
		TestActor->SetRootComponent(RootComp);
		RootComp->RegisterComponent();

		// Add PCG component
		PCGComponent = NewObject<UPCGComponent>(TestActor, TEXT("PCGExTestComponent"), RF_Transient);
		if (!PCGComponent) { return false; }
		TestActor->AddInstanceComponent(PCGComponent);
		PCGComponent->RegisterComponent();

		// Create FPCGExContext - use FPCGContext::Release() for proper cleanup
		Context = new FPCGExContext();
		Context->ExecutionSource = PCGComponent;

		return true;
#else
		// Non-editor builds not supported for context tests
		return false;
#endif
	}

	void FTestContext::Cleanup()
	{
		// Release context using proper PCG lifecycle method
		// FPCGContext::Release() handles Handle cleanup correctly
		if (Context)
		{
			FPCGContext::Release(Context);
			Context = nullptr;
		}

		if (PCGComponent)
		{
			PCGComponent->UnregisterComponent();
			PCGComponent->DestroyComponent();
			PCGComponent = nullptr;
		}

		if (TestActor)
		{
			TestActor->Destroy();
			TestActor = nullptr;
		}

		// We use the editor world, don't destroy it
		World = nullptr;
	}

	bool FTestContext::IsValid() const
	{
		return World && TestActor && PCGComponent && Context;
	}

	TSharedPtr<PCGExData::FPointIO> FTestContext::CreatePointIO(FName OutputPin, int32 Index)
	{
		if (!IsValid()) { return nullptr; }
		return PCGExData::NewPointIO(Context, OutputPin, Index);
	}

	TSharedPtr<PCGExData::FPointIO> FTestContext::CreatePointIO(
		const UPCGBasePointData* InData,
		FName OutputPin,
		int32 Index)
	{
		if (!IsValid() || !InData) { return nullptr; }
		return PCGExData::NewPointIO(Context, InData, OutputPin, Index);
	}

	UPCGBasePointData* FTestContext::CreatePointData(int32 NumPoints)
	{
		if (!IsValid() || NumPoints <= 0) { return nullptr; }

		// Create in transient package to avoid lifecycle issues with ManagedObjects
		UPCGBasePointData* PointData = NewObject<UPCGPointArrayData>(GetTransientPackage(), NAME_None, RF_Transient);
		if (!PointData) { return nullptr; }

		PointData->SetNumPoints(NumPoints);

		// Initialize transforms with sequential positions
		TPCGValueRange<FTransform> Transforms = PointData->GetTransformValueRange();
		TPCGValueRange<int32> Seeds = PointData->GetSeedValueRange();

		for (int32 i = 0; i < NumPoints; ++i)
		{
			Transforms[i] = FTransform(FVector(i * 100.0, 0.0, 0.0));
			Seeds[i] = i;
		}

		FSimplePointDataFactory::InitializeMetadataEntries(PointData, false);
		return PointData;
	}

	UPCGBasePointData* FTestContext::CreateGridPointData(
		const FVector& Origin,
		const FVector& Spacing,
		int32 CountX,
		int32 CountY,
		int32 CountZ)
	{
		if (!IsValid()) { return nullptr; }

		const int32 NumPoints = CountX * CountY * CountZ;
		if (NumPoints <= 0) { return nullptr; }

		// Create in transient package to avoid lifecycle issues with ManagedObjects
		UPCGBasePointData* PointData = NewObject<UPCGPointArrayData>(GetTransientPackage(), NAME_None, RF_Transient);
		if (!PointData) { return nullptr; }

		PointData->SetNumPoints(NumPoints);

		TPCGValueRange<FTransform> Transforms = PointData->GetTransformValueRange();
		TPCGValueRange<int32> Seeds = PointData->GetSeedValueRange();

		int32 Index = 0;
		for (int32 z = 0; z < CountZ; ++z)
		{
			for (int32 y = 0; y < CountY; ++y)
			{
				for (int32 x = 0; x < CountX; ++x)
				{
					FVector Position = Origin + FVector(
						x * Spacing.X,
						y * Spacing.Y,
						z * Spacing.Z
					);
					Transforms[Index] = FTransform(Position);
					Seeds[Index] = Index;
					++Index;
				}
			}
		}

		FSimplePointDataFactory::InitializeMetadataEntries(PointData, false);
		return PointData;
	}

	UPCGBasePointData* FTestContext::CreateRandomPointData(
		const FBox& Bounds,
		int32 NumPoints,
		uint32 Seed)
	{
		if (!IsValid() || NumPoints <= 0) { return nullptr; }

		// Create in transient package to avoid lifecycle issues with ManagedObjects
		UPCGBasePointData* PointData = NewObject<UPCGPointArrayData>(GetTransientPackage(), NAME_None, RF_Transient);
		if (!PointData) { return nullptr; }

		PointData->SetNumPoints(NumPoints);

		TPCGValueRange<FTransform> Transforms = PointData->GetTransformValueRange();
		TPCGValueRange<int32> Seeds = PointData->GetSeedValueRange();

		FRandomStream Random(Seed);
		const FVector Extent = Bounds.GetExtent();
		const FVector Center = Bounds.GetCenter();

		for (int32 i = 0; i < NumPoints; ++i)
		{
			FVector Position = Center + FVector(
				Random.FRandRange(-Extent.X, Extent.X),
				Random.FRandRange(-Extent.Y, Extent.Y),
				Random.FRandRange(-Extent.Z, Extent.Z)
			);
			Transforms[i] = FTransform(Position);
			Seeds[i] = i;
		}

		FSimplePointDataFactory::InitializeMetadataEntries(PointData, false);
		return PointData;
	}

	TSharedPtr<PCGExData::FFacade> FTestContext::CreateFacade(int32 NumPoints, double Spacing)
	{
		if (!IsValid() || NumPoints <= 0) { return nullptr; }

		// Create point data
		UPCGBasePointData* PointData = CreatePointData(NumPoints);
		if (!PointData) { return nullptr; }

		// Update spacing if not default
		if (!FMath::IsNearlyEqual(Spacing, 100.0))
		{
			TPCGValueRange<FTransform> Transforms = PointData->GetTransformValueRange();
			for (int32 i = 0; i < NumPoints; ++i)
			{
				Transforms[i] = FTransform(FVector(i * Spacing, 0.0, 0.0));
			}
		}

		return CreateFacade(PointData, PCGExData::EIOInit::Forward);
	}

	TSharedPtr<PCGExData::FFacade> FTestContext::CreateFacade(
		UPCGBasePointData* InData,
		PCGExData::EIOInit InitOutput)
	{
		if (!IsValid() || !InData) { return nullptr; }

		TSharedPtr<PCGExData::FPointIO> PointIO = CreatePointIO(InData);
		if (!PointIO) { return nullptr; }

		if (!PointIO->InitializeOutput(InitOutput)) { return nullptr; }

		return MakeShared<PCGExData::FFacade>(PointIO.ToSharedRef());
	}

	TSharedPtr<PCGExData::FFacade> FTestContext::CreateGridFacade(
		const FVector& Origin,
		const FVector& Spacing,
		int32 CountX,
		int32 CountY,
		int32 CountZ)
	{
		if (!IsValid()) { return nullptr; }

		UPCGBasePointData* PointData = CreateGridPointData(Origin, Spacing, CountX, CountY, CountZ);
		if (!PointData) { return nullptr; }

		return CreateFacade(PointData, PCGExData::EIOInit::Forward);
	}

	TSharedPtr<PCGExData::FFacade> FTestContext::CreateRandomFacade(
		const FBox& Bounds,
		int32 NumPoints,
		uint32 Seed)
	{
		if (!IsValid()) { return nullptr; }

		UPCGBasePointData* PointData = CreateRandomPointData(Bounds, NumPoints, Seed);
		if (!PointData) { return nullptr; }

		return CreateFacade(PointData, PCGExData::EIOInit::Forward);
	}
}

#pragma endregion

#pragma region FScopedTestContext

namespace PCGExTest
{
	FScopedTestContext::FScopedTestContext()
	{
		Context = MakeUnique<FTestContext>();
		if (!Context->Initialize())
		{
			Context.Reset();
		}
	}
}

#pragma endregion

#pragma region FSimplePointDataFactory

namespace PCGExTest
{
	UPCGBasePointData* FSimplePointDataFactory::CreateSequential(int32 NumPoints, double Spacing)
	{
		if (NumPoints <= 0) { return nullptr; }

		UPCGBasePointData* PointData = NewObject<UPCGPointArrayData>(GetTransientPackage(), NAME_None, RF_Transient);
		if (!PointData) { return nullptr; }

		PointData->SetNumPoints(NumPoints);

		TPCGValueRange<FTransform> Transforms = PointData->GetTransformValueRange();
		TPCGValueRange<int32> Seeds = PointData->GetSeedValueRange();

		for (int32 i = 0; i < NumPoints; ++i)
		{
			Transforms[i] = FTransform(FVector(i * Spacing, 0.0, 0.0));
			Seeds[i] = i;
		}

		InitializeMetadataEntries(PointData, false);
		return PointData;
	}

	UPCGBasePointData* FSimplePointDataFactory::CreateGrid(
		const FVector& Origin,
		const FVector& Spacing,
		int32 CountX,
		int32 CountY,
		int32 CountZ)
	{
		const int32 NumPoints = CountX * CountY * CountZ;
		if (NumPoints <= 0) { return nullptr; }

		UPCGBasePointData* PointData = NewObject<UPCGPointArrayData>(GetTransientPackage(), NAME_None, RF_Transient);
		if (!PointData) { return nullptr; }

		PointData->SetNumPoints(NumPoints);

		TPCGValueRange<FTransform> Transforms = PointData->GetTransformValueRange();
		TPCGValueRange<int32> Seeds = PointData->GetSeedValueRange();

		int32 Index = 0;
		for (int32 z = 0; z < CountZ; ++z)
		{
			for (int32 y = 0; y < CountY; ++y)
			{
				for (int32 x = 0; x < CountX; ++x)
				{
					FVector Position = Origin + FVector(
						x * Spacing.X,
						y * Spacing.Y,
						z * Spacing.Z
					);
					Transforms[Index] = FTransform(Position);
					Seeds[Index] = Index;
					++Index;
				}
			}
		}

		InitializeMetadataEntries(PointData, false);
		return PointData;
	}

	UPCGBasePointData* FSimplePointDataFactory::CreateRandom(
		const FBox& Bounds,
		int32 NumPoints,
		uint32 Seed)
	{
		if (NumPoints <= 0) { return nullptr; }

		UPCGBasePointData* PointData = NewObject<UPCGPointArrayData>(GetTransientPackage(), NAME_None, RF_Transient);
		if (!PointData) { return nullptr; }

		PointData->SetNumPoints(NumPoints);

		TPCGValueRange<FTransform> Transforms = PointData->GetTransformValueRange();
		TPCGValueRange<int32> Seeds = PointData->GetSeedValueRange();

		FRandomStream Random(Seed);
		const FVector Extent = Bounds.GetExtent();
		const FVector Center = Bounds.GetCenter();

		for (int32 i = 0; i < NumPoints; ++i)
		{
			FVector Position = Center + FVector(
				Random.FRandRange(-Extent.X, Extent.X),
				Random.FRandRange(-Extent.Y, Extent.Y),
				Random.FRandRange(-Extent.Z, Extent.Z)
			);
			Transforms[i] = FTransform(Position);
			Seeds[i] = i;
		}

		InitializeMetadataEntries(PointData, false);
		return PointData;
	}

	void FSimplePointDataFactory::InitializeMetadataEntries(UPCGBasePointData* InData, bool bConservative)
	{
		if (!InData) { return; }

		UPCGMetadata* Metadata = InData->MutableMetadata();
		if (!Metadata) { return; }

		TPCGValueRange<int64> MetadataEntries = InData->GetMetadataEntryValueRange(true);
		const int32 NumPoints = MetadataEntries.Num();

		if (bConservative)
		{
			// Only initialize entries that need it
			TArray<int64*> KeysNeedingInit;
			KeysNeedingInit.Reserve(NumPoints);
			const int64 ItemKeyOffset = Metadata->GetItemKeyCountForParent();

			for (int64& Key : MetadataEntries)
			{
				if (Key == PCGInvalidEntryKey || Key < ItemKeyOffset)
				{
					KeysNeedingInit.Add(&Key);
				}
			}

			if (KeysNeedingInit.Num() > 0)
			{
				Metadata->AddEntriesInPlace(KeysNeedingInit);
			}
		}
		else
		{
			// Reinitialize all entries
			TArray<int64*> AllKeys;
			AllKeys.Reserve(NumPoints);

			for (int64& Key : MetadataEntries)
			{
				AllKeys.Add(&Key);
			}

			if (AllKeys.Num() > 0)
			{
				Metadata->AddEntriesInPlace(AllKeys);
			}
		}
	}
}

#pragma endregion
