// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Metadata/PCGMetadataAttributeTraits.h"
#include "Helpers/PCGExMetaHelpers.h"
#include "UObject/SoftObjectPath.h"

struct FPCGExContext;
class UPCGBasePointData;

template <typename T>
class FPCGMetadataAttribute;

namespace PCGExData
{
	class FFacade;
	class IBufferProxy;

	template <typename T_REAL>
	class TBuffer;

	struct FProxyDescriptor;

	//
	// Buffer acquisition helpers
	//

	template <typename T_REAL>
	TSharedPtr<TBuffer<T_REAL>> TryGetBuffer(
		FPCGExContext* InContext,
		const FProxyDescriptor& InDescriptor,
		const TSharedPtr<FFacade>& InDataFacade);

	template <typename T_REAL>
	void TryGetInOutAttr(
		const FProxyDescriptor& InDescriptor,
		const TSharedPtr<FFacade>& InDataFacade,
		const FPCGMetadataAttribute<T_REAL>*& OutInAttribute,
		FPCGMetadataAttribute<T_REAL>*& OutOutAttribute);

#pragma region externalization TryGetInOutAttr / TryGetBuffer

#define PCGEX_TPL(_TYPE, _NAME, ...) \
	extern template void TryGetInOutAttr<_TYPE>( \
		const FProxyDescriptor& InDescriptor, \
		const TSharedPtr<FFacade>& InDataFacade, \
		const FPCGMetadataAttribute<_TYPE>*& OutInAttribute, \
		FPCGMetadataAttribute<_TYPE>*& OutOutAttribute); \
	extern template TSharedPtr<TBuffer<_TYPE>> TryGetBuffer<_TYPE>( \
		FPCGExContext* InContext, \
		const FProxyDescriptor& InDescriptor, \
		const TSharedPtr<FFacade>& InDataFacade);
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion

	//
	// Main proxy creation function - fully type-erased
	//
	PCGEXCORE_API TSharedPtr<IBufferProxy> GetProxyBuffer(
		FPCGExContext* InContext,
		const FProxyDescriptor& InDescriptor);

	//
	// Constant proxy creation
	//
	template <typename T>
	TSharedPtr<IBufferProxy> GetConstantProxyBuffer(const T& Constant, EPCGMetadataTypes InWorkingType);

#pragma region externalization GetConstantProxyBuffer

#define PCGEX_TPL(_TYPE, _NAME, ...) \
	extern template TSharedPtr<IBufferProxy> GetConstantProxyBuffer<_TYPE>(const _TYPE& Constant, EPCGMetadataTypes InWorkingType);
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion

	//
	// Per-field proxy creation for multi-component types
	//
	PCGEXCORE_API bool GetPerFieldProxyBuffers(
		FPCGExContext* InContext,
		const FProxyDescriptor& InBaseDescriptor,
		const int32 NumDesiredFields,
		TArray<TSharedPtr<IBufferProxy>>& OutProxies);
}
