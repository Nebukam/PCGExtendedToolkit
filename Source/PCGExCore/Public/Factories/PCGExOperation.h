// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

namespace PCGExData
{
	class FFacadePreloader;
	class FFacade;
}

struct FPCGExContext;

namespace PCGExMT
{
	class FScopedContainer;
	struct FScope;
	class FTaskManager;
}

class FPCGMetadataAttributeBase;
/**
 * 
 */
class PCGEXCORE_API FPCGExOperation : public TSharedFromThis<FPCGExOperation>
{
public:
	FPCGExOperation() = default;
	virtual ~FPCGExOperation() = default;
	void BindContext(FPCGExContext* InContext);

#if WITH_EDITOR
	virtual void UpdateUserFacingInfos();
#endif

	virtual void RegisterAssetDependencies(FPCGExContext* InContext);

	TSharedPtr<PCGExData::FFacade> PrimaryDataFacade;
	TSharedPtr<PCGExData::FFacade> SecondaryDataFacade;


	virtual void RegisterConsumableAttributesWithFacade(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InFacade) const;
	virtual void RegisterPrimaryBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) const;

	virtual void InitForScopes(const TArray<PCGExMT::FScope>& Loops);
	virtual TSharedPtr<PCGExMT::FScopedContainer> GetScopedContainer(const PCGExMT::FScope& InScope) const;

protected:
	FPCGExContext* Context = nullptr;

	//~End UPCGExOperation interface
};
