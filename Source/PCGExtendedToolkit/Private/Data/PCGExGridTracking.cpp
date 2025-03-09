// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExGridTracking.h"

#include "PCGGraph.h"


FPCGExGridID::FPCGExGridID(const FVector& InLocation, const int32 InGridSize, const FName InWorldID)
	: Name(InWorldID), GridSize(FMath::Max(1, InGridSize)), Location(InLocation)
{
}

FPCGExGridID::FPCGExGridID(const UPCGComponent* InComponent, const FVector& InLocation, const FName InName)
{
	Location = InLocation;
	Name = InName;
	if (const UPCGGraph* Graph = InComponent->GetGraph()) { GridSize = Graph->GetDefaultGridSize(); }
}

FPCGExGridID::FPCGExGridID(const UPCGComponent* InComponent, const FName InName)
{
	const AActor* Owner = InComponent->GetOwner();
	Location = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
	Name = InName;
	if (const UPCGGraph* Graph = InComponent->GetGraph()) { GridSize = Graph->GetDefaultGridSize(); }
}

FPCGExGridID FPCGExGridID::MakeFromGridID(const FVector& InLocation) const
{
	return FPCGExGridID(InLocation, GridSize, Name);
}
