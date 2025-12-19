// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include <functional>
#include "CoreMinimal.h"

namespace PCGExMT
{
	using FExecuteCallback = std::function<void()>;
	using FCompletionCallback = std::function<void()>;
	using FEndCallback = std::function<void(const bool)>;
	using FSimpleCallback = std::function<void()>;


	struct FScope
	{
		int32 Start = -1;
		int32 Count = -1;
		int32 End = -1;
		int32 LoopIndex = -1;

		FScope() = default;

		FScope(const int32 InStart, const int32 InCount, const int32 InLoopIndex = -1);

		~FScope() = default;
		bool IsValid() const { return Start != -1 && Count > 0; }
		int32 GetNextScopeIndex() const { return LoopIndex + 1; }
		void GetIndices(TArray<int32>& OutIndices) const;

		static int32 GetMaxRange(const TArray<FScope>& InScopes)
		{
			int32 MaxRange = 0;
			for (const FScope& S : InScopes) { MaxRange = FMath::Max(MaxRange, S.Count); }
			return MaxRange;
		}

		template <typename T>
		FORCEINLINE TArrayView<T> GetView(TArray<T>& InArray) const { return TArrayView<T>(InArray.GetData() + Start, Count); }

		template <typename T>
		FORCEINLINE TArrayView<const T> GetView(const TArray<T>& InArray) const { return TArrayView<T>(InArray.GetData() + Start, Count); }
	};
}
