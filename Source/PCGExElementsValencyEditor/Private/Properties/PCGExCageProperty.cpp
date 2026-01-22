// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Properties/PCGExCageProperty.h"
#include "Core/PCGExCagePropertyCompiled.h"

UPCGExCagePropertyBase::UPCGExCagePropertyBase()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = false;
}

FName UPCGExCagePropertyBase::GetEffectivePropertyName() const
{
	// If user specified a name, use it; otherwise default to component's name
	return PropertyName.IsNone() ? GetFName() : PropertyName;
}

//
// UPCGExCageProperty_AssetCollection
//

bool UPCGExCageProperty_AssetCollection::CompileProperty(FInstancedStruct& OutCompiled) const
{
	FPCGExCagePropertyCompiled_AssetCollection Compiled;
	Compiled.PropertyName = GetEffectivePropertyName();
	Compiled.AssetCollection = AssetCollection;

	OutCompiled.InitializeAs<FPCGExCagePropertyCompiled_AssetCollection>(MoveTemp(Compiled));
	return true;
}

UScriptStruct* UPCGExCageProperty_AssetCollection::GetCompiledStructType() const
{
	return FPCGExCagePropertyCompiled_AssetCollection::StaticStruct();
}

//
// UPCGExCageProperty_Tags
//

bool UPCGExCageProperty_Tags::CompileProperty(FInstancedStruct& OutCompiled) const
{
	FPCGExCagePropertyCompiled_Tags Compiled;
	Compiled.PropertyName = GetEffectivePropertyName();
	Compiled.Tags = Tags;

	OutCompiled.InitializeAs<FPCGExCagePropertyCompiled_Tags>(MoveTemp(Compiled));
	return true;
}

UScriptStruct* UPCGExCageProperty_Tags::GetCompiledStructType() const
{
	return FPCGExCagePropertyCompiled_Tags::StaticStruct();
}

//
// UPCGExCageProperty_Metadata
//

bool UPCGExCageProperty_Metadata::CompileProperty(FInstancedStruct& OutCompiled) const
{
	FPCGExCagePropertyCompiled_Metadata Compiled;
	Compiled.PropertyName = GetEffectivePropertyName();
	Compiled.Metadata = Metadata;

	OutCompiled.InitializeAs<FPCGExCagePropertyCompiled_Metadata>(MoveTemp(Compiled));
	return true;
}

UScriptStruct* UPCGExCageProperty_Metadata::GetCompiledStructType() const
{
	return FPCGExCagePropertyCompiled_Metadata::StaticStruct();
}
