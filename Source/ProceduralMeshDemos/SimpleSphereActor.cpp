// Copyright Sigurdur Gunnarsson. All Rights Reserved.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
// Example sphere mesh

#include "SimpleSphereActor.h"

ASimpleSphereActor::ASimpleSphereActor()
{
	PrimaryActorTick.bCanEverTick = false;
	MeshComponent = CreateDefaultSubobject<URuntimeProceduralMeshComponent>(TEXT("ProceduralMesh"));
	SetRootComponent(MeshComponent);
}

void ASimpleSphereActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (bRequiresMeshRebuild || MeshComponent->GetNumSections() == 0)
	{
		GenerateMesh();
		bRequiresMeshRebuild = false;
	}
}

#if WITH_EDITOR
void ASimpleSphereActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetOwnerClass()->IsChildOf(StaticClass()))
	{
		bRequiresMeshRebuild = true;
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void ASimpleSphereActor::PostLoad()
{
	Super::PostLoad();
	GenerateMesh();
	bRequiresMeshRebuild = false;
}

void ASimpleSphereActor::SetupMeshBuffers()
{
	int32 VertexCount = 0;
	int32 TriangleCount = 0;

	const int32 ClampedSegments = FMath::Max(Segments, 1);

	switch (SphereType)
	{
	case ESphereType::UVSphere:
	{
		const int32 Rings = ClampedSegments;
		const int32 Slices = ClampedSegments * 2;
		// Top and bottom pole fans: Slices triangles each (3 verts per triangle)
		// Middle rings: (Rings - 2) rings * Slices quads * 2 triangles * 3 verts
		VertexCount = Slices * 3 * 2 + FMath::Max(0, Rings - 2) * Slices * 4;
		TriangleCount = Slices * 3 * 2 + FMath::Max(0, Rings - 2) * Slices * 6;
		break;
	}
	case ESphereType::Geodesic:
	{
		const int32 Subdivisions = FMath::Clamp(ClampedSegments, 1, 6);
		int32 NumTriangles = 20;
		for (int32 i = 0; i < Subdivisions; ++i)
		{
			NumTriangles *= 4;
		}
		VertexCount = NumTriangles * 3;
		TriangleCount = NumTriangles * 3;
		break;
	}
	case ESphereType::NormalizedCube:
	{
		VertexCount = 6 * ClampedSegments * ClampedSegments * 4;
		TriangleCount = 6 * ClampedSegments * ClampedSegments * 6;
		break;
	}
	}

	if (VertexCount != Positions.Num())
	{
		Positions.Empty();
		Positions.AddUninitialized(VertexCount);
		Normals.Empty();
		Normals.AddUninitialized(VertexCount);
		Tangents.Empty();
		Tangents.AddUninitialized(VertexCount);
		TexCoords.Empty();
		TexCoords.AddUninitialized(VertexCount);
	}

	if (TriangleCount != Triangles.Num())
	{
		Triangles.Empty();
		Triangles.AddUninitialized(TriangleCount);
	}
}

void ASimpleSphereActor::GenerateMesh()
{
	if (!IsValid(MeshComponent))
	{
		return;
	}

	MeshComponent->ClearAllMeshSections();

	if (Radius <= 0 || Segments < 1)
	{
		return;
	}

	SetupMeshBuffers();

	switch (SphereType)
	{
	case ESphereType::UVSphere:
		GenerateUVSphere();
		break;
	case ESphereType::Geodesic:
		GenerateGeodesicSphere();
		break;
	case ESphereType::NormalizedCube:
		GenerateNormalizedCubeSphere();
		break;
	}

	MeshComponent->CreateMeshSection_LinearColor(0, Positions, Triangles, Normals, TexCoords, {}, {}, {}, {}, Tangents, false);
	if (Material)
	{
		MeshComponent->SetMaterial(0, Material);
	}
}

void ASimpleSphereActor::GenerateUVSphere()
{
	const int32 Rings = FMath::Max(Segments, 1);
	const int32 Slices = Rings * 2;
	int32 VertexIndex = 0;
	int32 TriangleIndex = 0;

	// Generate rings from top pole to bottom pole
	for (int32 RingIndex = 0; RingIndex < Rings; ++RingIndex)
	{
		const float Phi0 = PI * static_cast<float>(RingIndex) / static_cast<float>(Rings);
		const float Phi1 = PI * static_cast<float>(RingIndex + 1) / static_cast<float>(Rings);
		const float SinPhi0 = FMath::Sin(Phi0);
		const float CosPhi0 = FMath::Cos(Phi0);
		const float SinPhi1 = FMath::Sin(Phi1);
		const float CosPhi1 = FMath::Cos(Phi1);

		for (int32 SliceIndex = 0; SliceIndex < Slices; ++SliceIndex)
		{
			const float Theta0 = 2.0f * PI * static_cast<float>(SliceIndex) / static_cast<float>(Slices);
			const float Theta1 = 2.0f * PI * static_cast<float>(SliceIndex + 1) / static_cast<float>(Slices);
			const float CosTheta0 = FMath::Cos(Theta0);
			const float SinTheta0 = FMath::Sin(Theta0);
			const float CosTheta1 = FMath::Cos(Theta1);
			const float SinTheta1 = FMath::Sin(Theta1);

			// Four corners of the quad on the sphere
			const FVector N00 = FVector(SinPhi0 * CosTheta0, SinPhi0 * SinTheta0, CosPhi0);
			const FVector N10 = FVector(SinPhi0 * CosTheta1, SinPhi0 * SinTheta1, CosPhi0);
			const FVector N01 = FVector(SinPhi1 * CosTheta0, SinPhi1 * SinTheta0, CosPhi1);
			const FVector N11 = FVector(SinPhi1 * CosTheta1, SinPhi1 * SinTheta1, CosPhi1);

			const FVector P00 = N00 * Radius;
			const FVector P10 = N10 * Radius;
			const FVector P01 = N01 * Radius;
			const FVector P11 = N11 * Radius;

			// UVs
			const float U0 = 1.0f - static_cast<float>(SliceIndex) / static_cast<float>(Slices);
			const float U1 = 1.0f - static_cast<float>(SliceIndex + 1) / static_cast<float>(Slices);
			const float V0 = static_cast<float>(RingIndex) / static_cast<float>(Rings);
			const float V1 = static_cast<float>(RingIndex + 1) / static_cast<float>(Rings);

			if (RingIndex == 0)
			{
				// Top pole triangle fan
				const int32 Idx0 = VertexIndex++;
				const int32 Idx1 = VertexIndex++;
				const int32 Idx2 = VertexIndex++;

				Positions[Idx0] = P00;
				Positions[Idx1] = P11;
				Positions[Idx2] = P01;

				Normals[Idx0] = N00;
				Normals[Idx1] = N11;
				Normals[Idx2] = N01;

				TexCoords[Idx0] = FVector2D((U0 + U1) * 0.5f, V0);
				TexCoords[Idx1] = FVector2D(U1, V1);
				TexCoords[Idx2] = FVector2D(U0, V1);

				const FVector TangentDir0 = FVector(-SinTheta0, CosTheta0, 0.0f);
				const FVector TangentDir1 = FVector(-SinTheta1, CosTheta1, 0.0f);
				Tangents[Idx0] = FProcMeshTangent(TangentDir0, false);
				Tangents[Idx1] = FProcMeshTangent(TangentDir1, false);
				Tangents[Idx2] = FProcMeshTangent(TangentDir0, false);

				Triangles[TriangleIndex++] = Idx0;
				Triangles[TriangleIndex++] = Idx1;
				Triangles[TriangleIndex++] = Idx2;
			}
			else if (RingIndex == Rings - 1)
			{
				// Bottom pole triangle fan
				const int32 Idx0 = VertexIndex++;
				const int32 Idx1 = VertexIndex++;
				const int32 Idx2 = VertexIndex++;

				Positions[Idx0] = P00;
				Positions[Idx1] = P10;
				Positions[Idx2] = P01;

				Normals[Idx0] = N00;
				Normals[Idx1] = N10;
				Normals[Idx2] = N01;

				TexCoords[Idx0] = FVector2D(U0, V0);
				TexCoords[Idx1] = FVector2D(U1, V0);
				TexCoords[Idx2] = FVector2D((U0 + U1) * 0.5f, V1);

				const FVector TangentDir0 = FVector(-SinTheta0, CosTheta0, 0.0f);
				const FVector TangentDir1 = FVector(-SinTheta1, CosTheta1, 0.0f);
				Tangents[Idx0] = FProcMeshTangent(TangentDir0, false);
				Tangents[Idx1] = FProcMeshTangent(TangentDir1, false);
				Tangents[Idx2] = FProcMeshTangent(TangentDir0, false);

				Triangles[TriangleIndex++] = Idx0;
				Triangles[TriangleIndex++] = Idx1;
				Triangles[TriangleIndex++] = Idx2;
			}
			else
			{
				// Middle quad = 2 triangles
				const int32 Idx0 = VertexIndex++;
				const int32 Idx1 = VertexIndex++;
				const int32 Idx2 = VertexIndex++;
				const int32 Idx3 = VertexIndex++;

				Positions[Idx0] = P00;
				Positions[Idx1] = P10;
				Positions[Idx2] = P11;
				Positions[Idx3] = P01;

				Normals[Idx0] = N00;
				Normals[Idx1] = N10;
				Normals[Idx2] = N11;
				Normals[Idx3] = N01;

				TexCoords[Idx0] = FVector2D(U0, V0);
				TexCoords[Idx1] = FVector2D(U1, V0);
				TexCoords[Idx2] = FVector2D(U1, V1);
				TexCoords[Idx3] = FVector2D(U0, V1);

				const FVector TangentDir0 = FVector(-SinTheta0, CosTheta0, 0.0f);
				const FVector TangentDir1 = FVector(-SinTheta1, CosTheta1, 0.0f);
				Tangents[Idx0] = FProcMeshTangent(TangentDir0, false);
				Tangents[Idx1] = FProcMeshTangent(TangentDir1, false);
				Tangents[Idx2] = FProcMeshTangent(TangentDir1, false);
				Tangents[Idx3] = FProcMeshTangent(TangentDir0, false);

				Triangles[TriangleIndex++] = Idx0;
				Triangles[TriangleIndex++] = Idx1;
				Triangles[TriangleIndex++] = Idx2;
				Triangles[TriangleIndex++] = Idx0;
				Triangles[TriangleIndex++] = Idx2;
				Triangles[TriangleIndex++] = Idx3;
			}
		}
	}
}

void ASimpleSphereActor::GenerateGeodesicSphere()
{
	const int32 Subdivisions = FMath::Clamp(FMath::Max(Segments, 1), 1, 6);

	// Golden ratio
	const float T = (1.0f + FMath::Sqrt(5.0f)) / 2.0f;

	// Icosahedron vertices (normalized)
	TArray<FVector> IcoVertices;
	IcoVertices.Add(FVector(-1,  T,  0).GetSafeNormal());
	IcoVertices.Add(FVector( 1,  T,  0).GetSafeNormal());
	IcoVertices.Add(FVector(-1, -T,  0).GetSafeNormal());
	IcoVertices.Add(FVector( 1, -T,  0).GetSafeNormal());
	IcoVertices.Add(FVector( 0, -1,  T).GetSafeNormal());
	IcoVertices.Add(FVector( 0,  1,  T).GetSafeNormal());
	IcoVertices.Add(FVector( 0, -1, -T).GetSafeNormal());
	IcoVertices.Add(FVector( 0,  1, -T).GetSafeNormal());
	IcoVertices.Add(FVector( T,  0, -1).GetSafeNormal());
	IcoVertices.Add(FVector( T,  0,  1).GetSafeNormal());
	IcoVertices.Add(FVector(-T,  0, -1).GetSafeNormal());
	IcoVertices.Add(FVector(-T,  0,  1).GetSafeNormal());

	// 20 icosahedron faces (indices into IcoVertices)
	struct TriIndices { int32 V0, V1, V2; };
	TArray<TriIndices> IcoTriangles;
	IcoTriangles.Add({0, 11, 5});
	IcoTriangles.Add({0, 5, 1});
	IcoTriangles.Add({0, 1, 7});
	IcoTriangles.Add({0, 7, 10});
	IcoTriangles.Add({0, 10, 11});
	IcoTriangles.Add({1, 5, 9});
	IcoTriangles.Add({5, 11, 4});
	IcoTriangles.Add({11, 10, 2});
	IcoTriangles.Add({10, 7, 6});
	IcoTriangles.Add({7, 1, 8});
	IcoTriangles.Add({3, 9, 4});
	IcoTriangles.Add({3, 4, 2});
	IcoTriangles.Add({3, 2, 6});
	IcoTriangles.Add({3, 6, 8});
	IcoTriangles.Add({3, 8, 9});
	IcoTriangles.Add({4, 9, 5});
	IcoTriangles.Add({2, 4, 11});
	IcoTriangles.Add({6, 2, 10});
	IcoTriangles.Add({8, 6, 7});
	IcoTriangles.Add({9, 8, 1});

	// Subdivide
	for (int32 Sub = 0; Sub < Subdivisions; ++Sub)
	{
		TMap<uint64, int32> MidpointCache;
		TArray<TriIndices> NewTriangles;
		NewTriangles.Reserve(IcoTriangles.Num() * 4);

		for (const TriIndices& Tri : IcoTriangles)
		{
			// Get or create midpoints
			auto GetMidpoint = [&](int32 A, int32 B) -> int32
			{
				const uint64 Key = (FMath::Min(A, B) * 100000LL) + FMath::Max(A, B);
				if (const int32* Found = MidpointCache.Find(Key))
				{
					return *Found;
				}
				const FVector Mid = ((IcoVertices[A] + IcoVertices[B]) * 0.5f).GetSafeNormal();
				const int32 NewIdx = IcoVertices.Add(Mid);
				MidpointCache.Add(Key, NewIdx);
				return NewIdx;
			};

			const int32 A = GetMidpoint(Tri.V0, Tri.V1);
			const int32 B = GetMidpoint(Tri.V1, Tri.V2);
			const int32 C = GetMidpoint(Tri.V0, Tri.V2);

			NewTriangles.Add({Tri.V0, A, C});
			NewTriangles.Add({A, Tri.V1, B});
			NewTriangles.Add({C, B, Tri.V2});
			NewTriangles.Add({A, B, C});
		}

		IcoTriangles = MoveTemp(NewTriangles);
	}

	// Build mesh buffers from subdivided icosphere - unshared vertices for unique UVs
	int32 VertexIndex = 0;
	int32 TriangleIndex = 0;

	for (const TriIndices& Tri : IcoTriangles)
	{
		const FVector& N0 = IcoVertices[Tri.V0];
		const FVector& N1 = IcoVertices[Tri.V1];
		const FVector& N2 = IcoVertices[Tri.V2];

		const int32 Idx0 = VertexIndex++;
		const int32 Idx1 = VertexIndex++;
		const int32 Idx2 = VertexIndex++;

		Positions[Idx0] = N0 * Radius;
		Positions[Idx1] = N1 * Radius;
		Positions[Idx2] = N2 * Radius;

		Normals[Idx0] = N0;
		Normals[Idx1] = N1;
		Normals[Idx2] = N2;

		// Spherical UV projection
		auto SphericalUV = [](const FVector& N) -> FVector2D
		{
			const float U = 0.5f - FMath::Atan2(N.Y, N.X) / (2.0f * PI);
			const float V = 0.5f - FMath::Asin(FMath::Clamp(N.Z, -1.0f, 1.0f)) / PI;
			return FVector2D(U, V);
		};

		FVector2D UV0 = SphericalUV(N0);
		FVector2D UV1 = SphericalUV(N1);
		FVector2D UV2 = SphericalUV(N2);

		// Fix UV seam wrapping: if any two UVs differ by more than 0.5 in U,
		// the triangle crosses the seam
		const float MaxU = FMath::Max3(UV0.X, UV1.X, UV2.X);
		const float MinU = FMath::Min3(UV0.X, UV1.X, UV2.X);
		if (MaxU - MinU > 0.5f)
		{
			if (UV0.X < 0.25f) UV0.X += 1.0f;
			if (UV1.X < 0.25f) UV1.X += 1.0f;
			if (UV2.X < 0.25f) UV2.X += 1.0f;
		}

		TexCoords[Idx0] = UV0;
		TexCoords[Idx1] = UV1;
		TexCoords[Idx2] = UV2;

		// Tangent: along longitude direction (perpendicular to normal in horizontal plane)
		auto ComputeTangent = [](const FVector& N) -> FProcMeshTangent
		{
			const FVector TangentDir = FVector(-N.Y, N.X, 0.0f).GetSafeNormal();
			// Handle pole case
			if (TangentDir.IsNearlyZero())
			{
				return FProcMeshTangent(FVector(1, 0, 0), false);
			}
			return FProcMeshTangent(TangentDir, false);
		};

		Tangents[Idx0] = ComputeTangent(N0);
		Tangents[Idx1] = ComputeTangent(N1);
		Tangents[Idx2] = ComputeTangent(N2);

		Triangles[TriangleIndex++] = Idx0;
		Triangles[TriangleIndex++] = Idx2;
		Triangles[TriangleIndex++] = Idx1;
	}
}

void ASimpleSphereActor::GenerateNormalizedCubeSphere()
{
	const int32 Subdivisions = FMath::Max(Segments, 1);
	int32 VertexIndex = 0;
	int32 TriangleIndex = 0;

	// 6 face directions: +X, -X, +Y, -Y, +Z, -Z
	// Each face defined by: normal, right, up
	struct FaceData
	{
		FVector Normal;
		FVector Right;
		FVector Up;
	};

	const FaceData Faces[6] = {
		{ FVector( 1,  0,  0), FVector( 0,  1,  0), FVector(0, 0,  1) }, // +X
		{ FVector(-1,  0,  0), FVector( 0, -1,  0), FVector(0, 0,  1) }, // -X
		{ FVector( 0,  1,  0), FVector(-1,  0,  0), FVector(0, 0,  1) }, // +Y
		{ FVector( 0, -1,  0), FVector( 1,  0,  0), FVector(0, 0,  1) }, // -Y
		{ FVector( 0,  0,  1), FVector( 0,  1,  0), FVector(-1, 0, 0) }, // +Z
		{ FVector( 0,  0, -1), FVector( 0,  1,  0), FVector( 1, 0, 0) }, // -Z
	};

	for (int32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
	{
		const FaceData& Face = Faces[FaceIndex];

		for (int32 Row = 0; Row < Subdivisions; ++Row)
		{
			for (int32 Col = 0; Col < Subdivisions; ++Col)
			{
				// Parametric coordinates [-1, 1] for each corner of the quad
				const float U0 = -1.0f + 2.0f * static_cast<float>(Col) / static_cast<float>(Subdivisions);
				const float U1 = -1.0f + 2.0f * static_cast<float>(Col + 1) / static_cast<float>(Subdivisions);
				const float V0 = -1.0f + 2.0f * static_cast<float>(Row) / static_cast<float>(Subdivisions);
				const float V1 = -1.0f + 2.0f * static_cast<float>(Row + 1) / static_cast<float>(Subdivisions);

				// Cube face positions, then normalize to sphere
				auto CubeToSphere = [&](float U, float V) -> FVector
				{
					return (Face.Normal + Face.Right * U + Face.Up * V).GetSafeNormal();
				};

				const FVector N00 = CubeToSphere(U0, V0);
				const FVector N10 = CubeToSphere(U1, V0);
				const FVector N11 = CubeToSphere(U1, V1);
				const FVector N01 = CubeToSphere(U0, V1);

				const int32 Idx0 = VertexIndex++;
				const int32 Idx1 = VertexIndex++;
				const int32 Idx2 = VertexIndex++;
				const int32 Idx3 = VertexIndex++;

				Positions[Idx0] = N00 * Radius;
				Positions[Idx1] = N10 * Radius;
				Positions[Idx2] = N11 * Radius;
				Positions[Idx3] = N01 * Radius;

				Normals[Idx0] = N00;
				Normals[Idx1] = N10;
				Normals[Idx2] = N11;
				Normals[Idx3] = N01;

				// Per-face UV mapping [0, 1]
				const float TexU0 = static_cast<float>(Col) / static_cast<float>(Subdivisions);
				const float TexU1 = static_cast<float>(Col + 1) / static_cast<float>(Subdivisions);
				const float TexV0 = static_cast<float>(Row) / static_cast<float>(Subdivisions);
				const float TexV1 = static_cast<float>(Row + 1) / static_cast<float>(Subdivisions);

				TexCoords[Idx0] = FVector2D(1.0f - TexU0, 1.0f - TexV0);
				TexCoords[Idx1] = FVector2D(1.0f - TexU1, 1.0f - TexV0);
				TexCoords[Idx2] = FVector2D(1.0f - TexU1, 1.0f - TexV1);
				TexCoords[Idx3] = FVector2D(1.0f - TexU0, 1.0f - TexV1);

				// Tangent: along the face's Right direction, projected onto the sphere tangent plane
				auto ComputeFaceTangent = [&](const FVector& Normal) -> FProcMeshTangent
				{
					FVector TangentDir = Face.Right - Normal * FVector::DotProduct(Face.Right, Normal);
					TangentDir = TangentDir.GetSafeNormal();
					if (TangentDir.IsNearlyZero())
					{
						TangentDir = FVector(1, 0, 0);
					}
					return FProcMeshTangent(TangentDir, false);
				};

				Tangents[Idx0] = ComputeFaceTangent(N00);
				Tangents[Idx1] = ComputeFaceTangent(N10);
				Tangents[Idx2] = ComputeFaceTangent(N11);
				Tangents[Idx3] = ComputeFaceTangent(N01);

				Triangles[TriangleIndex++] = Idx0;
				Triangles[TriangleIndex++] = Idx2;
				Triangles[TriangleIndex++] = Idx1;
				Triangles[TriangleIndex++] = Idx0;
				Triangles[TriangleIndex++] = Idx3;
				Triangles[TriangleIndex++] = Idx2;
			}
		}
	}
}
