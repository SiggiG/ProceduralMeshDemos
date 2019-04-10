// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example cube mesh

#include "ProceduralMeshesPrivatePCH.h"
#include "SimpleCubeActor.h"

ASimpleCubeActor::ASimpleCubeActor()
{
	RootNode = CreateDefaultSubobject<USceneComponent>("Root");
	RootComponent = RootNode;

	MeshComponent = CreateDefaultSubobject<URuntimeMeshComponent>(TEXT("ProceduralMesh"));
	MeshComponent->GetOrCreateRuntimeMesh()->SetShouldSerializeMeshData(false);
	MeshComponent->SetupAttachment(RootComponent);
}

// This is called when actor is spawned (at runtime or when you drop it into the world in editor)
void ASimpleCubeActor::PostActorCreated()
{
	Super::PostActorCreated();
	GenerateMesh();
}

// This is called when actor is already in level and map is opened
void ASimpleCubeActor::PostLoad()
{
	Super::PostLoad();
	GenerateMesh();
}

void ASimpleCubeActor::SetupMeshBuffers()
{
	int32 VertexCount = 6 * 4; // 6 sides on a cube, 4 verts each
	Vertices.AddUninitialized(VertexCount);
	Triangles.AddUninitialized(6 * 2 * 3); // 2x triangles per cube side, 3 verts each
}

void ASimpleCubeActor::GenerateMesh()
{
	// The number of vertices or polygons wont change at runtime, so we'll just allocate the arrays once
	if (!bHaveBuffersBeenInitialized)
	{
		SetupMeshBuffers();
		bHaveBuffersBeenInitialized = true;
	}

	FBox BoundingBox = FBox(-Size / 2.0f, Size / 2.0f);
	GenerateCube(Vertices, Triangles, Size);
	
	MeshComponent->ClearAllMeshSections();
	MeshComponent->CreateMeshSection(0, Vertices, Triangles, BoundingBox, false, EUpdateFrequency::Infrequent);
	MeshComponent->SetMaterial(0, Material);
}

void ASimpleCubeActor::GenerateCube(TArray<FRuntimeMeshVertexSimple>& InVertices, TArray<int32>& InTriangles, FVector InSize)
{
	// NOTE: Unreal uses an upper-left origin UV
	// NOTE: This sample uses a simple UV mapping scheme where each face is the same
	// NOTE: For a normal facing towards us, be build the polygon CCW in the order 0-1-2 then 0-2-3 to complete the quad.
	// Remember in Unreal, X is forwards, Y is to your right and Z is up.

	// Calculate a half offset so we get correct center of object
	float OffsetX = InSize.X / 2.0f;
	float OffsetY = InSize.Y / 2.0f;
	float OffsetZ = InSize.Z / 2.0f;

	// Define the 8 corners of the cube
	FVector p0 = FVector(OffsetX,  OffsetY, -OffsetZ);
	FVector p1 = FVector(OffsetX, -OffsetY, -OffsetZ);
	FVector p2 = FVector(OffsetX, -OffsetY,  OffsetZ);
	FVector p3 = FVector(OffsetX,  OffsetY,  OffsetZ);
	FVector p4 = FVector(-OffsetX, OffsetY, -OffsetZ);
	FVector p5 = FVector(-OffsetX, -OffsetY, -OffsetZ);
	FVector p6 = FVector(-OffsetX, -OffsetY, OffsetZ);
	FVector p7 = FVector(-OffsetX, OffsetY, OffsetZ);

	// Now we create 6x faces, 4 vertices each
	int32 VertexOffset = 0;
	int32 TriangleOffset = 0;
	FPackedNormal Normal;
	FPackedNormal Tangent;

 	// Front (+X) face: 0-1-2-3
	Normal = FVector(1, 0, 0);
	Tangent = FVector(0, 1, 0);
	BuildQuad(InVertices, InTriangles, p0, p1, p2, p3, VertexOffset, TriangleOffset, Normal, Tangent);

 	// Back (-X) face: 5-4-7-6
	Normal = FVector(-1, 0, 0);
	Tangent = FVector(0, -1, 0);
	BuildQuad(InVertices, InTriangles, p5, p4, p7, p6, VertexOffset, TriangleOffset, Normal, Tangent);

 	// Left (-Y) face: 1-5-6-2
	Normal = FVector(0, -1, 0);
	Tangent = FVector(1, 0, 0);
	BuildQuad(InVertices, InTriangles, p1, p5, p6, p2, VertexOffset, TriangleOffset, Normal, Tangent);

 	// Right (+Y) face: 4-0-3-7
	Normal = FVector(0, 1, 0);
	Tangent = FVector(-1, 0, 0);
	BuildQuad(InVertices, InTriangles, p4, p0, p3, p7, VertexOffset, TriangleOffset, Normal, Tangent);

 	// Top (+Z) face: 6-7-3-2
	Normal = FVector(0, 0, 1);
	Tangent = FVector(0, 1, 0);
	BuildQuad(InVertices, InTriangles, p6, p7, p3, p2, VertexOffset, TriangleOffset, Normal, Tangent);

 	// Bottom (-Z) face: 1-0-4-5
	Normal = FVector(0, 0, -1);
	Tangent = FVector(0, -1, 0);
	BuildQuad(InVertices, InTriangles, p1, p0, p4, p5, VertexOffset, TriangleOffset, Normal, Tangent);
}

void ASimpleCubeActor::BuildQuad(TArray<FRuntimeMeshVertexSimple>& InVertices, TArray<int32>& InTriangles, FVector BottomLeft, FVector BottomRight, FVector TopRight, FVector TopLeft, int32& VertexOffset, int32& TriangleOffset, FPackedNormal Normal, FPackedNormal Tangent)
{
	int32 Index1 = VertexOffset++;
	int32 Index2 = VertexOffset++;
	int32 Index3 = VertexOffset++;
	int32 Index4 = VertexOffset++;
	InVertices[Index1].Position = BottomLeft;
	InVertices[Index2].Position = BottomRight;
	InVertices[Index3].Position = TopRight;
	InVertices[Index4].Position = TopLeft;
	InVertices[Index1].UV0 = FVector2D(0.0f, 1.0f);
	InVertices[Index2].UV0 = FVector2D(1.0f, 1.0f);
	InVertices[Index3].UV0 = FVector2D(1.0f, 0.0f);
	InVertices[Index4].UV0 = FVector2D(0.0f, 0.0f);
	InTriangles[TriangleOffset++] = Index1;
	InTriangles[TriangleOffset++] = Index2;
	InTriangles[TriangleOffset++] = Index3;
	InTriangles[TriangleOffset++] = Index1;
	InTriangles[TriangleOffset++] = Index3;
	InTriangles[TriangleOffset++] = Index4;
	// On a cube side, all the vertex normals face the same way
	InVertices[Index1].Normal = InVertices[Index2].Normal = InVertices[Index3].Normal = InVertices[Index4].Normal = Normal;
	InVertices[Index1].Tangent = InVertices[Index2].Tangent = InVertices[Index3].Tangent = InVertices[Index4].Tangent = Tangent;
}

#if WITH_EDITOR
void ASimpleCubeActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName MemberPropertyChanged = (PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None);

	if ((MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(ASimpleCubeActor, Size)))
	{
		GenerateMesh();
	}
	else if ((MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(ASimpleCubeActor, Material)))
	{
		MeshComponent->SetMaterial(0, Material);
	}
}
#endif // WITH_EDITOR
