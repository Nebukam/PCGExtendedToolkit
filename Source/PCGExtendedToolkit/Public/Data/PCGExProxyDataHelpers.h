// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "Details/PCGExMacros.h"

struct FPCGExContext;
class UPCGBasePointData;

template <typename T>
class FPCGMetadataAttribute;

namespace PCGExData
{
	class FFacade;
	class IBufferProxy;

	template<typename T_REAL>
	class TBuffer;
	
	struct FProxyDescriptor;
	
	template <typename T_REAL>
	TSharedPtr<TBuffer<T_REAL>> TryGetBuffer(FPCGExContext* InContext, const FProxyDescriptor& InDescriptor, const TSharedPtr<FFacade>& InDataFacade);

	template <typename T_REAL>
	void TryGetInOutAttr(const FProxyDescriptor& InDescriptor, const TSharedPtr<FFacade>& InDataFacade, const FPCGMetadataAttribute<T_REAL>*& OutInAttribute, FPCGMetadataAttribute<T_REAL>*& OutOutAttribute);

#pragma region externalization TryGetInOutAttr / TryGetBuffer

#define PCGEX_TPL(_TYPE, _NAME, ...) \
extern template void TryGetInOutAttr<_TYPE>(const FProxyDescriptor& InDescriptor, const TSharedPtr<FFacade>& InDataFacade, const FPCGMetadataAttribute<_TYPE>*& OutInAttribute, FPCGMetadataAttribute<_TYPE>*& OutOutAttribute); \
extern template TSharedPtr<TBuffer<_TYPE>> TryGetBuffer<_TYPE>(FPCGExContext* InContext, const FProxyDescriptor& InDescriptor, const TSharedPtr<FFacade>& InDataFacade);
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion

	template <typename T_REAL, typename T_WORKING>
	TSharedPtr<IBufferProxy> GetProxyBuffer(FPCGExContext* InContext, const FProxyDescriptor& InDescriptor, const TSharedPtr<FFacade>& InDataFacade, UPCGBasePointData* PointData);

#pragma region externalization

#define PCGEX_TPL(_TYPE_A, _NAME_A, _TYPE_B, _NAME_B, ...) \
extern template TSharedPtr<IBufferProxy> GetProxyBuffer<_TYPE_A, _TYPE_B>(FPCGExContext* InContext, const FProxyDescriptor& InDescriptor, const TSharedPtr<FFacade>& InDataFacade, UPCGBasePointData* PointData);
	PCGEX_FOREACH_SUPPORTEDTYPES_PAIRS(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion

	TSharedPtr<IBufferProxy> GetProxyBuffer(FPCGExContext* InContext, const FProxyDescriptor& InDescriptor);

	template <typename T>
	TSharedPtr<IBufferProxy> GetConstantProxyBuffer(const T& Constant);

#pragma region externalization GetConstantProxyBuffer

#define PCGEX_TPL(_TYPE, _NAME, ...) \
extern template TSharedPtr<IBufferProxy> GetConstantProxyBuffer<_TYPE>(const _TYPE& Constant);
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion

	bool GetPerFieldProxyBuffers(
		FPCGExContext* InContext,
		const FProxyDescriptor& InBaseDescriptor,
		const int32 NumDesiredFields,
		TArray<TSharedPtr<IBufferProxy>>& OutProxies);
}
