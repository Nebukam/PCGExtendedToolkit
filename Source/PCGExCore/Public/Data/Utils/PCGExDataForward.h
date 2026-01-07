// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDataFilterDetails.h"
#include "PCGExDataForwardDetails.h"
#include "Types/PCGExAttributeIdentity.h"

namespace PCGExData
{
	struct FConstPoint;
	class IBuffer;
	class FDataForwardHandler;
	class IAttributeBroadcaster;
}

namespace PCGExData
{
	class PCGEXCORE_API FDataForwardHandler
	{		
		FPCGExForwardDetails Details;
		TSharedPtr<FFacade> SourceDataFacade;
		TSharedPtr<FFacade> TargetDataFacade;
		TArray<FAttributeIdentity> Identities;
		TArray<TSharedPtr<IBuffer>> Readers;
		TArray<TSharedPtr<IBuffer>> Writers;
		bool bElementDomainToDataDomain = false;

	public:
		
		using FValidateFn = std::function<bool(const FAttributeIdentity&)>;
		
		~FDataForwardHandler() = default;
		FDataForwardHandler(const FPCGExForwardDetails& InDetails, const TSharedPtr<FFacade>& InSourceDataFacade, const bool ElementDomainToDataDomain = false);
		FDataForwardHandler(const FPCGExForwardDetails& InDetails, const TSharedPtr<FFacade>& InSourceDataFacade, const TSharedPtr<FFacade>& InTargetDataFacade, const bool ElementDomainToDataDomain = false);
		
		void ValidateIdentities(FValidateFn&& Fn);
		
		bool IsEmpty() const { return Identities.IsEmpty(); }
		void Forward(const int32 SourceIndex, const int32 TargetIndex);
		void Forward(const int32 SourceIndex, const TSharedPtr<FFacade>& InTargetDataFacade);
		void Forward(const int32 SourceIndex, const TSharedPtr<FFacade>& InTargetDataFacade, const TArray<int32>& Indices);
		void Forward(const int32 SourceIndex, UPCGMetadata* InTargetMetadata);
	};
}
