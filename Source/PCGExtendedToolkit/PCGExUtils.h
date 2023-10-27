// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Metadata/PCGMetadata.h"

/**
 * 
 */
class PCGEXTENDEDTOOLKIT_API PCGExUtils
{
public:
	template <class T>
	T GetAttribute(PCGMetadataEntryKey Key, const UPCGMetadata* Metadata, FName AttributeName);
	template <class T>
	void SetAttribute(PCGMetadataEntryKey& Key, UPCGMetadata* Metadata, FName AttributeName, const T& Value);
	PCGExUtils();
	~PCGExUtils();
};
