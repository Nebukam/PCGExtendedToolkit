// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/Actions/PCGExActorDataPackerActions.h"

#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "ToolMenuSection.h"
#include "Elements/PCGExPackActorData.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Views/SListView.h"

FText FPCGExActorDataPackerActions::GetName() const
{
	return INVTEXT("PCGEx Actor Data Packer");
}

FString FPCGExActorDataPackerActions::GetObjectDisplayName(UObject* Object) const
{
	return Object->GetName();
}

UClass* FPCGExActorDataPackerActions::GetSupportedClass() const
{
	return UPCGExCustomActorDataPacker::StaticClass();
}

FColor FPCGExActorDataPackerActions::GetTypeColor() const
{
	return FColor(195, 124, 40);
}

uint32 FPCGExActorDataPackerActions::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

bool FPCGExActorDataPackerActions::HasActions(const TArray<UObject*>& InObjects) const
{
	return false;
}
