// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

namespace PCGExGeo
{
	template <int DIMENSIONS>
	class PCGEXTENDEDTOOLKIT_API TFVtx
	{
	public:
		double Position[DIMENSIONS];

		int32 Id = -1;
		int32 Tag = 0;
		bool bIsOnHull = false;
		FVector Location = FVector::Zero();

		double& operator[](int32 Component) { return Position[Component]; };
		double operator[](int32 Component) const { return Position[Component]; };

		TFVtx()
		{
		}

		double SqrMagnitude()
		{
			double sum = 0.0f;
			for (int i = 0; i < DIMENSIONS; i++) { sum += Position[i] * Position[i]; }
			return sum;
		}
	};

#pragma region V3 Cast

	template <int DIMENSIONS, typename CompilerSafety = void>
	static FVector GetV3(TFVtx<DIMENSIONS>* Vtx) { return FVector::ZeroVector; }

	template <typename CompilerSafety = void>
	static FVector GetV3(TFVtx<2>* Vtx) { return FVector(Vtx->Position[0], Vtx->Position[1], 0); }

	template <typename CompilerSafety = void>
	static FVector GetV3(TFVtx<3>* Vtx) { return FVector(Vtx->Position[0], Vtx->Position[1], Vtx->Position[2]); }

	template <typename CompilerSafety = void>
	static FVector GetV3(TFVtx<4>* Vtx) { return FVector(Vtx->Position[0], Vtx->Position[1], Vtx->Position[2]); }

	//

	template <int DIMENSIONS, typename CompilerSafety = void>
	static FVector GetV3Downscaled(TFVtx<DIMENSIONS>* Vtx) { return FVector::ZeroVector; }

	template <typename CompilerSafety = void>
	static FVector GetV3Downscaled(TFVtx<2>* Vtx) { return FVector(Vtx->Position[0], Vtx->Position[1], 0); }

	template <typename CompilerSafety = void>
	static FVector GetV3Downscaled(TFVtx<3>* Vtx) { return FVector(Vtx->Position[0], Vtx->Position[1], 0); }

	template <typename CompilerSafety = void>
	static FVector GetV3Downscaled(TFVtx<4>* Vtx) { return FVector(Vtx->Position[0], Vtx->Position[1], Vtx->Position[2]); }

#pragma endregion

	template <int DIMENSIONS>
	struct PCGEXTENDEDTOOLKIT_API FTriangle
	{
		int32 A = -1;
		int32 B = -1;
		int32 C = -1;

		FTriangle(int32 InA, int32 InB, int32 InC)
			: A(InA), B(InB), C(InC)
		{
		}

		void GetLongestEdge(TArray<TFVtx<DIMENSIONS>*>& Vertices, int32& OutStart, int32& OutEnd) const
		{
			//TODO: Refactor this
			const double Lengths[3] = {
				FVector::DistSquared(PCGExGeo::GetV3Downscaled(Vertices[A]), PCGExGeo::GetV3Downscaled(Vertices[B])),
				FVector::DistSquared(PCGExGeo::GetV3Downscaled(Vertices[A]), PCGExGeo::GetV3Downscaled(Vertices[C])),
				FVector::DistSquared(PCGExGeo::GetV3Downscaled(Vertices[B]), PCGExGeo::GetV3Downscaled(Vertices[C]))
			};

			if (Lengths[0] > Lengths[1] && Lengths[0] > Lengths[2])
			{
				OutStart = A;
				OutEnd = B;
			}
			else if (Lengths[1] > Lengths[0] && Lengths[1] > Lengths[2])
			{
				OutStart = A;
				OutEnd = C;
			}
			else
			{
				OutStart = B;
				OutEnd = C;
			}
		}
	};

	template <int DIMENSIONS>
	class PCGEXTENDEDTOOLKIT_API TFSimplex
	{
	public:
		double Normal[DIMENSIONS];
		double Centroid[DIMENSIONS];
		FBox Bounds;

		/// The vertices that make up the simplex.
		/// For 2D a face will be 2 vertices making a line.
		/// For 3D a face will be 3 vertices making a triangle.
		TFVtx<DIMENSIONS>* Vertices[DIMENSIONS];

		/// The simplexs adjacent to this simplex
		/// For 2D a simplex will be a segment and it with have two adjacent segments joining it.
		/// For 3D a simplex will be a triangle and it with have three adjacent triangles joining it.
		TFSimplex* AdjacentFaces[DIMENSIONS];

		double Offset = 0;
		int32 Tag = 0;
		bool bIsNormalFlipped = false;

		TFSimplex()
		{
			for (int i = 0; i < DIMENSIONS; i++)
			{
				AdjacentFaces[i] = nullptr;
				Vertices[i] = nullptr;
			}
		}

		explicit TFSimplex(TFSimplex* Other)
		{
			for (int i = 0; i < DIMENSIONS; i++)
			{
				Vertices[i] = Other->Vertices[i];
				Normal[i] = Other->Normal[i];
				Vertices[i] = Other->Vertices[i];
				Centroid[i] = Other->Centroid[i];
			}

			bIsNormalFlipped = Other->bIsNormalFlipped;
			Offset = Other->Offset;
		}

		virtual ~TFSimplex()
		{
		}

		double Dot(TFVtx<DIMENSIONS>* V)
		{
			if (!V) { return 0.0f; }
			double Dp = 0.0f;
			for (int i = 0; i < DIMENSIONS; i++) { Dp += Normal[i] * V[i]; }
			return Dp;
		}

		bool Remove(TFSimplex* OtherSimplex)
		{
			if (!OtherSimplex) { return false; }
			for (int i = 0; i < DIMENSIONS; i++)
			{
				if (AdjacentFaces[i] == OtherSimplex)
				{
					AdjacentFaces[i] = nullptr;
					return true;
				}
			}
			return false;
		}

		void UpdateNormal()
		{
			if (DIMENSIONS == 3)
			{
				const TFVtx<DIMENSIONS>& A = *Vertices[0];
				const TFVtx<DIMENSIONS>& B = *Vertices[1];
				const TFVtx<DIMENSIONS>& C = *Vertices[2];

				const double X[3] = {B[0] - A[0], B[1] - A[1], B[2] - A[2]};
				const double Y[3] = {C[0] - B[0], C[1] - B[1], C[2] - B[2]};

				const double Nx = X[1] * Y[2] - X[2] * Y[1];
				const double Ny = X[2] * Y[0] - X[0] * Y[2];
				const double Nz = X[0] * Y[1] - X[1] * Y[0];

				const double f = 1 / FMath::Sqrt(Nx * Nx + Ny * Ny + Nz * Nz);
				Normal[0] = f * Nx;
				Normal[1] = f * Ny;
				Normal[2] = f * Nz;
			}
			else if (DIMENSIONS == 2)
			{
				const TFVtx<DIMENSIONS>& A = *Vertices[0];
				const TFVtx<DIMENSIONS>& B = *Vertices[1];

				const double Sub[2] = {A[0] - B[0], A[1] - B[1]};

				const double Nx = -Sub[1];
				const double Ny = Sub[0];

				const double f = 1.0f / FMath::Sqrt(Nx * Nx + Ny * Ny);
				Normal[0] = f * Nx;
				Normal[1] = f * Ny;
			}
			else if (DIMENSIONS == 4)
			{
				const TFVtx<DIMENSIONS>& A = *Vertices[0];
				const TFVtx<DIMENSIONS>& B = *Vertices[1];
				const TFVtx<DIMENSIONS>& C = *Vertices[2];
				const TFVtx<DIMENSIONS>& D = *Vertices[3];

				const double X[4] = {B[0] - A[0], B[1] - A[1], B[2] - A[2], B[3] - A[3]};
				const double Y[4] = {C[0] - B[0], C[1] - B[1], C[2] - B[2], C[3] - B[3]};
				const double Z[4] = {D[0] - C[0], D[1] - C[1], D[2] - C[2], D[3] - C[3]};

				const double Nx = X[3] * (Y[2] * Z[1] - Y[1] * Z[2])
					+ X[2] * (Y[1] * Z[3] - Y[3] * Z[1])
					+ X[1] * (Y[3] * Z[2] - Y[2] * Z[3]);
				const double Ny = X[3] * (Y[0] * Z[2] - Y[2] * Z[0])
					+ X[2] * (Y[3] * Z[0] - Y[0] * Z[3])
					+ X[0] * (Y[2] * Z[3] - Y[3] * Z[2]);
				const double Nz = X[3] * (Y[1] * Z[0] - Y[0] * Z[1])
					+ X[1] * (Y[0] * Z[3] - Y[3] * Z[0])
					+ X[0] * (Y[3] * Z[1] - Y[1] * Z[3]);
				const double Nw = X[2] * (Y[0] * Z[1] - Y[1] * Z[0])
					+ X[1] * (Y[2] * Z[0] - Y[0] * Z[2])
					+ X[0] * (Y[1] * Z[2] - Y[2] * Z[1]);

				const double F = 1 / FMath::Sqrt(Nx * Nx + Ny * Ny + Nz * Nz + Nw * Nw);
				Normal[0] = F * Nx;
				Normal[1] = F * Ny;
				Normal[2] = F * Nz;
				Normal[3] = F * Nw;
			}
		}

		void UpdateCentroid()
		{
			for (int x = 0; x < DIMENSIONS; x++) { Centroid[x] = 0; }
			for (int i = 0; i < DIMENSIONS; i++) { for (int x = 0; x < DIMENSIONS; x++) { Centroid[x] += (*Vertices[i])[x]; } }
			for (int x = 0; x < DIMENSIONS; x++) { Centroid[x] /= static_cast<double>(DIMENSIONS); }

			Bounds = FBox(ForceInit);
			for (int i = 0; i < DIMENSIONS; i++) { Bounds += Vertices[i]->Location; }
		};

		void UpdateAdjacency(TFSimplex* OtherSimplex)
		{
			int i;

			// reset marks on the 1st face & mark 2nd
			for (i = 0; i < DIMENSIONS; i++)
			{
				Vertices[i]->Tag = 0;
				OtherSimplex->Vertices[i]->Tag = 1;
			}

			// find the 1st false index
			for (i = 0; i < DIMENSIONS; i++) { if (Vertices[i]->Tag == 0) { break; } }

			// no vertex was marked
			if (i == DIMENSIONS) { return; }

			// check if only 1 vertex wasn't marked
			for (int j = i + 1; j < DIMENSIONS; j++) { if (Vertices[j]->Tag == 0) { return; } }

			// if we are here, the two faces share an edge
			AdjacentFaces[i] = OtherSimplex;

			// update the adj. face on the other face - find the vertex that remains marked
			for (i = 0; i < DIMENSIONS; i++) { Vertices[i]->Tag = 0; }
			for (i = 0; i < DIMENSIONS; i++) { if (OtherSimplex->Vertices[i]->Tag == 1) { break; } }

			OtherSimplex->AdjacentFaces[i] = this;
		}

		bool HasNullAdjacency() const
		{
			for (int i = 0; i < DIMENSIONS; i++) { if (!AdjacentFaces[i]) { return true; } }
			return false;
		}

		bool HasAdjacency() const
		{
			for (int i = 0; i < DIMENSIONS; i++) { if (AdjacentFaces[i]) { return true; } }
			return false;
		}

		/// Check if the vertex is "visible" from the face.
		/// The vertex is "over face" if the return value is > Constants.PlaneDistanceTolerance.
		double GetVertexDistance(const TFVtx<DIMENSIONS>* V)
		{
			double Distance = Offset;
			for (int i = 0; i < DIMENSIONS; i++) { Distance += Normal[i] * (*V)[i]; }
			return Distance;
		}

		double GetVertexDistance(const FVector& V)
		{
			double Distance = Offset;
			for (int i = 0; i < DIMENSIONS; i++) { Distance += Normal[i] * V[i]; }
			return Distance;
		}

		void GetTriangles(TArray<FTriangle<DIMENSIONS>>& Triangles)
		{
			if (DIMENSIONS == 3)
			{
				Triangles.Emplace(Vertices[0]->Id, Vertices[1]->Id, Vertices[2]->Id);
			}
			else if (DIMENSIONS == 4)
			{
				Triangles.Emplace(Vertices[0]->Id, Vertices[1]->Id, Vertices[2]->Id);
				Triangles.Emplace(Vertices[0]->Id, Vertices[1]->Id, Vertices[3]->Id);
				Triangles.Emplace(Vertices[0]->Id, Vertices[2]->Id, Vertices[3]->Id);
			}
		}
	};
}
