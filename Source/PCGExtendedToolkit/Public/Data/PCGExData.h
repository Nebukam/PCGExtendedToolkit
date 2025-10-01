// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

#include "PCGExPointElements.h"
#include "PCGExCommon.h"

#pragma region DATA MACROS

#ifndef PCGEX_DATA_MACROS
#define PCGEX_DATA_MACROS

#define PCGEX_INIT_IO_VOID(_IO, _INIT) if (!_IO->InitializeOutput(_INIT)) { return; }
#define PCGEX_INIT_IO(_IO, _INIT) if (!_IO->InitializeOutput(_INIT)) { return false; }

#define PCGEX_CLEAR_IO_VOID(_IO) if (!_IO->InitializeOutput(PCGExData::EIOInit::NoInit)) { return; } _IO->Disable();
#define PCGEX_CLEAR_IO(_IO) if (!_IO->InitializeOutput(PCGExData::EIOInit::NoInit)) { return false; } _IO->Disable();

#endif
#pragma endregion

namespace PCGEx
{
	struct FAttributeIdentity;

	template <typename T>
	class TAttributeBroadcaster;
}

namespace PCGExGeo
{
	class FPointBoxCloud;
}

namespace PCGExData
{
	enum class EBufferInit : uint8
	{
		Inherit = 0,
		New,
	};

	enum class EDomainType : uint8
	{
		Unknown  = 0,
		Data     = 1,
		Elements = 2,
	};

#pragma region Buffers

	class FFacade;

	PCGEXTENDEDTOOLKIT_API
	uint64 BufferUID(const FPCGAttributeIdentifier& Identifier, const EPCGMetadataTypes Type);

	PCGEXTENDEDTOOLKIT_API
	FPCGAttributeIdentifier GetBufferIdentifierFromSelector(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData);

	class PCGEXTENDEDTOOLKIT_API IBuffer : public TSharedFromThis<IBuffer>
	{
		friend class FFacade;

	protected:
		mutable FRWLock BufferLock;

		EPCGMetadataTypes Type = EPCGMetadataTypes::Unknown;
		EDomainType UnderlyingDomain = EDomainType::Unknown;

		uint64 UID = 0;

		bool bIsNewOutput = false;
		std::atomic<bool> bIsEnabled{true}; // BUG : Need to have a better look at why we hang when this is false
		bool bReadComplete = false;

	public:
		FPCGAttributeIdentifier Identifier;
		bool bResetWithFirstValue = false;

		bool IsEnabled() const { return bIsEnabled.load(std::memory_order_acquire); }
		void Disable() { bIsEnabled.store(false, std::memory_order_release); }
		void Enable() { bIsEnabled.store(true, std::memory_order_release); }

		// Unsafe read value hash from input
		virtual uint32 ReadValueHash(const int32 Index) = 0;

		// Unsafe read value hash from output
		virtual uint32 GetValueHash(const int32 Index) = 0;

		// Unsafe read value hash from output
		virtual int32 GetNumValues(const EIOSide InSide = EIOSide::In) = 0;

		const FPCGMetadataAttributeBase* InAttribute = nullptr;
		FPCGMetadataAttributeBase* OutAttribute = nullptr;

		int32 BufferIndex = -1;
		const TSharedRef<FPointIO> Source;

		IBuffer(const TSharedRef<FPointIO>& InSource, const FPCGAttributeIdentifier& InIdentifier);

		FORCEINLINE uint64 GetUID() const { return UID; }
		FORCEINLINE EPCGMetadataTypes GetType() const { return Type; }
		FORCEINLINE EDomainType GetUnderlyingDomain() const { return UnderlyingDomain; }

		template <typename T>
		bool IsA() const;

		virtual ~IBuffer();

		virtual bool EnsureReadable() = 0;
		virtual void Write(const bool bEnsureValidKeys = true) = 0;

		virtual void Fetch(const PCGExMT::FScope& Scope)
		{
		}

		virtual bool IsSparse() const { return false; }

		virtual bool IsWritable() = 0;
		virtual bool IsReadable() = 0;
		virtual bool ReadsFromOutput() = 0;

		virtual void Flush()
		{
		}

	protected:
		void SetType(const EPCGMetadataTypes InType);
	};

#define PCGEX_TPL(_TYPE, _NAME, ...) \
extern template bool IBuffer::IsA<_TYPE>() const;
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

	template <typename T>
	class TBuffer : public IBuffer
	{
		friend class FFacade;

	protected:
		const FPCGMetadataAttribute<T>* TypedInAttribute = nullptr;
		FPCGMetadataAttribute<T>* TypedOutAttribute = nullptr;

	public:
		T Min = T{};
		T Max = T{};

		TBuffer(const TSharedRef<FPointIO>& InSource, const FPCGAttributeIdentifier& InIdentifier);

		const FPCGMetadataAttribute<T>* GetTypedInAttribute() const;
		FPCGMetadataAttribute<T>* GetTypedOutAttribute() const;

		// Unsafe read from input
		virtual const T& Read(const int32 Index) const = 0;

		// Unsafe read from output
		virtual const T& GetValue(const int32 Index) = 0;

		// Unsafe read value hash from input
		virtual uint32 ReadValueHash(const int32 Index) override;

		// Unsafe read value hash from output
		virtual uint32 GetValueHash(const int32 Index) override;

		// Unsafe set value in output
		virtual void SetValue(const int32 Index, const T& Value) = 0;

		virtual bool InitForRead(const EIOSide InSide = EIOSide::In, const bool bScoped = false) = 0;
		virtual bool InitForBroadcast(const FPCGAttributePropertyInputSelector& InSelector, const bool bCaptureMinMax = false, const bool bScoped = false) = 0;
		virtual bool InitForWrite(const T& DefaultValue, bool bAllowInterpolation, EBufferInit Init = EBufferInit::Inherit) = 0;
		virtual bool InitForWrite(const EBufferInit Init = EBufferInit::Inherit) = 0;

		void DumpValues(TArray<T>& OutValues) const;
		void DumpValues(const TSharedPtr<TArray<T>>& OutValues) const;
	};

#define PCGEX_USING_TBUFFER \
	friend class FFacade;\
	using TBuffer<T>::BufferLock;\
	using TBuffer<T>::Source;\
	using TBuffer<T>::Identifier;\
	using TBuffer<T>::InAttribute;\
	using TBuffer<T>::TypedInAttribute;\
	using TBuffer<T>::OutAttribute;\
	using TBuffer<T>::TypedOutAttribute;\
	using TBuffer<T>::bReadComplete;\
	using TBuffer<T>::IsEnabled;

	template <typename T>
	class TArrayBuffer : public TBuffer<T>
	{
		PCGEX_USING_TBUFFER

	protected:
		// Used to read from an attribute as another type
		TSharedPtr<PCGEx::TAttributeBroadcaster<T>> InternalBroadcaster;
		bool bSparseBuffer = false;

		TSharedPtr<TArray<T>> InValues;
		TSharedPtr<TArray<T>> OutValues;

	public:
		TArrayBuffer(const TSharedRef<FPointIO>& InSource, const FPCGAttributeIdentifier& InIdentifier);

		virtual bool IsSparse() const override { return bSparseBuffer || InternalBroadcaster; }

		TSharedPtr<TArray<T>> GetInValues();
		TSharedPtr<TArray<T>> GetOutValues();

		virtual int32 GetNumValues(const EIOSide InSide) override;

		virtual bool IsWritable() override;
		virtual bool IsReadable() override;
		virtual bool ReadsFromOutput() override;

		virtual const T& Read(const int32 Index) const override;
		virtual const T& GetValue(const int32 Index) override;
		virtual void SetValue(const int32 Index, const T& Value) override;

	protected:
		virtual void InitForReadInternal(const bool bScoped, const FPCGMetadataAttributeBase* Attribute);
		virtual void InitForWriteInternal(FPCGMetadataAttributeBase* Attribute, const T& InDefaultValue, const EBufferInit Init);

	public:
		virtual bool EnsureReadable() override;

		virtual bool InitForRead(const EIOSide InSide = EIOSide::In, const bool bScoped = false) override;
		virtual bool InitForBroadcast(const FPCGAttributePropertyInputSelector& InSelector, const bool bCaptureMinMax = false, const bool bScoped = false) override;

		virtual bool InitForWrite(const T& DefaultValue, bool bAllowInterpolation, EBufferInit Init = EBufferInit::Inherit) override;
		virtual bool InitForWrite(const EBufferInit Init = EBufferInit::Inherit) override;
		virtual void Write(const bool bEnsureValidKeys = true) override;

		virtual void Fetch(const PCGExMT::FScope& Scope) override;

		virtual void Flush() override;
	};

	template <typename T>
	class TSingleValueBuffer : public TBuffer<T>
	{
		PCGEX_USING_TBUFFER

	protected:
		bool bReadInitialized = false;
		bool bWriteInitialized = false;
		bool bReadFromOutput = false;

		T InValue = T{};
		T OutValue = T{};

	public:
		virtual int32 GetNumValues(const EIOSide InSide) override;

		virtual bool EnsureReadable() override;

		TSingleValueBuffer(const TSharedRef<FPointIO>& InSource, const FPCGAttributeIdentifier& InIdentifier);

		virtual bool IsWritable() override;
		virtual bool IsReadable() override;
		virtual bool ReadsFromOutput() override;

		virtual const T& Read(const int32 Index) const override;
		virtual const T& GetValue(const int32 Index) override;
		virtual void SetValue(const int32 Index, const T& Value) override;

		virtual bool InitForRead(const EIOSide InSide = EIOSide::In, const bool bScoped = false) override;
		virtual bool InitForBroadcast(const FPCGAttributePropertyInputSelector& InSelector, const bool bCaptureMinMax = false, const bool bScoped = false) override;

		virtual bool InitForWrite(const T& DefaultValue, bool bAllowInterpolation, EBufferInit Init = EBufferInit::Inherit) override;
		virtual bool InitForWrite(const EBufferInit Init = EBufferInit::Inherit) override;
		virtual void Write(const bool bEnsureValidKeys = true) override;
	};

#pragma region externalization

#define PCGEX_TPL(_TYPE, _NAME, ...)\
extern template class TBuffer<_TYPE>;\
extern template class TArrayBuffer<_TYPE>;\
extern template class TSingleValueBuffer<_TYPE>;

	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)

#undef PCGEX_TPL

#pragma endregion

	class PCGEXTENDEDTOOLKIT_API FFacade : public TSharedFromThis<FFacade>
	{
		mutable FRWLock BufferLock;
		mutable FRWLock CloudLock;

	public:
		int32 Idx = -1;

		TSharedRef<FPointIO> Source;
		TArray<TSharedPtr<IBuffer>> Buffers;
		TMap<uint64, TSharedPtr<IBuffer>> BufferMap;
		TSharedPtr<PCGExGeo::FPointBoxCloud> Cloud;

		TMap<FName, FName> WritableRemap; // TODO : Manage remapping in the facade directly to remove the need for dummy attributes

		bool bSupportsScopedGet = false;

		int32 GetNum(const EIOSide InSide = EIOSide::In) const;

		TSharedPtr<IBuffer> FindBuffer_Unsafe(const uint64 UID);
		TSharedPtr<IBuffer> FindBuffer(const uint64 UID);
		TSharedPtr<IBuffer> FindReadableAttributeBuffer(const FPCGAttributeIdentifier& InIdentifier);
		TSharedPtr<IBuffer> FindWritableAttributeBuffer(const FPCGAttributeIdentifier& InIdentifier);

		EPCGPointNativeProperties GetAllocations() const;

		FPCGExContext* GetContext() const;

		explicit FFacade(const TSharedRef<FPointIO>& InSource);
		~FFacade() = default;

		bool IsDataValid(const EIOSide InSide) const;
		bool ShareSource(const FFacade* OtherManager) const;

		template <typename T>
		TSharedPtr<TBuffer<T>> FindBuffer_Unsafe(const FPCGAttributeIdentifier& InIdentifier);

		template <typename T>
		TSharedPtr<TBuffer<T>> FindBuffer(const FPCGAttributeIdentifier& InIdentifier);

		template <typename T>
		TSharedPtr<TBuffer<T>> GetBuffer(const FPCGAttributeIdentifier& InIdentifier);


#pragma region Writable

		template <typename T>
		TSharedPtr<TBuffer<T>> GetWritable(const FPCGAttributeIdentifier& InIdentifier, T DefaultValue, bool bAllowInterpolation, EBufferInit Init);

		template <typename T>
		TSharedPtr<TBuffer<T>> GetWritable(const FPCGMetadataAttribute<T>* InAttribute, EBufferInit Init);

		template <typename T>
		TSharedPtr<TBuffer<T>> GetWritable(const FPCGAttributeIdentifier& InIdentifier, EBufferInit Init);

		TSharedPtr<IBuffer> GetWritable(EPCGMetadataTypes Type, const FPCGMetadataAttributeBase* InAttribute, EBufferInit Init);
		TSharedPtr<IBuffer> GetWritable(EPCGMetadataTypes Type, const FName InName, EBufferInit Init);

#pragma endregion

#pragma region Readable

		template <typename T>
		TSharedPtr<TBuffer<T>> GetReadable(const FPCGAttributeIdentifier& InIdentifier, const EIOSide InSide = EIOSide::In, const bool bSupportScoped = false);

		TSharedPtr<IBuffer> GetReadable(const PCGEx::FAttributeIdentity& Identity, const EIOSide InSide = EIOSide::In, const bool bSupportScoped = false);

		TSharedPtr<IBuffer> GetDefaultReadable(const FPCGAttributeIdentifier& InIdentifier, const EIOSide InSide = EIOSide::In, const bool bSupportScoped = false);

#pragma endregion

#pragma region Broadcasters

		template <typename T>
		TSharedPtr<TBuffer<T>> GetBroadcaster(const FPCGAttributePropertyInputSelector& InSelector, const bool bSupportScoped = false, const bool bCaptureMinMax = false);

		template <typename T>
		TSharedPtr<TBuffer<T>> GetBroadcaster(const FName InName, const bool bSupportScoped = false, const bool bCaptureMinMax = false);

#pragma endregion

		FPCGMetadataAttributeBase* FindMutableAttribute(const FPCGAttributeIdentifier& InIdentifier, const EIOSide InSide = EIOSide::In) const;

		const FPCGMetadataAttributeBase* FindConstAttribute(const FPCGAttributeIdentifier& InIdentifier, const EIOSide InSide = EIOSide::In) const;

		template <typename T>
		FPCGMetadataAttribute<T>* FindMutableAttribute(const FPCGAttributeIdentifier& InIdentifier, const EIOSide InSide = EIOSide::In) const;

		template <typename T>
		const FPCGMetadataAttribute<T>* FindConstAttribute(const FPCGAttributeIdentifier& InIdentifier, const EIOSide InSide = EIOSide::In) const;

		TSharedPtr<PCGExGeo::FPointBoxCloud> GetCloud(const EPCGExPointBoundsSource BoundsSource, const double Expansion = DBL_EPSILON);

		const UPCGBasePointData* GetData(const EIOSide InSide) const;
		const UPCGBasePointData* GetIn() const;
		UPCGBasePointData* GetOut() const;

		void CreateReadables(const TArray<PCGEx::FAttributeIdentity>& Identities, const bool bWantsScoped = true);
		void MarkCurrentBuffersReadAsComplete();

		void Flush();

		void Write(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const bool bEnsureValidKeys = true);
		FPlatformTypes::int32 WriteBuffersAsCallbacks(const TSharedPtr<PCGExMT::FTaskGroup>& TaskGroup);
		void WriteBuffers(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, PCGExMT::FCompletionCallback&& Callback);
		int32 WriteSynchronous(const bool bEnsureValidKeys = true);
		void WriteFastest(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const bool bEnsureValidKeys = true);

		void Fetch(const PCGExMT::FScope& Scope);

		FConstPoint GetInPoint(const int32 Index) const;
		FMutablePoint GetOutPoint(const int32 Index) const;

		FScope GetInScope(const int32 Start, const int32 Count, const bool bInclusive = true) const;
		FScope GetInScope(const PCGExMT::FScope& Scope) const;
		FScope GetInFullScope() const;
		FScope GetInRange(const int32 Start, const int32 End, const bool bInclusive = true) const;

		FScope GetOutScope(const int32 Start, const int32 Count, const bool bInclusive = true) const;
		FScope GetOutScope(const PCGExMT::FScope& Scope) const;
		FScope GetOutFullScope() const;
		FScope GetOutRange(const int32 Start, const int32 End, const bool bInclusive = true) const;

	protected:
		template <typename Func>
		void ForEachWritable(Func&& Callback)
		{
			for (int i = 0; i < Buffers.Num(); i++)
			{
				const TSharedPtr<IBuffer> Buffer = Buffers[i];
				if (!Buffer.IsValid() || !Buffer->IsWritable() || !Buffer->IsEnabled()) { continue; }
				Callback(Buffer);
			}
		}

		bool ValidateOutputsBeforeWriting() const;
		void Flush(const TSharedPtr<IBuffer>& Buffer);
	};

#define PCGEX_TPL(_TYPE, _NAME, ...) \
extern template TSharedPtr<TBuffer<_TYPE>> FFacade::FindBuffer_Unsafe<_TYPE>(const FPCGAttributeIdentifier& InIdentifier); \
extern template TSharedPtr<TBuffer<_TYPE>> FFacade::FindBuffer<_TYPE>(const FPCGAttributeIdentifier& InIdentifier); \
extern template TSharedPtr<TBuffer<_TYPE>> FFacade::GetBuffer<_TYPE>(const FPCGAttributeIdentifier& InIdentifier); \
extern template TSharedPtr<TBuffer<_TYPE>> FFacade::GetWritable<_TYPE>(const FPCGAttributeIdentifier& InIdentifier, _TYPE DefaultValue, bool bAllowInterpolation, EBufferInit Init); \
extern template TSharedPtr<TBuffer<_TYPE>> FFacade::GetWritable<_TYPE>(const FPCGMetadataAttribute<_TYPE>* InAttribute, EBufferInit Init); \
extern template TSharedPtr<TBuffer<_TYPE>> FFacade::GetWritable<_TYPE>(const FPCGAttributeIdentifier& InIdentifier, EBufferInit Init); \
extern template TSharedPtr<TBuffer<_TYPE>> FFacade::GetReadable<_TYPE>(const FPCGAttributeIdentifier& InIdentifier, const EIOSide InSide, const bool bSupportScoped); \
extern template TSharedPtr<TBuffer<_TYPE>> FFacade::GetBroadcaster<_TYPE>(const FPCGAttributePropertyInputSelector& InSelector, const bool bSupportScoped, const bool bCaptureMinMax); \
extern template TSharedPtr<TBuffer<_TYPE>> FFacade::GetBroadcaster<_TYPE>(const FName InName, const bool bSupportScoped, const bool bCaptureMinMax); \
extern template FPCGMetadataAttribute<_TYPE>* FFacade::FindMutableAttribute<_TYPE>(const FPCGAttributeIdentifier& InIdentifier, const EIOSide InSide) const; \
extern template const FPCGMetadataAttribute<_TYPE>* FFacade::FindConstAttribute<_TYPE>(const FPCGAttributeIdentifier& InIdentifier, const EIOSide InSide) const;
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion

#pragma region Data Marking

	template <typename T>
	FPCGMetadataAttribute<T>* WriteMark(UPCGData* InData, const FPCGAttributeIdentifier& MarkID, T MarkValue);

	template <typename T>
	FPCGMetadataAttribute<T>* WriteMark(const TSharedRef<FPointIO>& PointIO, const FName MarkID, T MarkValue);

	template <typename T>
	bool TryReadMark(UPCGMetadata* Metadata, const FPCGAttributeIdentifier& MarkID, T& OutMark);

	template <typename T>
	bool TryReadMark(const TSharedRef<FPointIO>& PointIO, const FName MarkID, T& OutMark);

#define PCGEX_TPL(_TYPE, _NAME, ...)\
extern template FPCGMetadataAttribute<_TYPE>* WriteMark(UPCGData* InData, const FPCGAttributeIdentifier& MarkID, _TYPE MarkValue); \
extern template FPCGMetadataAttribute<_TYPE>* WriteMark(const TSharedRef<FPointIO>& PointIO, const FName MarkID, _TYPE MarkValue); \
extern template bool TryReadMark<_TYPE>(UPCGMetadata* Metadata, const FPCGAttributeIdentifier& MarkID, _TYPE& OutMark); \
extern template bool TryReadMark<_TYPE>(const TSharedRef<FPointIO>& PointIO, const FName MarkID, _TYPE& OutMark);
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)

#undef PCGEX_TPL

	PCGEXTENDEDTOOLKIT_API
	void WriteId(const TSharedRef<FPointIO>& PointIO, const FName IdName, const int64 Id);

	PCGEXTENDEDTOOLKIT_API
	UPCGBasePointData* GetMutablePointData(FPCGContext* Context, const FPCGTaggedData& Source);

#pragma endregion

	TSharedPtr<FFacade> TryGetSingleFacade(FPCGExContext* InContext, const FName InputPinLabel, bool bTransactional, const bool bThrowError);
	bool TryGetFacades(FPCGExContext* InContext, const FName InputPinLabel, TArray<TSharedPtr<FFacade>>& OutFacades, const bool bThrowError, const bool bIsTransactional = false);

	class PCGEXTENDEDTOOLKIT_API FWriteBufferTask final : public PCGExMT::FTask
	{
	public:
		PCGEX_ASYNC_TASK_NAME(FWriteTask)

		explicit FWriteBufferTask(const TSharedPtr<IBuffer>& InBuffer, const bool InEnsureValidKeys = true)
			: FTask(), bEnsureValidKeys(InEnsureValidKeys), Buffer(InBuffer)
		{
		}

		bool bEnsureValidKeys = true;
		TSharedPtr<IBuffer> Buffer;

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};

	PCGEXTENDEDTOOLKIT_API
	void WriteBuffer(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const TSharedPtr<IBuffer>& InBuffer, const bool InEnsureValidKeys = true);
}
