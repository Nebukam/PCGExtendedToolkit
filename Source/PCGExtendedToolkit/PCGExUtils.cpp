// Fill out your copyright notice in the Description page of Project Settings.


#include "PCGExUtils.h"
#include "Metadata/PCGMetadataAccessor.h"
#include "Metadata/PCGMetadata.h"
#include "PCGPoint.h"

/**
 * Key-based implmentations taken from  PCGMetadataAccessor.cpp
 **/
template<typename T>
T PCGExUtils::GetAttribute(PCGMetadataEntryKey Key, const UPCGMetadata* Metadata, FName AttributeName)
{
	if (!Metadata)
	{
		UE_LOG(LogPCG, Error, TEXT("Source data has no metadata"));
		return T{};
	}

	const FPCGMetadataAttribute<T>* Attribute = static_cast<const FPCGMetadataAttribute<T>*>(Metadata->GetConstAttribute(AttributeName));
	if (Attribute && Attribute->GetTypeId() == PCG::Private::MetadataTypes<T>::Id)
	{
		return Attribute->GetValueFromItemKey(Key);
	}
	else if (Attribute)
	{
		UE_LOG(LogPCG, Error, TEXT("Attribute %s does not have the matching type"), *AttributeName.ToString());
		return T{};
	}
	else
	{
		UE_LOG(LogPCG, Error, TEXT("Invalid attribute name (%s)"), *AttributeName.ToString());
		return T{};
	}
}

template<typename T>
void PCGExUtils::SetAttribute(PCGMetadataEntryKey& Key, UPCGMetadata* Metadata, FName AttributeName, const T& Value)
{
	if (!Metadata)
	{
		UE_LOG(LogPCG, Error, TEXT("Data has no metadata; cannot write value in attribute"));
		return;
	}

	Metadata->InitializeOnSet(Key);

	if (Key == PCGInvalidEntryKey)
	{
		UE_LOG(LogPCG, Error, TEXT("Metadata key has no entry, therefore can't set values"));
		return;
	}

	FPCGMetadataAttribute<T>* Attribute = static_cast<FPCGMetadataAttribute<T>*>(Metadata->GetMutableAttribute(AttributeName));
	if (Attribute && Attribute->GetTypeId() == PCG::Private::MetadataTypes<T>::Id)
	{
		Attribute->SetValue(Key, Value);
	}
	else if (Attribute)
	{
		UE_LOG(LogPCG, Error, TEXT("Attribute %s does not have the matching type"), *AttributeName.ToString());
	}
	else
	{
		UE_LOG(LogPCG, Error, TEXT("Invalid attribute name (%s)"), *AttributeName.ToString());
	}
}

PCGExUtils::PCGExUtils()
{
}

PCGExUtils::~PCGExUtils()
{
}
