// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExAssemblyRootActor.h"

#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"

APCGExAssemblyRootActor::APCGExAssemblyRootActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;

	// Its own components never render or collide; the attached content does. Deliberately NOT an
	// editor-only actor: the scanners that consume it reject editor-only actors.
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);

#if WITH_EDITORONLY_DATA
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite)
	{
		// Optional finder: a missing texture keeps the billboard's stock S_Actor sprite instead of failing the CDO.
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> SpriteTexture;
			FName ID_Category;
			FText NAME_Category;

			FConstructorStatics()
				: SpriteTexture(TEXT("/PCGExtendedToolkit/Data/Textures/PCGExEditor_PCGDA_DataAsset"))
				, ID_Category(TEXT("PCGEx"))
				, NAME_Category(NSLOCTEXT("SpriteCategory", "PCGEx", "PCGEx"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		if (UTexture2D* SpriteTexture = ConstructorStatics.SpriteTexture.Get())
		{
			Sprite->Sprite = SpriteTexture;
		}

		Sprite->SpriteInfo.Category = ConstructorStatics.ID_Category;
		Sprite->SpriteInfo.DisplayName = ConstructorStatics.NAME_Category;
		Sprite->SetupAttachment(Root);
		Sprite->bIsScreenSizeScaled = true;
	}
#endif
}

bool APCGExAssemblyRootActor::IsSelectionParentOfAttachedActors() const
{
#if WITH_EDITORONLY_DATA
	return !bEditorSubSelectionLatch;
#else
	return false;
#endif
}
