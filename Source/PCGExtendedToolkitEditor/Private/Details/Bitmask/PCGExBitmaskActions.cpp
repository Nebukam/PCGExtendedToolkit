// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/Bitmask/PCGExBitmaskActions.h"

#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "ToolMenuSection.h"
#include "Collections/PCGExBitmaskCollection.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Views/SListView.h"

FText FPCGExBitmaskActions::GetName() const
{
	return INVTEXT("PCGEx Bitmasks");
}

FString FPCGExBitmaskActions::GetObjectDisplayName(UObject* Object) const
{
	return Object->GetName();
}

UClass* FPCGExBitmaskActions::GetSupportedClass() const
{
	return UPCGExBitmaskCollection::StaticClass();
}

FColor FPCGExBitmaskActions::GetTypeColor() const
{
	return FColor(195, 0, 40);
}

uint32 FPCGExBitmaskActions::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

bool FPCGExBitmaskActions::HasActions(const TArray<UObject*>& InObjects) const
{
	return false;
}
