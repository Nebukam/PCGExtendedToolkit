// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGeoPrimtives.h"

namespace PCGExGeo
{
	/// Used to effectively store vertices beyond.
	template <int DIMENSIONS>
	class TVertexBuffer
	{
	public:
		TArray<TFVtx<DIMENSIONS>*> Items;

		TVertexBuffer()
		{
		}

		explicit TVertexBuffer(int32 InCapacity)
		{
			Items.Reserve(InCapacity);
		}

		~TVertexBuffer() { Items.Empty(); }

		bool IsEmpty() { return Items.IsEmpty(); }
		int32 Num() { return Items.Num(); }

		TFVtx<DIMENSIONS>* operator[](int32 Index) const { return Items[Index]; }

		/// Adds a vertex to the buffer.
		void Add(TFVtx<DIMENSIONS>* Vtx) { Items.Add(Vtx); }

		/// Sets the Count to 0, otherwise does nothing.
		void Clear() { Items.Empty(); }
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

		/// Adds the element to the beginning.
		void AddFirst(TSimplexConnector<DIMENSIONS>* Connector)
		{
			First->Previous = Connector;
			Connector->Next = First;
			First = Connector;
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
			//TODO: Destroy
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
	class TObjectManager
	{
	public:
		TQueue<TSimplexWrap<DIMENSIONS>*> RecycledFaceStack;
		TQueue<TSimplexConnector<DIMENSIONS>*> ConnectorStack;
		TQueue<TVertexBuffer<DIMENSIONS>*> EmptyBufferStack;
		TQueue<TDeferredSimplex<DIMENSIONS>*> DeferredSimplexStack;

		TObjectManager()
		{
		}

		~TObjectManager()
		{
			TSimplexWrap<DIMENSIONS>* OutFace = nullptr;
			while (RecycledFaceStack.Dequeue(OutFace)) { delete OutFace; }

			TSimplexConnector<DIMENSIONS>* OutConnector = nullptr;
			while (ConnectorStack.Dequeue(OutConnector)) { delete OutConnector; }

			TVertexBuffer<DIMENSIONS>* OutBuffer = nullptr;
			while (EmptyBufferStack.Dequeue(OutBuffer)) { delete OutBuffer; }

			TDeferredSimplex<DIMENSIONS>* OutDeferredSimplex = nullptr;
			while (DeferredSimplexStack.Dequeue(OutDeferredSimplex)) { delete OutDeferredSimplex; }
		}

		void Clear()
		{
			//TODO: Flush memory and delete
			RecycledFaceStack.Empty();
			ConnectorStack.Empty();
			EmptyBufferStack.Empty();
			DeferredSimplexStack.Empty();
		}

		void DepositFace(TSimplexWrap<DIMENSIONS>* Face)
		{
			Face->Clear();
			RecycledFaceStack.Enqueue(Face);
		}

		TSimplexWrap<DIMENSIONS>* GetFace()
		{
			if (TSimplexWrap<DIMENSIONS>* OutFace = nullptr;
				RecycledFaceStack.Dequeue(OutFace)) { return OutFace; }
			return new TSimplexWrap<DIMENSIONS>(GetVertexBuffer());
		}

		void DepositConnector(TSimplexConnector<DIMENSIONS>* Connector)
		{
			Connector->Clear();
			ConnectorStack.Enqueue(Connector);
		}

		TSimplexConnector<DIMENSIONS>* GetConnector()
		{
			if (TSimplexConnector<DIMENSIONS>* OutConnector = nullptr;
				ConnectorStack.Dequeue(OutConnector)) { return OutConnector; }
			return new TSimplexConnector<DIMENSIONS>();
		}

		void DepositVertexBuffer(TVertexBuffer<DIMENSIONS>* Buffer)
		{
			Buffer->Clear();
			EmptyBufferStack.Enqueue(Buffer);
		}

		TVertexBuffer<DIMENSIONS>* GetVertexBuffer()
		{
			if (TVertexBuffer<DIMENSIONS>* OutBuffer = nullptr;
				EmptyBufferStack.Dequeue(OutBuffer)) { return OutBuffer; }
			return new TVertexBuffer<DIMENSIONS>();
		}

		void DepositDeferredSimplex(TDeferredSimplex<DIMENSIONS>* Face)
		{
			Face->Clear();
			DeferredSimplexStack.Enqueue(Face);
		}

		TDeferredSimplex<DIMENSIONS>* GetDeferredSimplex()
		{
			if (TDeferredSimplex<DIMENSIONS>* OutDeferredSimplex = nullptr;
				DeferredSimplexStack.Dequeue(OutDeferredSimplex)) { return OutDeferredSimplex; }
			return new TDeferredSimplex<DIMENSIONS>();
		}
	};

	/// Holds all the objects needed to create a convex hull.
	/// Helps keep the Convex hull class clean and could maybe recyle them.
	template <int DIMENSIONS>
	class TObjectBuffer
	{
	public:
		static constexpr int32 CONNECTOR_TABLE_SIZE = 2017;

		double MaxDistance;

		TArray<TFVtx<DIMENSIONS>*> InputVertices;
		TArray<TSimplexWrap<DIMENSIONS>*> ConvexSimplices;

		TFVtx<DIMENSIONS>* CurrentVertex = nullptr;
		TFVtx<DIMENSIONS>* FurthestVertex = nullptr;

		TObjectManager<DIMENSIONS>* ObjectManager = nullptr;

		TSimplexList<DIMENSIONS>* UnprocessedFaces = nullptr;
		TArray<TSimplexWrap<DIMENSIONS>*> AffectedFaceBuffer;
		TQueue<TSimplexWrap<DIMENSIONS>*> TraverseStack;
		TSet<TFVtx<DIMENSIONS>*> SingularVertices;

		TArray<TDeferredSimplex<DIMENSIONS>*> ConeFaceBuffer;

		TSimplexWrap<DIMENSIONS>* UpdateBuffer[DIMENSIONS];
		int32 UpdateIndices[DIMENSIONS];

		ConnectorList<DIMENSIONS>* ConnectorTable[CONNECTOR_TABLE_SIZE];
		TVertexBuffer<DIMENSIONS>* EmptyBuffer = nullptr;
		TVertexBuffer<DIMENSIONS>* BeyondBuffer = nullptr;

		TObjectBuffer()
		{
			for (int i = 0; i < CONNECTOR_TABLE_SIZE; i++) { ConnectorTable[i] = nullptr; }
			Reset();
		}

		void Reset()
		{
			MaxDistance = TNumericLimits<double>::Min();

			InputVertices.Empty();
			ConvexSimplices.Empty();

			PCGEX_DELETE(UnprocessedFaces)
			UnprocessedFaces = new TSimplexList<DIMENSIONS>();
			AffectedFaceBuffer.Empty();
			TraverseStack.Empty();
			SingularVertices.Empty();

			ConeFaceBuffer.Empty();

			for (int i = 0; i < DIMENSIONS; i++)
			{
				UpdateBuffer[i] = nullptr;
				UpdateIndices[i] = -1;
			}

			PCGEX_DELETE(ObjectManager)
			ObjectManager = new TObjectManager<DIMENSIONS>();

			PCGEX_DELETE(EmptyBuffer)
			EmptyBuffer = new TVertexBuffer<DIMENSIONS>();

			PCGEX_DELETE(BeyondBuffer)
			BeyondBuffer = new TVertexBuffer<DIMENSIONS>();

			for (int i = 0; i < CONNECTOR_TABLE_SIZE; i++)
			{
				ConnectorList<DIMENSIONS>* CList = ConnectorTable[i];
				PCGEX_DELETE(CList)
				ConnectorTable[i] = new ConnectorList<DIMENSIONS>();
			}
		}

		void Clear()
		{
			Reset();
		}

		~TObjectBuffer()
		{
			InputVertices.Empty();
			ConvexSimplices.Empty();

			PCGEX_DELETE(UnprocessedFaces)

			AffectedFaceBuffer.Empty();
			TraverseStack.Empty();
			SingularVertices.Empty();

			ConeFaceBuffer.Empty();

			for (int i = 0; i < DIMENSIONS; i++)
			{
				UpdateBuffer[i] = nullptr;
				UpdateIndices[i] = -1;
			}

			PCGEX_DELETE(EmptyBuffer)
			PCGEX_DELETE(BeyondBuffer)

			for (int i = 0; i < CONNECTOR_TABLE_SIZE; i++)
			{
				ConnectorList<DIMENSIONS>* CList = ConnectorTable[i];
				PCGEX_DELETE(CList)
				ConnectorTable[i] = nullptr;
			}

			PCGEX_DELETE(ObjectManager)
		}

		void InitInput(const TArray<TFVtx<DIMENSIONS>*>& Input)
		{
			InputVertices.Empty(Input.Num());
			InputVertices.Append(Input);
		}
	};
}
