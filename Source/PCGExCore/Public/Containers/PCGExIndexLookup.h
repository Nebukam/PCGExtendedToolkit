// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

namespace PCGEx
{
	class FIndexLookup : public TSharedFromThis<FIndexLookup>
	{
	protected:
		TArray<int32> Data;

	public:
		explicit FIndexLookup(const int32 Size)
		{
			Data.Init(-1, Size);
		}

		FIndexLookup(const int32 Size, bool bFill)
		{
			Data.Init(-1, Size);
		}

		FORCEINLINE int32& operator[](const int32 At) { return Data[At]; }
		FORCEINLINE int32 operator[](const int32 At) const { return Data[At]; }
		FORCEINLINE void Set(const int32 At, const int32 Value) { Data[At] = Value; }
		FORCEINLINE int32 Get(const int32 At) { return Data[At]; }
		FORCEINLINE int32& GetMutable(const int32 At) { return Data[At]; }

		operator TArrayView<const int32>() const { return Data; }
		operator TArrayView<int32>() { return Data; }
	};
}
