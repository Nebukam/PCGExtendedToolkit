// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "Metadata/PCGMetadataAttributeTraits.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Details/PCGExMacros.h"
//#include "Data/PCGExTypeErasedProxy.h"
//#include "Data/BlendOperations/PCGExBlendOperations.h"

namespace PCGExTypeOps
{
	class ITypeOpsBase;
}

struct FPCGExContext;

namespace PCGExData
{
	class FTypeErasedProxy;
	class FFacade;
	class IBuffer;
	struct FProxyDescriptor;
}

namespace PCGExDataBlending
{
	class IBlendOperation;
	struct FBlendingParam;
	
	/**
	 * FTypeErasedProxyBlender - Type-erased proxy data blender
	 * 
	 * Replaces the template-heavy hierarchy:
	 *   FProxyDataBlender -> IProxyDataBlender<T> -> TProxyDataBlender<T, BlendMode, bReset>
	 * 
	 * This implementation leverages the existing type infrastructure:
	 *   - IBlendOperation from PCGExBlendOperations.h for blend logic
	 *   - FTypeErasedProxy for data access
	 *   - ITypeOpsBase for type operations
	 * 
	 * Old system: 616+ template instantiations (14 types × 22 modes × 2 reset flags)
	 * New system: 1 class using IBlendOperation (14 instantiations total)
	 */
	class PCGEXTENDEDTOOLKIT_API FTypeErasedProxyBlender : public TSharedFromThis<FTypeErasedProxyBlender>
	{
	protected:
		// Proxies for A, B, C (source A, source B, target/output)
		TSharedPtr<PCGExData::FTypeErasedProxy> ProxyA;
		TSharedPtr<PCGExData::FTypeErasedProxy> ProxyB;
		TSharedPtr<PCGExData::FTypeErasedProxy> ProxyC;  // Output
		
		// The actual blend operation (from PCGExBlendOperations)
		TSharedPtr<IBlendOperation> BlendOp;
		
		// Type operations (from FTypeOps system)
		const PCGExTypeOps::ITypeOpsBase* TypeOps = nullptr;
		
		// Type information
		EPCGMetadataTypes WorkingType = EPCGMetadataTypes::Unknown;
		EPCGExABBlendingType BlendMode = EPCGExABBlendingType::None;
		bool bResetValueForMultiBlend = true;
		
		// Temporary storage for values and accumulation
		alignas(16) uint8 AccumulatorStorage[sizeof(FTransform)];
		
	public:
		FTypeErasedProxyBlender();
		virtual ~FTypeErasedProxyBlender();
		
		// === Initialization ===
		
		/**
		 * Initialize from blend parameters
		 * This is the main initialization method
		 */
		bool Init(
			FPCGExContext* InContext,
			const FBlendingParam& InParam,
			const TSharedPtr<PCGExData::FFacade>& InTargetFacade,
			const TSharedPtr<PCGExData::FFacade>& InSourceFacade,
			PCGExData::EIOSide InSide,
			bool bWantsDirectAccess = false);
		
		/**
		 * Initialize from pre-created proxies
		 */
		bool InitFromProxies(
			const TSharedPtr<PCGExData::FTypeErasedProxy>& InProxyA,
			const TSharedPtr<PCGExData::FTypeErasedProxy>& InProxyB,
			const TSharedPtr<PCGExData::FTypeErasedProxy>& InProxyC,
			EPCGExABBlendingType InBlendMode,
			bool bInResetValueForMultiBlend = true);
		
		/**
		 * Initialize with just output proxy (A=B=C)
		 */
		bool InitSelfBlend(
			const TSharedPtr<PCGExData::FTypeErasedProxy>& InProxy,
			EPCGExABBlendingType InBlendMode,
			bool bInResetValueForMultiBlend = true);
		
		// === Blend Operations ===
		
		/**
		 * Blend: Target = Source | Target (in-place)
		 */
		FORCEINLINE void Blend(int32 SourceIndex, int32 TargetIndex, double Weight = 1.0)
		{
			Blend(SourceIndex, TargetIndex, TargetIndex, Weight);
		}
		
		/**
		 * Blend: Target = SourceA | SourceB
		 */
		void Blend(int32 SourceIndexA, int32 SourceIndexB, int32 TargetIndex, double Weight = 1.0);
		
		// === Multi-Blend Operations ===
		
		/**
		 * Begin multi-source blend at target index
		 * Returns operation stats tracker
		 */
		PCGEx::FOpStats BeginMultiBlend(int32 TargetIndex);
		
		/**
		 * Add a source to the multi-blend
		 */
		void MultiBlend(int32 SourceIndex, int32 TargetIndex, double Weight, PCGEx::FOpStats& Tracker);
		
		/**
		 * Finalize multi-blend at target index
		 */
		void EndMultiBlend(int32 TargetIndex, PCGEx::FOpStats& Tracker);
		
		// === Utility Operations ===
		
		/**
		 * Divide target value by scalar
		 * Useful for finalizing average operations
		 */
		void Div(int32 TargetIndex, double Divider);
		
		// === Typed Set Methods ===
		
		/*
		template<typename T>
		void Set(int32 TargetIndex, const T& Value) const
		{
			if (ProxyC)
			{
				ProxyC->Set<T>(TargetIndex, Value);
			}
		}
		*/
		
		// Set by runtime type
		void Set(int32 TargetIndex, EPCGMetadataTypes Type, const void* Value) const;
		
		// === Accessors ===
		
		bool IsValid() const { return ProxyC != nullptr && BlendOp != nullptr; }
		EPCGMetadataTypes GetWorkingType() const { return WorkingType; }
		EPCGExABBlendingType GetBlendMode() const { return BlendMode; }
		const IBlendOperation* GetBlendOperation() const { return BlendOp.Get(); }
		
		TSharedPtr<PCGExData::IBuffer> GetOutputBuffer() const;
		TSharedPtr<PCGExData::FTypeErasedProxy> GetProxyA() const { return ProxyA; }
		TSharedPtr<PCGExData::FTypeErasedProxy> GetProxyB() const { return ProxyB; }
		TSharedPtr<PCGExData::FTypeErasedProxy> GetProxyC() const { return ProxyC; }
		
	protected:
		// Create the blend operation from current settings using FBlendOperationFactory
		void CreateBlendOperation();
	};
	
	/*
#define PCGEX_TPL(_TYPE, _NAME, ...) extern template void FTypeErasedProxyBlender::Set<_TYPE>(int32 TargetIndex, const _TYPE& Value) const;
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL
*/

	/**
	 * Factory for creating type-erased proxy blenders
	 */
	class PCGEXTENDEDTOOLKIT_API FTypeErasedProxyBlenderFactory
	{
	public:
		/**
		 * Create blender from descriptors
		 * Replaces CreateProxyBlender<T_REAL, T_WORKING>(...)
		 */
		static TSharedPtr<FTypeErasedProxyBlender> Create(
			FPCGExContext* InContext,
			EPCGExABBlendingType BlendMode,
			const PCGExData::FProxyDescriptor& DescriptorA,
			const PCGExData::FProxyDescriptor& DescriptorB,
			const PCGExData::FProxyDescriptor& DescriptorC,
			bool bResetValueForMultiBlend = true);
		
		/**
		 * Create blender with same source for A and B
		 */
		static TSharedPtr<FTypeErasedProxyBlender> Create(
			FPCGExContext* InContext,
			EPCGExABBlendingType BlendMode,
			const PCGExData::FProxyDescriptor& DescriptorA,
			const PCGExData::FProxyDescriptor& DescriptorC,
			bool bResetValueForMultiBlend = true);
		
		/**
		 * Create blender from existing proxies
		 */
		static TSharedPtr<FTypeErasedProxyBlender> CreateFromProxies(
			const TSharedPtr<PCGExData::FTypeErasedProxy>& ProxyA,
			const TSharedPtr<PCGExData::FTypeErasedProxy>& ProxyB,
			const TSharedPtr<PCGExData::FTypeErasedProxy>& ProxyC,
			EPCGExABBlendingType BlendMode,
			bool bResetValueForMultiBlend = true);
		
		/**
		 * Create blender for blend parameter
		 */
		static TSharedPtr<FTypeErasedProxyBlender> CreateFromParam(
			FPCGExContext* InContext,
			const FBlendingParam& InParam,
			const TSharedPtr<PCGExData::FFacade>& InTargetFacade,
			const TSharedPtr<PCGExData::FFacade>& InSourceFacade,
			PCGExData::EIOSide InSide,
			bool bWantsDirectAccess = false);
	};
	
}