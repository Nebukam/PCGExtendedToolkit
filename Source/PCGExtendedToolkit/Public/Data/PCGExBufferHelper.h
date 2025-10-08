﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGEx.h"
#include "PCGExData.h"
#include "PCGExtendedToolkit.h"

namespace PCGExData
{
	enum class EBufferHelperMode : uint8
	{
		Write = 0,
		Read  = 1,
	};

	template <EBufferHelperMode Mode = EBufferHelperMode::Write>
	class PCGEXTENDEDTOOLKIT_API TBufferHelper : public TSharedFromThis<TBufferHelper<Mode>>
	{
		TSharedPtr<FFacade> DataFacade;
		TMap<FName, TSharedPtr<IBuffer>> BufferMap;
		mutable FRWLock BufferLock;

	public:
		explicit TBufferHelper(const TSharedRef<FFacade>& InDataFacade):
			DataFacade(InDataFacade)
		{
		}

		template <typename T>
		TSharedPtr<TArrayBuffer<T>> TryGetBuffer(const FName InName)
		{
			FReadScopeLock ReadScopeLock(BufferLock);
			if (TSharedPtr<IBuffer>* BufferPtr = BufferMap.Find(InName))
			{
				if (!(*BufferPtr)->IsA<T>()) { return nullptr; }
				return StaticCastSharedPtr<TArrayBuffer<T>>(*BufferPtr);
			}

			return nullptr;
		}

		template <typename T>
		TSharedPtr<TArrayBuffer<T>> GetBuffer(const FName InName)
		{
			{
				FReadScopeLock ReadScopeLock(BufferLock);
				if (const TSharedPtr<IBuffer>* BufferPtr = BufferMap.Find(InName))
				{
					if (!(*BufferPtr)->IsA<T>())
					{
						UE_LOG(LogPCGEx, Error, TEXT("Attempted to create an attribute that already exist, with a different type (%s)"), *InName.ToString())
						return nullptr;
					}

					return StaticCastSharedPtr<TArrayBuffer<T>>(*BufferPtr);
				}
			}
			{
				FWriteScopeLock WriteScopeLock(BufferLock);
				if (PCGEx::IsPCGExAttribute(InName))
				{
					UE_LOG(LogPCGEx, Error, TEXT("Attempted to create an attribute with a protected prefix (%s)"), *InName.ToString())
					return nullptr;
				}

				TSharedPtr<TArrayBuffer<T>> NewBuffer;

				if constexpr (Mode == EBufferHelperMode::Write)
				{
					NewBuffer = StaticCastSharedPtr<TArrayBuffer<T>>(DataFacade->GetWritable<T>(InName, EBufferInit::Inherit));
				}
				else
				{
					NewBuffer = StaticCastSharedPtr<TArrayBuffer<T>>(DataFacade->GetReadable<T>(InName));
					if (!NewBuffer)
					{
						UE_LOG(LogPCGEx, Error, TEXT("Readable attribute (%s) does not exists."), *InName.ToString())
						return nullptr;
					}
				}

				BufferMap.Add(InName, StaticCastSharedPtr<IBuffer>(NewBuffer));
				return NewBuffer;
			}
		}

		template <typename T>
		TSharedPtr<TArrayBuffer<T>> GetBuffer(const FName InName, const T& DefaultValue)
		{
			{
				FReadScopeLock ReadScopeLock(BufferLock);
				if (const TSharedPtr<IBuffer>* BufferPtr = BufferMap.Find(InName))
				{
					if (!(*BufferPtr)->IsA<T>())
					{
						UE_LOG(LogPCGEx, Error, TEXT("Attempted to create an attribute that already exist, with a different type (%s)"), *InName.ToString())
						return nullptr;
					}

					return StaticCastSharedPtr<TArrayBuffer<T>>(*BufferPtr);
				}
			}
			{
				FWriteScopeLock WriteScopeLock(BufferLock);

				if (PCGEx::IsPCGExAttribute(InName))
				{
					UE_LOG(LogPCGEx, Error, TEXT("Attempted to create an attribute with a protected prefix (%s)"), *InName.ToString())
					return nullptr;
				}

				TSharedPtr<TArrayBuffer<T>> NewBuffer;

				if constexpr (Mode == EBufferHelperMode::Write)
				{
					NewBuffer = StaticCastSharedPtr<TArrayBuffer<T>>(DataFacade->GetWritable<T>(InName, EBufferInit::Inherit));
				}
				else
				{
					NewBuffer = DataFacade->GetReadable<T>(InName);
					if (!NewBuffer)
					{
						UE_LOG(LogPCGEx, Error, TEXT("Readable attribute (%s) does not exists."), *InName.ToString())
						return nullptr;
					}
				}

				BufferMap.Add(InName, StaticCastSharedPtr<IBuffer>(NewBuffer));
				return NewBuffer;
			}
		}

		template <typename T>
		bool SetValue(const FName& InAttributeName, const int32 InIndex, const T& InValue)
		{
			TSharedPtr<TArrayBuffer<T>> Buffer = GetBuffer<T>(InAttributeName);
			if (!Buffer) { return false; }

			if constexpr (Mode == EBufferHelperMode::Write)
			{
				Buffer->SetValue(InIndex, InValue);
			}
			else
			{
				if (Buffer->IsWritable())
				{
					Buffer->SetValue(InIndex, InValue);
				}
				else
				{
					UE_LOG(LogPCGEx, Error, TEXT("Attempting to SET on readable (%s), this is not allowed."), *InAttributeName.ToString())
					return false;
				}
			}

			return true;
		}

		template <typename T>
		bool GetValue(const FName& InAttributeName, const int32 InIndex, T& OutValue)
		{
			TSharedPtr<TArrayBuffer<T>> Buffer = GetBuffer<T>(InAttributeName);
			if (!Buffer) { return false; }

			if constexpr (Mode == EBufferHelperMode::Write)
			{
				OutValue = Buffer->GetValue(InIndex);
			}
			else
			{
				OutValue = Buffer->Read(InIndex);
			}

			return true;
		}
	};
}
