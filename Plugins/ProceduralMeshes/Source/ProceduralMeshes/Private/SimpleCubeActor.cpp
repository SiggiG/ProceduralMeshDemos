// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example cube mesh

#include "SimpleCubeActor.h"
#include "RuntimeMeshProviderStatic.h"

ASimpleCubeActor::ASimpleCubeActor()
{
	PrimaryActorTick.bCanEverTick = false;
	StaticProvider = CreateDefaultSubobject<URuntimeMeshProviderStatic>(TEXT("RuntimeMeshProvider-Static"));
}

void ASimpleCubeActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	GenerateMesh();
}

void ASimpleCubeActor::SetupMeshBuffers()
{
	const int32 VertexCount = 6 * 4; // 6 sides on a cube, 4 verts each
	const int32 TriangleCount = 6 * 2 * 3; // 2x triangles per cube side, 3 verts each

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

void ASimpleCubeActor::GenerateMesh()
{
	GetRuntimeMeshComponent()->Initialize(StaticProvider);
	StaticProvider->ClearSection(0, 0);
	SetupMeshBuffers();
	GenerateCube(Positions, Triangles, Normals, Tangents, TexCoords, Size);

	const TArray<FColor> EmptyColors{};
	StaticProvider->CreateSectionFromComponents(0, 0, 0, Positions, Triangles, Normals, TexCoords, EmptyColors, Tangents, ERuntimeMeshUpdateFrequency::Infrequent, false);
	StaticProvider->SetupMaterialSlot(0, TEXT("CubeMaterial"), Material);
}

void ASimpleCubeActor::GenerateCube(TArray<FVector>& InVertices, TArray<int32>& InTriangles, TArray<FVector>& InNormals, TArray<FRuntimeMeshTangent>& InTangents, TArray<FVector2D>& InTexCoords, const FVector InSize)
{
	// NOTE: Unreal uses an upper-left origin UV
	// NOTE: This sample uses a simple UV mapping scheme where each face is the same
	// NOTE: For a normal facing towards us, be build the polygon CCW in the order 0-1-2 then 0-2-3 to complete the quad.
	// Remember in Unreal, X is forwards, Y is to your right and Z is up.

	// Calculate a half offset so we get correct center of object
	const float OffsetX = InSize.X / 2.0f;
	const float OffsetY = InSize.Y / 2.0f;
	const float OffsetZ = InSize.Z / 2.0f;

	// Define the 8 corners of the cube
	const FVector P0 = FVector(OffsetX,  OffsetY, -OffsetZ);
	const FVector P1 = FVector(OffsetX, -OffsetY, -OffsetZ);
	const FVector P2 = FVector(OffsetX, -OffsetY,  OffsetZ);
	const FVector P3 = FVector(OffsetX,  OffsetY,  OffsetZ);
	const FVector P4 = FVector(-OffsetX, OffsetY, -OffsetZ);
	const FVector P5 = FVector(-OffsetX, -OffsetY, -OffsetZ);
	const FVector P6 = FVector(-OffsetX, -OffsetY, OffsetZ);
	const FVector P7 = FVector(-OffsetX, OffsetY, OffsetZ);

	// Now we create 6x faces, 4 vertices each
	int32 VertexOffset = 0;
	int32 TriangleOffset = 0;
	FVector Normal;
	FRuntimeMeshTangent Tangent;

 	// Front (+X) face: 0-1-2-3
	Normal = FVector(1, 0, 0);
	Tangent = FVector(0, 1, 0);
	BuildQuad(InVertices, InTriangles, InNormals, InTangents, InTexCoords, P0, P1, P2, P3, VertexOffset, TriangleOffset, Normal, Tangent);

 	// Back (-X) face: 5-4-7-6
	Normal = FVector(-1, 0, 0);
	Tangent = FVector(0, -1, 0);
	BuildQuad(InVertices, InTriangles, InNormals, InTangents, InTexCoords, P5, P4, P7, P6, VertexOffset, TriangleOffset, Normal, Tangent);

 	// Left (-Y) face: 1-5-6-2
	Normal = FVector(0, -1, 0);
	Tangent = FVector(1, 0, 0);
	BuildQuad(InVertices, InTriangles, InNormals, InTangents, InTexCoords, P1, P5, P6, P2, VertexOffset, TriangleOffset, Normal, Tangent);

 	// Right (+Y) face: 4-0-3-7
	Normal = FVector(0, 1, 0);
	Tangent = FVector(-1, 0, 0);
	BuildQuad(InVertices, InTriangles, InNormals, InTangents, InTexCoords, P4, P0, P3, P7, VertexOffset, TriangleOffset, Normal, Tangent);

 	// Top (+Z) face: 6-7-3-2
	Normal = FVector(0, 0, 1);
	Tangent = FVector(0, 1, 0);
	BuildQuad(InVertices, InTriangles, InNormals, InTangents, InTexCoords, P6, P7, P3, P2, VertexOffset, TriangleOffset, Normal, Tangent);

 	// Bottom (-Z) face: 1-0-4-5
	Normal = FVector(0, 0, -1);
	Tangent = FVector(0, -1, 0);
	BuildQuad(InVertices, InTriangles, InNormals, InTangents, InTexCoords, P1, P0, P4, P5, VertexOffset, TriangleOffset, Normal, Tangent);
}

void ASimpleCubeActor::BuildQuad(TArray<FVector>& InVertices, TArray<int32>& InTriangles, TArray<FVector>& InNormals, TArray<FRuntimeMeshTangent>& InTangents, TArray<FVector2D>& InTexCoords, const FVector BottomLeft, const FVector BottomRight, const FVector TopRight, const FVector TopLeft, int32& VertexOffset, int32& TriangleOffset, const FVector Normal, const FRuntimeMeshTangent Tangent)
{
	const int32 Index1 = VertexOffset++;
	const int32 Index2 = VertexOffset++;
	const int32 Index3 = VertexOffset++;
	const int32 Index4 = VertexOffset++;
	InVertices[Index1] = BottomLeft;
	InVertices[Index2] = BottomRight;
	InVertices[Index3] = TopRight;
	InVertices[Index4] = TopLeft;
	InTexCoords[Index1] = FVector2D(0.0f, 1.0f);
	InTexCoords[Index2] = FVector2D(1.0f, 1.0f);
	InTexCoords[Index3] = FVector2D(1.0f, 0.0f);
	InTexCoords[Index4] = FVector2D(0.0f, 0.0f);
	InTriangles[TriangleOffset++] = Index1;
	InTriangles[TriangleOffset++] = Index2;
	InTriangles[TriangleOffset++] = Index3;
	InTriangles[TriangleOffset++] = Index1;
	InTriangles[TriangleOffset++] = Index3;
	InTriangles[TriangleOffset++] = Index4;
	// On a cube side, all the vertex normals face the same way
	InNormals[Index1] = InNormals[Index2] = InNormals[Index3] = InNormals[Index4] = Normal;
	InTangents[Index1] = InTangents[Index2] = InTangents[Index3] = InTangents[Index4] = Tangent;
}
