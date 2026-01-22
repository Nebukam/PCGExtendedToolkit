// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Properties/PCGExCagePropertyTypes.h"
#include "Core/PCGExCagePropertyCompiledTypes.h"

#pragma region UPCGExCageProperty_AssetCollection

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

#pragma endregion

#pragma region UPCGExCageProperty_Metadata

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

#pragma endregion
