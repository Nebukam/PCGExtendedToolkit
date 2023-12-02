// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

namespace PCGExPaths
{
	struct PCGEXTENDEDTOOLKIT_API FMetadata
	{
		double Position = 0;
		double TotalLength = 0;

		double GetAlpha() const { return Position / TotalLength; }
		double GetInvertedAlpha() const { return 1 - (Position / TotalLength); }
	};

	constexpr FMetadata InvalidMetadata = FMetadata();
}
