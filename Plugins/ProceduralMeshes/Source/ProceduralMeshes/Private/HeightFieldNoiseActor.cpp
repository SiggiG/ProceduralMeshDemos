// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example heightfield generated with noise

#include "ProceduralMeshesPrivatePCH.h"
#include "HeightFieldNoiseActor.h"

AHeightFieldNoiseActor::AHeightFieldNoiseActor()
{
	RootNode = CreateDefaultSubobject<USceneComponent>("Root");
	RootComponent = RootNode;

	MeshComponent = CreateDefaultSubobject<URuntimeMeshComponent>(TEXT("ProceduralMesh"));
	MeshComponent->GetOrCreateRuntimeMesh()->SetShouldSerializeMeshData(false);
	MeshComponent->SetupAttachment(RootComponent);
}

// This is called when actor is spawned (at runtime or when you drop it into the world in editor)
void AHeightFieldNoiseActor::PostActorCreated()
{
	Super::PostActorCreated();
	GenerateMesh();
}

// This is called when actor is already in level and map is opened
void AHeightFieldNoiseActor::PostLoad()
{
	Super::PostLoad();
	GenerateMesh();
}

void AHeightFieldNoiseActor::SetupMeshBuffers()
{
	int32 NumberOfPoints = (LengthSections + 1) * (WidthSections + 1);
	int32 NumberOfVertices = LengthSections * WidthSections * 4; // 4x vertices per quad/section
	int32 NumberOfTriangles = LengthSections * WidthSections * 2 * 3; // 2x3 vertex indexes per quad
	Vertices.AddUninitialized(NumberOfVertices);
	Triangles.AddUninitialized(NumberOfTriangles);
	HeightValues.AddUninitialized(NumberOfPoints);
}

void AHeightFieldNoiseActor::GeneratePoints()
{
	RngStream.Initialize(RandomSeed);

	// Setup example height data
	int32 NumberOfPoints = (LengthSections + 1) * (WidthSections + 1);

	// Fill height data with random values
	for (int32 i = 0; i < NumberOfPoints; i++)
	{
		HeightValues[i] = RngStream.FRandRange(0, Size.Z);
	}
}

void AHeightFieldNoiseActor::GenerateMesh()
{
	if (Size.X < 1 || Size.Y < 1 || LengthSections < 1 || WidthSections < 1)
	{
		MeshComponent->ClearAllMeshSections();
		return;
	}

	// The number of vertices or polygons wont change at runtime, so we'll just allocate the arrays once
	if (!bHaveBuffersBeenInitialized)
	{
		SetupMeshBuffers();
		bHaveBuffersBeenInitialized = true;
	}

	GeneratePoints();

	GenerateGrid(Vertices, Triangles, FVector2D(Size.X, Size.Y), LengthSections, WidthSections, HeightValues);
	FBox BoundingBox = FBox(FVector(0, 0, 0), Size);
	MeshComponent->ClearAllMeshSections();
	MeshComponent->CreateMeshSection(0, Vertices, Triangles, BoundingBox, false, EUpdateFrequency::Infrequent);
	MeshComponent->SetMaterial(0, Material);
}

void AHeightFieldNoiseActor::GenerateGrid(TArray<FRuntimeMeshVertexSimple>& InVertices, TArray<int32>& InTriangles, FVector2D InSize, int32 InLengthSections, int32 InWidthSections, const TArray<float>& InHeightValues)
{
	// Note the coordinates are a bit weird here since I aligned it to the transform (X is forwards or "up", which Y is to the right)
	// Should really fix this up and use standard X, Y coords then transform into object space?
	FVector2D SectionSize = FVector2D(InSize.X / InLengthSections, InSize.Y / InWidthSections);
	int32 VertexIndex = 0;
	int32 TriangleIndex = 0;

	for (int32 X = 0; X < InLengthSections; X++)
	{
		for (int32 Y = 0; Y < InWidthSections; Y++)
		{
			// Setup a quad
			int32 BottomLeftIndex = VertexIndex++;
			int32 BottomRightIndex = VertexIndex++;
			int32 TopRightIndex = VertexIndex++;
			int32 TopLeftIndex = VertexIndex++;

			int32 NoiseIndex_BottomLeft = (X * InWidthSections) + Y;
			int32 NoiseIndex_BottomRight = NoiseIndex_BottomLeft + 1;
			int32 NoiseIndex_TopLeft = ((X+1) * InWidthSections) + Y;
			int32 NoiseIndex_TopRight = NoiseIndex_TopLeft + 1;

			FVector pBottomLeft = FVector(X * SectionSize.X, Y * SectionSize.Y, InHeightValues[NoiseIndex_BottomLeft]);
			FVector pBottomRight = FVector(X * SectionSize.X, (Y+1) * SectionSize.Y, InHeightValues[NoiseIndex_BottomRight]);
			FVector pTopRight = FVector((X + 1) * SectionSize.X, (Y + 1) * SectionSize.Y, InHeightValues[NoiseIndex_TopRight]);
			FVector pTopLeft = FVector((X+1) * SectionSize.X, Y * SectionSize.Y, InHeightValues[NoiseIndex_TopLeft]);
			
			InVertices[BottomLeftIndex].Position = pBottomLeft;
			InVertices[BottomRightIndex].Position = pBottomRight;
			InVertices[TopRightIndex].Position = pTopRight;
			InVertices[TopLeftIndex].Position = pTopLeft;

			// Note that Unreal UV origin (0,0) is top left
			InVertices[BottomLeftIndex].UV0 = FVector2D((float)X / (float)InLengthSections, (float)Y / (float)InWidthSections);
			InVertices[BottomRightIndex].UV0 = FVector2D((float)X / (float)InLengthSections, (float)(Y+1) / (float)InWidthSections);
			InVertices[TopRightIndex].UV0 = FVector2D((float)(X+1) / (float)InLengthSections, (float)(Y+1) / (float)InWidthSections);
			InVertices[TopLeftIndex].UV0 = FVector2D((float)(X+1) / (float)InLengthSections, (float)Y / (float)InWidthSections);

			// Now create two triangles from those four vertices
			// The order of these (clockwise/counter-clockwise) dictates which way the normal will face. 
			InTriangles[TriangleIndex++] = BottomLeftIndex;
			InTriangles[TriangleIndex++] = TopRightIndex;
			InTriangles[TriangleIndex++] = TopLeftIndex;

			InTriangles[TriangleIndex++] = BottomLeftIndex;
			InTriangles[TriangleIndex++] = BottomRightIndex;
			InTriangles[TriangleIndex++] = TopRightIndex;

			// Normals
			FVector NormalCurrent = FVector::CrossProduct(InVertices[BottomLeftIndex].Position - InVertices[TopLeftIndex].Position, InVertices[TopLeftIndex].Position - InVertices[TopRightIndex].Position).GetSafeNormal();

			// If not smoothing we just set the vertex normal to the same normal as the polygon they belong to
			InVertices[BottomLeftIndex].Normal = InVertices[BottomRightIndex].Normal = InVertices[TopRightIndex].Normal = InVertices[TopLeftIndex].Normal = FPackedNormal(NormalCurrent);

			// Tangents (perpendicular to the surface)
			FVector SurfaceTangent = pBottomLeft - pBottomRight;
			SurfaceTangent = SurfaceTangent.GetSafeNormal();
			InVertices[BottomLeftIndex].Tangent = InVertices[BottomRightIndex].Tangent = InVertices[TopRightIndex].Tangent = InVertices[TopLeftIndex].Tangent = FPackedNormal(SurfaceTangent);
		}
	}
}


#if WITH_EDITOR
void AHeightFieldNoiseActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName MemberPropertyChanged = (PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None);

	if ((MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AHeightFieldNoiseActor, Size)) || (MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AHeightFieldNoiseActor, RandomSeed)))
	{
		// Same vert count, so just regen mesh with same buffers
		GenerateMesh();
	}
	else if ((MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AHeightFieldNoiseActor, LengthSections)) || (MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AHeightFieldNoiseActor, WidthSections)))
	{
		// Vertice count has changed, so reset buffer and then regen mesh
		Vertices.Empty();
		Triangles.Empty();
		HeightValues.Empty();
		bHaveBuffersBeenInitialized = false;
		GenerateMesh();
	}
	else if ((MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AHeightFieldNoiseActor, Material)))
	{
		MeshComponent->SetMaterial(0, Material);
	}
}
#endif // WITH_EDITOR
