// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGeoPrimtives.h"

namespace PCGExGeo
{
	/// Used to effectively store vertices beyond.
	template <int DIMENSIONS>
	class TVertexBuffer : public TArray<TFVtx<DIMENSIONS>*>
	{
	};

	template <int DIMENSIONS>
	class TSimplexWrap : public TFSimplex<DIMENSIONS>
	{
	public:
		/// Gets or sets the vertices beyond.
		TVertexBuffer<DIMENSIONS>* VerticesBeyond = nullptr;

		/// The furthest vertex.
		TFVtx<DIMENSIONS>* FurthestVertex = nullptr;

		/// Prev node in the list.
		TSimplexWrap* Previous = nullptr;

		/// Next node in the list.
		TSimplexWrap* Next = nullptr;

		/// Is it present in the list.
		bool bInList = false;

		TSimplexWrap* TypedAdjacentFace(int32 Index) { return static_cast<TSimplexWrap*>(this->AdjacentFaces[Index]); }

		explicit TSimplexWrap(TVertexBuffer<DIMENSIONS>* InBeyondList)
		{
			Clear();
			VerticesBeyond = InBeyondList;
		}

		virtual ~TSimplexWrap() override
		{
			PCGEX_DELETE(VerticesBeyond)
		}
		
		void Clear()
		{
			Previous = nullptr;
			Next = nullptr;

			for (int i = 0; i < DIMENSIONS; i++)
			{
				this->AdjacentFaces[i] = nullptr;
				this->Vertices[i] = nullptr;
			}
		}
	};

	/// For deferred face addition.
	template <int DIMENSIONS>
	class TDeferredSimplex
	{
	public:
		TSimplexWrap<DIMENSIONS>* Face = nullptr;
		TSimplexWrap<DIMENSIONS>* Pivot = nullptr;
		TSimplexWrap<DIMENSIONS>* OldFace = nullptr;
		int32 FaceIndex = -1;
		int32 PivotIndex = -1;

		void Clear()
		{
			Face = nullptr;
			Pivot = nullptr;
			OldFace = nullptr;
			FaceIndex = -1;
			PivotIndex = -1;
		}
	};

	/// A helper class used to connect faces.
	template <int DIMENSIONS>
	class TSimplexConnector
	{
	public:
		/// The face.
		TSimplexWrap<DIMENSIONS>* Face = nullptr;

		/// The edge to be connected.
		int32 EdgeIndex = -1;

		/// The vertex indices.
		int32 Vertices[DIMENSIONS - 1];

		/// The hash code computed from indices.
		uint64 HashCode = 0;

		/// Prev node in the list.
		TSimplexConnector* Previous = nullptr;

		/// Next node in the list.
		TSimplexConnector* Next = nullptr;

		TSimplexConnector()
		{
			Clear();
		}

		/// Updates the connector.
		void Update(TSimplexWrap<DIMENSIONS>* InFace, int InEdgeIndex)
		{
			Face = InFace;
			EdgeIndex = InEdgeIndex;

			uint64 TempHashCode = 31;

			for (int i = 0, c = 0; i < DIMENSIONS; i++)
			{
				if (i == InEdgeIndex) { continue; }
				int v = InFace->Vertices[i]->Id;
				Vertices[c++] = v;
				TempHashCode += 23 * TempHashCode + static_cast<uint64>(v);
			}

			HashCode = TempHashCode;
		}

		/// Can two faces be connected.
		static bool AreConnectable(TSimplexConnector* A, TSimplexConnector* B)
		{
			if (A->HashCode != B->HashCode) return false;
			for (int i = 0; i < DIMENSIONS - 1; i++) { if (A->Vertices[i] != B->Vertices[i]) { return false; } }
			return true;
		}

		/// Connect two faces.
		static void Connect(TSimplexConnector* A, TSimplexConnector* B)
		{
			A->Face->AdjacentFaces[A->EdgeIndex] = B->Face;
			B->Face->AdjacentFaces[B->EdgeIndex] = A->Face;
		}

		void Clear()
		{
			Face = nullptr;
			EdgeIndex = -1;
			HashCode = 0;

			Previous = nullptr;
			Next = nullptr;

			for (int i = 0; i < DIMENSIONS - 1; i++) { Vertices[i] = -1; }
		}
	};

	/// Connector list.
	template <int DIMENSIONS>
	class ConnectorList
	{
	public:
		TSimplexConnector<DIMENSIONS>* First = nullptr;
		TSimplexConnector<DIMENSIONS>* Last = nullptr;

		void Clear()
		{
			First = nullptr;
			Last = nullptr;
		}

		/// Adds a face to the list.
		void Add(TSimplexConnector<DIMENSIONS>* Element)
		{
			if (Last) { Last->Next = Element; }
			Element->Previous = Last;
			Last = Element;
			if (!First) { First = Element; }
		}

		/// <summary>
		/// Removes the element from the list.
		/// </summary>
		void Remove(TSimplexConnector<DIMENSIONS>* Connector)
		{
			if (Connector->Previous) { Connector->Previous->Next = Connector->Next; }
			else if (!Connector->Previous) { First = Connector->Next; }

			if (Connector->Next) { Connector->Next->Previous = Connector->Previous; }
			else if (!Connector->Next) { Last = Connector->Previous; }

			Connector->Next = nullptr;
			Connector->Previous = nullptr;
		}
	};

	template <int DIMENSIONS>
	class TSimplexList
	{
	public:
		TSimplexWrap<DIMENSIONS>* First = nullptr;
		TSimplexWrap<DIMENSIONS>* Last = nullptr;

		void Clear()
		{
			First = nullptr;
			Last = nullptr;
		}

		/// Adds the element to the beginning.
		void AddFirst(TSimplexWrap<DIMENSIONS>* Face)
		{
			Face->bInList = true;
			First->Previous = Face;
			Face->Next = First;
			First = Face;
		}

		/// Adds a face to the list.
		void Add(TSimplexWrap<DIMENSIONS>* Face)
		{
			if (Face->bInList)
			{
				if (First->VerticesBeyond->Num() < Face->VerticesBeyond->Num())
				{
					Remove(Face);
					AddFirst(Face);
				}
				return;
			}

			Face->bInList = true;

			if (First && First->VerticesBeyond->Num() < Face->VerticesBeyond->Num())
			{
				First->Previous = Face;
				Face->Next = First;
				First = Face;
			}
			else
			{
				if (Last) { Last->Next = Face; }
				Face->Previous = Last;
				Last = Face;
				if (!First) { First = Face; }
			}
		}

		/// Removes the element from the list.
		void Remove(TSimplexWrap<DIMENSIONS>* Face)
		{
			if (!Face->bInList) { return; }

			Face->bInList = false;

			if (Face->Previous) { Face->Previous->Next = Face->Next; }
			else if (!Face->Previous && Face == First) { First = Face->Next; }

			if (Face->Next) { Face->Next->Previous = Face->Previous; }
			else if (!Face->Next && Last == Face) { Last = Face->Previous; }

			Face->Next = nullptr;
			Face->Previous = nullptr;
		}
	};

	/// A helper class for object allocation/storage. 
	template <int DIMENSIONS>
	class THullObjectsPool
	{
	public:
		TQueue<TSimplexWrap<DIMENSIONS>*> FacesQueue;
		TQueue<TSimplexConnector<DIMENSIONS>*> ConnectorsQueue;
		TQueue<TVertexBuffer<DIMENSIONS>*> EmptyBufferQueue;
		TQueue<TDeferredSimplex<DIMENSIONS>*> DeferredSimplexQueue;

		static constexpr int32 CONNECTOR_TABLE_SIZE = 2017;
		ConnectorList<DIMENSIONS>* ConnectorTable[CONNECTOR_TABLE_SIZE];

		THullObjectsPool()
		{
			for (int i = 0; i < CONNECTOR_TABLE_SIZE; i++){ ConnectorTable[i] = new ConnectorList<DIMENSIONS>(); }
		}

		~THullObjectsPool()
		{
			TSimplexWrap<DIMENSIONS>* OutFace = nullptr;
			while (FacesQueue.Dequeue(OutFace)) { delete OutFace; }

			TSimplexConnector<DIMENSIONS>* OutConnector = nullptr;
			while (ConnectorsQueue.Dequeue(OutConnector)) { delete OutConnector; }

			TVertexBuffer<DIMENSIONS>* OutBuffer = nullptr;
			while (EmptyBufferQueue.Dequeue(OutBuffer)) { delete OutBuffer; }

			TDeferredSimplex<DIMENSIONS>* OutDeferredSimplex = nullptr;
			while (DeferredSimplexQueue.Dequeue(OutDeferredSimplex)) { delete OutDeferredSimplex; }

			for (int i = 0; i < CONNECTOR_TABLE_SIZE; i++){ delete ConnectorTable[i]; }
		}

		void ReturnFace(TSimplexWrap<DIMENSIONS>* Face)
		{
			Face->Clear();
			FacesQueue.Enqueue(Face);
		}

		TSimplexWrap<DIMENSIONS>* GetFace()
		{
			if (TSimplexWrap<DIMENSIONS>* OutFace = nullptr;
				FacesQueue.Dequeue(OutFace)) { return OutFace; }
			return new TSimplexWrap<DIMENSIONS>(GetVertexBuffer());
		}

		void ReturnConnector(TSimplexConnector<DIMENSIONS>* Connector)
		{
			Connector->Clear();
			ConnectorsQueue.Enqueue(Connector);
		}

		TSimplexConnector<DIMENSIONS>* GetConnector()
		{
			if (TSimplexConnector<DIMENSIONS>* OutConnector = nullptr;
				ConnectorsQueue.Dequeue(OutConnector)) { return OutConnector; }
			return new TSimplexConnector<DIMENSIONS>();
		}

		void ReturnVertexBuffer(TVertexBuffer<DIMENSIONS>* Buffer)
		{
			Buffer->Empty();
			EmptyBufferQueue.Enqueue(Buffer);
		}

		TVertexBuffer<DIMENSIONS>* GetVertexBuffer()
		{
			if (TVertexBuffer<DIMENSIONS>* OutBuffer = nullptr;
				EmptyBufferQueue.Dequeue(OutBuffer)) { return OutBuffer; }
			return new TVertexBuffer<DIMENSIONS>();
		}

		void ReturnDeferredSimplex(TDeferredSimplex<DIMENSIONS>* Face)
		{
			Face->Clear();
			DeferredSimplexQueue.Enqueue(Face);
		}

		TDeferredSimplex<DIMENSIONS>* GetDeferredSimplex()
		{
			if (TDeferredSimplex<DIMENSIONS>* OutDeferredSimplex = nullptr;
				DeferredSimplexQueue.Dequeue(OutDeferredSimplex)) { return OutDeferredSimplex; }
			return new TDeferredSimplex<DIMENSIONS>();
		}
	};
	
}
