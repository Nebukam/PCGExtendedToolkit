// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/PCGExRelationalData.h"

#include "PCGExCommon.h"
#include "Data/PCGPointData.h"
//#include "Metadata/Accessors/IPCGAttributeAccessor.h"
//#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

UPCGExRelationalData::UPCGExRelationalData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LocalRelations.Empty();
	Relations = &LocalRelations;
}

/**
 * 
 * @param PointData 
 * @return 
 */
bool UPCGExRelationalData::IsDataReady(UPCGPointData* PointData)
{
	// Whether the data has metadata matching this RelationalData block or not
	return true;
}

/**
 * Initialize as new from Params
 * @param InParams 
 * @param InRelationalData 
 */
void UPCGExRelationalData::Initialize(UPCGExRelationalParamsData** InParams)
{

	Parent = nullptr;
	Params = *InParams;
	
	LocalRelations.Empty();
	Relations = &LocalRelations;
	
}

/**
 * Initialize this from another RelationalData
 * @param InRelationalData 
 */
void UPCGExRelationalData::Initialize(UPCGExRelationalData** InRelationalData)
{

	Parent = *InRelationalData;
	Params = Parent->Params;
	
	Relations = Parent->Relations;
	
}

