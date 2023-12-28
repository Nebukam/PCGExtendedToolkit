// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGeoPrimtives.h"

namespace PCGExGeo
{
	/// Used to effectively store vertices beyond.
	template <typename T>
	class TVertexBuffer
	{
	public:
		TArray<T*> Items;

		TVertexBuffer()
		{
		}

		explicit TVertexBuffer(int32 InCapacity)
		{
			Items.Reserve(InCapacity);
		}

		int32 Num() { return Items.Num(); }

		T* operator[](int32 Index) const { return Items[Index]; }

		/// Adds a vertex to the buffer.
		void Add(T* Vtx) { Items.Add(Vtx); }

		/// Sets the Count to 0, otherwise does nothing.
		void Clear() { Items.Empty(); }
	};

	template <int DIMENSIONS, typename VECTOR_TYPE>
	class TSimplexWrap : public TFSimplex<DIMENSIONS, VECTOR_TYPE>
	{
	public:
		/// Gets or sets the vertices beyond.
		TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>* VerticesBeyond = nullptr;

		/// The furthest vertex.
		TFVtx<DIMENSIONS, VECTOR_TYPE>* FurthestVertex = nullptr;

		/// Prev node in the list.
		TSimplexWrap* Previous = nullptr;

		/// Next node in the list.
		TSimplexWrap* Next = nullptr;

		/// Is it present in the list.
		bool bInList = false;

		TSimplexWrap* TypedAdjacentFace(int32 Index) { return static_cast<TSimplexWrap*>(this->AdjacentFaces[Index]); }

		explicit TSimplexWrap(TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>* InBeyondList)
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
	template <typename T>
	class TDeferredSimplex
	{
	public:
		T* Face = nullptr;
		T* Pivot = nullptr;
		T* OldFace = nullptr;
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
	template <int DIMENSIONS, typename VECTOR_TYPE>
	class TSimplexConnector
	{
	public:
		/// The face.
		TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* Face = nullptr;

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
		void Update(TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* InFace, int InEdgeIndex)
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
	template <typename T>
	class ConnectorList
	{
	public:
		T* First = nullptr;
		T* Last = nullptr;

		void Clear()
		{
			First = nullptr;
			Last = nullptr;
		}

		/// Adds the element to the beginning.
		void AddFirst(T* Connector)
		{
			First->Previous = Connector;
			Connector->Next = First;
			First = Connector;
		}

		/// Adds a face to the list.
		void Add(T* Element)
		{
			if (Last) { Last->Next = Element; }
			Element->Previous = Last;
			Last = Element;
			if (!First) { First = Element; }
		}

		/// <summary>
		/// Removes the element from the list.
		/// </summary>
		void Remove(T* Connector)
		{
			if (Connector->Previous) { Connector->Previous->Next = Connector->Next; }
			else if (!Connector->Previous) { First = Connector->Next; }

			if (Connector->Next) { Connector->Next->Previous = Connector->Previous; }
			else if (!Connector->Next) { Last = Connector->Previous; }

			Connector->Next = nullptr;
			Connector->Previous = nullptr;
		}
	};

	template <typename T>
	class TSimplexList
	{
	public:
		T* First = nullptr;
		T* Last = nullptr;

		void Clear()
		{
			//TODO: Destroy
			First = nullptr;
			Last = nullptr;
		}

		/// Adds the element to the beginning.
		void AddFirst(T* Face)
		{
			Face->bInList = true;
			First->Previous = Face;
			Face->Next = First;
			First = Face;
		}

		/// Adds a face to the list.
		void Add(T* Face)
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
		void Remove(T* Face)
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
	template <int DIMENSIONS, typename VECTOR_TYPE>
	class TObjectManager
	{
	public:
		TQueue<TSimplexWrap<DIMENSIONS, VECTOR_TYPE>*> RecycledFaceStack;
		TQueue<TSimplexConnector<DIMENSIONS, VECTOR_TYPE>*> ConnectorStack;
		TQueue<TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>*> EmptyBufferStack;
		TQueue<TDeferredSimplex<TSimplexWrap<DIMENSIONS, VECTOR_TYPE>>*> DeferredSimplexStack;

		TObjectManager()
		{
		}

		~TObjectManager()
		{
		}

		void Clear()
		{
			RecycledFaceStack.Empty();
			ConnectorStack.Clear();
			EmptyBufferStack.Clear();
			DeferredSimplexStack.Clear();
		}

		void DepositFace(TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* Face)
		{
			Face->Clear();
			RecycledFaceStack.Enqueue(Face);
		}

		TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* GetFace()
		{
			if (TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* OutFace = nullptr;
				RecycledFaceStack.Dequeue(OutFace)) { return OutFace; }
			return new TSimplexWrap<DIMENSIONS, VECTOR_TYPE>(GetVertexBuffer());
		}

		void DepositConnector(TSimplexConnector<DIMENSIONS, VECTOR_TYPE>* Connector)
		{
			Connector->Clear();
			ConnectorStack.Enqueue(Connector);
		}

		TSimplexConnector<DIMENSIONS, VECTOR_TYPE>* GetConnector()
		{
			if (TSimplexConnector<DIMENSIONS, VECTOR_TYPE>* OutConnector = nullptr;
				ConnectorStack.Dequeue(OutConnector)) { return OutConnector; }
			return new TSimplexConnector<DIMENSIONS, VECTOR_TYPE>();
		}

		void DepositVertexBuffer(TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>* Buffer)
		{
			Buffer->Clear();
			EmptyBufferStack.Enqueue(Buffer);
		}

		TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>* GetVertexBuffer()
		{
			if (TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>* OutBuffer = nullptr;
				EmptyBufferStack.Dequeue(OutBuffer)) { return OutBuffer; }
			return new TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>();
		}

		void DepositDeferredSimplex(TDeferredSimplex<TSimplexWrap<DIMENSIONS, VECTOR_TYPE>>* Face)
		{
			Face->Clear();
			DeferredSimplexStack.Enqueue(Face);
		}

		TDeferredSimplex<TSimplexWrap<DIMENSIONS, VECTOR_TYPE>>* GetDeferredSimplex()
		{
			if (TDeferredSimplex<TSimplexWrap<DIMENSIONS, VECTOR_TYPE>>* OutDeferredSimplex = nullptr;
				DeferredSimplexStack.Dequeue(OutDeferredSimplex)) { return OutDeferredSimplex; }
			return new TDeferredSimplex<TSimplexWrap<DIMENSIONS, VECTOR_TYPE>>();
		}
	};

	/// Holds all the objects needed to create a convex hull.
	/// Helps keep the Convex hull class clean and could maybe recyle them.
	template <int DIMENSIONS, typename VECTOR_TYPE>
	class TObjectBuffer
	{
	public:
		static constexpr int32 CONNECTOR_TABLE_SIZE = 2017;

		float MaxDistance;

		TArray<TFVtx<DIMENSIONS, VECTOR_TYPE>*> InputVertices;
		TArray<TSimplexWrap<DIMENSIONS, VECTOR_TYPE>*> ConvexSimplexs;

		TFVtx<DIMENSIONS, VECTOR_TYPE>* CurrentVertex = nullptr;
		TFVtx<DIMENSIONS, VECTOR_TYPE>* FurthestVertex = nullptr;

		TObjectManager<DIMENSIONS, VECTOR_TYPE>* ObjectManager = nullptr;

		TSimplexList<TSimplexWrap<DIMENSIONS, VECTOR_TYPE>>* UnprocessedFaces = nullptr;
		TArray<TSimplexWrap<DIMENSIONS, VECTOR_TYPE>*> AffectedFaceBuffer;
		TQueue<TSimplexWrap<DIMENSIONS, VECTOR_TYPE>*> TraverseStack;
		TSet<TFVtx<DIMENSIONS, VECTOR_TYPE>*> SingularVertices;

		TArray<TDeferredSimplex<TSimplexWrap<DIMENSIONS, VECTOR_TYPE>>*> ConeFaceBuffer;

		TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* UpdateBuffer[DIMENSIONS];
		int32 UpdateIndices[DIMENSIONS];

		ConnectorList<TSimplexConnector<DIMENSIONS, VECTOR_TYPE>>* ConnectorTable[CONNECTOR_TABLE_SIZE];
		TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>* EmptyBuffer = nullptr;
		TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>* BeyondBuffer = nullptr;

		TObjectBuffer()
		{
			for (int i = 0; i < CONNECTOR_TABLE_SIZE; i++) { ConnectorTable[i] = nullptr; }
			Reset();
		}

		void Reset()
		{
			MaxDistance = TNumericLimits<double>::Min();

			InputVertices.Empty();
			ConvexSimplexs.Empty();

			PCGEX_DELETE(UnprocessedFaces)
			UnprocessedFaces = new TSimplexList<TSimplexWrap<DIMENSIONS, VECTOR_TYPE>>();
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
			ObjectManager = new TObjectManager<DIMENSIONS, VECTOR_TYPE>();

			PCGEX_DELETE(EmptyBuffer)
			EmptyBuffer = new TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>();

			PCGEX_DELETE(BeyondBuffer)
			BeyondBuffer = new TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>();

			for (int i = 0; i < CONNECTOR_TABLE_SIZE; i++)
			{
				ConnectorList<TSimplexConnector<DIMENSIONS, VECTOR_TYPE>>* CList = ConnectorTable[i];
				PCGEX_DELETE(CList)
				ConnectorTable[i] = new ConnectorList<TSimplexConnector<DIMENSIONS, VECTOR_TYPE>>();
			}
		}

		void Clear()
		{
			Reset();
		}

		~TObjectBuffer()
		{
			InputVertices.Empty();
			ConvexSimplexs.Empty();

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

			PCGEX_DELETE(ObjectManager)
			PCGEX_DELETE(EmptyBuffer)
			PCGEX_DELETE(BeyondBuffer)

			for (int i = 0; i < CONNECTOR_TABLE_SIZE; i++)
			{
				ConnectorList<TSimplexConnector<DIMENSIONS, VECTOR_TYPE>>* CList = ConnectorTable[i];
				PCGEX_DELETE(CList)
				ConnectorTable[i] = nullptr;
			}
		}

		void InitInput(TArray<TFVtx<DIMENSIONS, VECTOR_TYPE>*>& Input)
		{
			InputVertices.Empty(Input.Num());
			InputVertices.Append(Input);
		}
	};
}
