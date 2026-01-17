// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Cages/PCGExValencyCageNull.h"

#include "Components/BillboardComponent.h"
#include "UObject/ConstructorHelpers.h"

APCGExValencyCageNull::APCGExValencyCageNull()
{
	// Create billboard component for visualization
	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	BillboardComponent->SetupAttachment(RootComponent);

	// Use a simple X icon texture for null cages
	// This will be visible in editor only
#if WITH_EDITORONLY_DATA
	static ConstructorHelpers::FObjectFinder<UTexture2D> IconTexture(TEXT("/Engine/EditorMaterials/WidgetMaterial_X"));
	if (IconTexture.Succeeded())
	{
		BillboardComponent->SetSprite(IconTexture.Object);
	}
#endif

	BillboardComponent->SetRelativeScale3D(FVector(0.5f));
	BillboardComponent->bIsScreenSizeScaled = true;
	BillboardComponent->ScreenSize = 0.0015f;
}

FString APCGExValencyCageNull::GetCageDisplayName() const
{
	if (!CageName.IsEmpty())
	{
		return FString::Printf(TEXT("NULL: %s"), *CageName);
	}

	if (!Description.IsEmpty())
	{
		return FString::Printf(TEXT("NULL (%s)"), *Description);
	}

	return TEXT("NULL Cage");
}
