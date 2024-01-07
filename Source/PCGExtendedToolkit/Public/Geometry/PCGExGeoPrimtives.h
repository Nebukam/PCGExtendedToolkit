// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

namespace PCGExGeo
{
	template <int DIMENSIONS>
	class PCGEXTENDEDTOOLKIT_API TFVtx
	{
		double Position[DIMENSIONS];

	public:
		int32 Id = -1;
		int32 Tag = 0;
		bool bIsOnHull = false;

		int Dimension() const { return DIMENSIONS; }

		double& operator[](int32 Component) { return Position[Component]; };
		double operator[](int32 Component) const { return Position[Component]; };

		TFVtx()
		{
		}

		FVector2D GetV2() const { return FVector2D(Position[0], Position[1]); }

		void SetV2(FVector2D V)
		{
			Position[0] = V[0];
			Position[1] = V[1];
		}

		FVector GetV3() const { return FVector(Position[0], Position[1], Position[2]); }

		void SetV3(FVector V)
		{
			Position[0] = V[0];
			Position[1] = V[1];
			Position[2] = V[2];
		}

		FVector4 GetV4() const { return FVector4(Position[0], Position[1], Position[2], Position[3]); }

		void SetV4(FVector4 V)
		{
			Position[0] = V[0];
			Position[1] = V[1];
			Position[2] = V[2];
			Position[3] = V[3];
		}

		double SqrMagnitude()
		{
			double sum = 0.0f;
			for (int i = 0; i < Dimension; i++) { sum += Position[i] * Position[i]; }
			return sum;
		}
	};

	template <int DIMENSIONS>
	class PCGEXTENDEDTOOLKIT_API TFSimplex
	{
	public:
		double Normal[DIMENSIONS];
		double Centroid[DIMENSIONS];

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
			for (int i = 0; i < DIMENSIONS; i++) { if (!AdjacentFaces[i]) return true; }
			return false;
		}

		bool HasAdjacency() const
		{
			for (int i = 0; i < DIMENSIONS; i++) { if (AdjacentFaces[i]) return true; }
			return false;
		}

		/// Check if the vertex is "visible" from the face.
		/// The vertex is "over face" if the return value is > Constants.PlaneDistanceTolerance.
		double GetVertexDistance(const TFVtx<DIMENSIONS>* V)
		{
			double Distance = Offset;
			for (int i = 0; i < DIMENSIONS; i++) Distance += Normal[i] * (*V)[i];
			return Distance;
		}
	};
}
