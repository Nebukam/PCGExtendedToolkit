// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExH.h"

struct FPCGContext;
class UPCGParamData;
class UPCGExValencyBondingRules;
class UPCGExMeshCollection;
class UPCGExActorCollection;

/**
 * Valency Map: hash encoding + packer/unpacker for the Valency pipeline.
 *
 * Mirrors the PCGExCollections FPickPacker/FPickUnpacker architecture.
 * A single int64 attribute per point encodes (BondingRules identity + ModuleIndex + PatternFlags).
 * A UPCGParamData "Valency Map" carries BondingRules asset paths, mergeable from multiple sources.
 *
 * Hash encoding:
 *   uint64 = H64( H32(BaseHash, BondingRulesMapIdx), H32(ModuleIndex, PatternFlags) )
 *
 * Extraction:
 *   H64A(hash) -> BondingRulesId (upper 32)
 *   H32A(H64B(hash)) -> ModuleIndex (uint16)
 *   H32B(H64B(hash)) -> PatternFlags (uint16)
 */
namespace PCGExValency
{
	/**
	 * Entry data encoding/decoding helpers.
	 * Packs BondingRules identity + module index + pattern flags into a single uint64.
	 *
	 * IMPORTANT: INVALID_ENTRY = 0. Because H64/H32 are simple bit shifts, decoding 0 yields
	 * all-zero fields (BondingRulesMapId=0, ModuleIndex=0, PatternFlags=0) — all potentially valid.
	 * Extraction functions guard against this: GetModuleIndex returns -1 for INVALID_ENTRY.
	 * Callers should also check INVALID_ENTRY before calling extraction functions where possible.
	 */
	namespace EntryData
	{
		/** Pattern flags (stored in lower 16 bits of the lower 32 bits) */
		namespace Flags
		{
			constexpr uint16 None = 0;
			constexpr uint16 Consumed = 1 << 0;    // Point consumed by pattern (Remove/Fork/Collapse)
			constexpr uint16 Swapped = 1 << 1;     // Module index was swapped by pattern
			constexpr uint16 Collapsed = 1 << 2;   // This is the collapsed point (kept, transform updated)
			constexpr uint16 Annotated = 1 << 3;   // Point was annotated by pattern (no removal)
		}

		/** Invalid entry sentinel. IMPORTANT: 0 is the sentinel — see note above. */
		constexpr uint64 INVALID_ENTRY = 0;

		/** Check if a ValencyEntry hash is valid (not the INVALID_ENTRY sentinel) */
		FORCEINLINE bool IsValid(uint64 Hash) { return Hash != INVALID_ENTRY; }

		/** Pack BondingRules map ID + module index + flags into uint64 */
		FORCEINLINE uint64 Pack(uint32 BondingRulesMapId, uint16 ModuleIndex, uint16 PatternFlags = 0)
		{
			return PCGEx::H64(BondingRulesMapId, PCGEx::H32(ModuleIndex, PatternFlags));
		}

		/** Extract BondingRules map ID from hash (upper 32 bits). Returns MAX_uint32 for INVALID_ENTRY. */
		FORCEINLINE uint32 GetBondingRulesMapId(uint64 Hash)
		{
			if (Hash == INVALID_ENTRY) { return MAX_uint32; }
			return PCGEx::H64A(Hash);
		}

		/** Extract module index from hash. Returns -1 for INVALID_ENTRY. */
		FORCEINLINE int32 GetModuleIndex(uint64 Hash)
		{
			if (Hash == INVALID_ENTRY) { return -1; }
			return static_cast<int32>(PCGEx::H32A(PCGEx::H64B(Hash)));
		}

		/** Extract pattern flags from hash. Returns Flags::None for INVALID_ENTRY. */
		FORCEINLINE uint16 GetPatternFlags(uint64 Hash)
		{
			if (Hash == INVALID_ENTRY) { return Flags::None; }
			return PCGEx::H32B(PCGEx::H64B(Hash));
		}

		/** Replace pattern flags in hash, preserving BondingRules ID and module index. Returns INVALID_ENTRY if Hash is invalid. */
		FORCEINLINE uint64 SetPatternFlags(uint64 Hash, uint16 NewFlags)
		{
			if (Hash == INVALID_ENTRY) { return INVALID_ENTRY; }
			return Pack(GetBondingRulesMapId(Hash), static_cast<uint16>(GetModuleIndex(Hash)), NewFlags);
		}

		/** Set a single flag on the hash. Safe on INVALID_ENTRY (returns INVALID_ENTRY). */
		FORCEINLINE uint64 SetFlag(uint64 Hash, uint16 Flag)
		{
			return SetPatternFlags(Hash, GetPatternFlags(Hash) | Flag);
		}

		/** Clear a single flag on the hash. Safe on INVALID_ENTRY (returns INVALID_ENTRY). */
		FORCEINLINE uint64 ClearFlag(uint64 Hash, uint16 Flag)
		{
			return SetPatternFlags(Hash, GetPatternFlags(Hash) & ~Flag);
		}

		/** Check if a specific flag is set. Returns false for INVALID_ENTRY. */
		FORCEINLINE bool HasFlag(uint64 Hash, uint16 Flag)
		{
			return (GetPatternFlags(Hash) & Flag) != 0;
		}

		/** Get the per-point attribute name for a given suffix: "PCGEx/V/Entry/{Suffix}" */
		static FName GetEntryAttributeName(const FName& Suffix)
		{
			return FName(FString::Printf(TEXT("PCGEx/V/Entry/%s"), *Suffix.ToString()));
		}
	}

	/**
	 * Serializes BondingRules references and ValencyEntry hashes into a UPCGParamData
	 * attribute set (the "Valency Map"). This is the bridge between solve/generative nodes
	 * and downstream consumption nodes (Valency : Staging, Write Properties, etc.).
	 *
	 * Thread-safe: GetEntryIdx() can be called from parallel ProcessPoints loops.
	 *
	 * Usage:
	 *   // In Boot:
	 *   Packer = MakeShared<FValencyPacker>(Context);
	 *   // In ProcessPoints (parallel):
	 *   uint64 Hash = Packer->GetEntryIdx(BondingRules, ModuleIndex, PatternFlags);
	 *   ValencyEntryWriter->SetValue(Index, Hash);
	 *   // After processing:
	 *   UPCGParamData* OutputSet = Context->ManagedObjects->New<UPCGParamData>();
	 *   Packer->PackToDataset(OutputSet);
	 */
	class PCGEXELEMENTSVALENCY_API FValencyPacker : public TSharedFromThis<FValencyPacker>
	{
		TArray<const UPCGExValencyBondingRules*> BondingRulesArray;
		TMap<const UPCGExValencyBondingRules*, uint32> BondingRulesMap;
		mutable FRWLock BondingRulesLock;

		uint16 BaseHash = 0;

	public:
		explicit FValencyPacker(FPCGContext* InContext);

		/**
		 * Get a packed ValencyEntry hash for a module.
		 * Thread-safe via FRWLock double-check pattern.
		 * @param InBondingRules The bonding rules asset this module belongs to
		 * @param InModuleIndex Module index within the bonding rules (uint16, max 65535)
		 * @param InPatternFlags Optional pattern flags
		 * @return Packed 64-bit ValencyEntry hash
		 */
		uint64 GetEntryIdx(const UPCGExValencyBondingRules* InBondingRules, uint16 InModuleIndex, uint16 InPatternFlags = 0);

		/** Write BondingRules mapping to an attribute set (the Valency Map) */
		void PackToDataset(const UPCGParamData* InAttributeSet);
	};

	/**
	 * Deserializes a Valency Map (produced by FValencyPacker) back into usable
	 * BondingRules references. Loads the referenced assets, then resolves per-point
	 * hashes into concrete BondingRules + module index + flags.
	 *
	 * Usage:
	 *   // In Boot:
	 *   Unpacker = MakeShared<FValencyUnpacker>();
	 *   Unpacker->UnpackPin(Context, Labels::SourceValencyMapLabel);
	 *   if (!Unpacker->HasValidMapping()) { return false; }
	 *   // In ProcessPoints:
	 *   uint16 ModuleIndex, PatternFlags;
	 *   UPCGExValencyBondingRules* Rules = Unpacker->ResolveEntry(Hash, ModuleIndex, PatternFlags);
	 */
	class PCGEXELEMENTSVALENCY_API FValencyUnpacker : public TSharedFromThis<FValencyUnpacker>
	{
	protected:
		TMap<uint32, UPCGExValencyBondingRules*> BondingRulesMap;
		TSharedPtr<struct FStreamableHandle> BondingRulesHandle;

	public:
		FValencyUnpacker() = default;
		~FValencyUnpacker();

		bool HasValidMapping() const { return !BondingRulesMap.IsEmpty(); }

		/** Get read-only access to the BondingRules map */
		const TMap<uint32, UPCGExValencyBondingRules*>& GetBondingRules() const { return BondingRulesMap; }

		/** Unpack BondingRules mappings from an attribute set */
		bool UnpackDataset(FPCGContext* InContext, const UPCGParamData* InAttributeSet);

		/** Unpack from a specific input pin (merges multiple param data sources) */
		void UnpackPin(FPCGContext* InContext, FName InPinLabel = NAME_None);

		/**
		 * Resolve a ValencyEntry hash to BondingRules + module index + flags.
		 * @param EntryHash The packed ValencyEntry hash
		 * @param OutModuleIndex Output: module index within the bonding rules
		 * @param OutPatternFlags Output: pattern flags
		 * @return The BondingRules asset, or nullptr if not found
		 */
		UPCGExValencyBondingRules* ResolveEntry(uint64 EntryHash, uint16& OutModuleIndex, uint16& OutPatternFlags);

		/**
		 * Get the MeshCollection for a given BondingRules (cached on first access).
		 * @param InBondingRules The bonding rules to get collection from
		 * @return The mesh collection, or nullptr
		 */
		UPCGExMeshCollection* GetMeshCollection(const UPCGExValencyBondingRules* InBondingRules);

		/**
		 * Get the ActorCollection for a given BondingRules (cached on first access).
		 * @param InBondingRules The bonding rules to get collection from
		 * @return The actor collection, or nullptr
		 */
		UPCGExActorCollection* GetActorCollection(const UPCGExValencyBondingRules* InBondingRules);
	};
}
