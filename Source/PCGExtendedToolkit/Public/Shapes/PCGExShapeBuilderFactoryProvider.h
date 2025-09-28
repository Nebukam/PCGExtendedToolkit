﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFactoryProvider.h"
#include "PCGExPointsProcessor.h"
#include "PCGExShapes.h"

#include "PCGExShapeBuilderFactoryProvider.generated.h"

#define PCGEX_SHAPE_BUILDER_BOILERPLATE(_SHAPE) \
TSharedPtr<FPCGExShapeBuilderOperation> UPCGExShape##_SHAPE##Factory::CreateOperation(FPCGExContext* InContext) const{ \
	PCGEX_FACTORY_NEW_OPERATION(Shape##_SHAPE##Builder)\
	NewOperation->Config = Config; \
	NewOperation->Config.Init(); \
	NewOperation->BaseConfig = NewOperation->Config; \
	NewOperation->Transform = NewOperation->Config.LocalTransform; \
	return NewOperation; } \
UPCGExFactoryData* UPCGExCreateShape##_SHAPE##Settings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const{ \
	UPCGExShape##_SHAPE##Factory* NewFactory = InContext->ManagedObjects->New<UPCGExShape##_SHAPE##Factory>(); \
	NewFactory->Config = Config; \
	return Super::CreateFactory(InContext, NewFactory);}

class FPCGExShapeBuilderOperation;

USTRUCT(/*PCG_DataType*/DisplayName="PCGEx | Shape")
struct FPCGExDataTypeInfoShape : public FPCGExFactoryDataTypeInfo
{
	GENERATED_BODY()
	PCG_DECLARE_TYPE_INFO(PCGEXTENDEDTOOLKIT_API)
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExShapeBuilderFactoryData : public UPCGExFactoryData
{
	GENERATED_BODY()

public:
	PCG_ASSIGN_TYPE_INFO(FPCGExDataTypeInfoShape)

	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::ShapeBuilder; }
	virtual TSharedPtr<FPCGExShapeBuilderOperation> CreateOperation(FPCGExContext* InContext) const;
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params", meta=(PCGExNodeLibraryDoc="misc/shapes/create-shapes"))
class PCGEXTENDEDTOOLKIT_API UPCGExShapeBuilderFactoryProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

protected:
	PCGEX_FACTORY_TYPE_ID(FPCGExDataTypeInfoShape)

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ShapeBuilder, "ShapeBuilder Definition", "Creates a single shape builder node, to be used with a Shape processor node.")
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->ColorShape; }
#endif
	//~End UPCGSettings

	virtual FName GetMainOutputPin() const override { return PCGExShapes::OutputShapeBuilderLabel; }
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;
};
