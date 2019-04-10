// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example heightfield grid animated with sine and cosine waves

#include "ProceduralMeshesPrivatePCH.h"
#include "HeightFieldAnimatedActor.h"

AHeightFieldAnimatedActor::AHeightFieldAnimatedActor()
{
	RootNode = CreateDefaultSubobject<USceneComponent>("Root");
	RootComponent = RootNode;

	MeshComponent = CreateDefaultSubobject<URuntimeMeshComponent>(TEXT("ProceduralMesh"));
	MeshComponent->GetOrCreateRuntimeMesh()->SetShouldSerializeMeshData(false);
	MeshComponent->SetupAttachment(RootComponent);

	PrimaryActorTick.bCanEverTick = true;
}

// This is called when actor is spawned (at runtime or when you drop it into the world in editor)
void AHeightFieldAnimatedActor::PostActorCreated()
{
	Super::PostActorCreated();
	GenerateMesh();
}

// This is called when actor is already in level and map is opened
void AHeightFieldAnimatedActor::PostLoad()
{
	Super::PostLoad();
	GenerateMesh();
}

void AHeightFieldAnimatedActor::SetupMeshBuffers()
{
	int32 NumberOfPoints = (LengthSections + 1) * (WidthSections + 1);
	int32 NumberOfTriangles = LengthSections * WidthSections * 2 * 3; // 2x3 vertex indexes per quad
	Vertices.AddUninitialized(NumberOfPoints);
	Triangles.AddUninitialized(NumberOfTriangles);
	HeightValues.AddUninitialized(NumberOfPoints);
}

void AHeightFieldAnimatedActor::GeneratePoints()
{
	// Setup example height data
	int32 NumberOfPoints = (LengthSections + 1) * (WidthSections + 1);

	// Combine variations of sine and cosine to create some variable waves
	// TODO Convert this to use a parallel for
	int32 PointIndex = 0;

	for (int32 X = 0; X < LengthSections + 1; X++)
	{
		for (int32 Y = 0; Y < WidthSections + 1; Y++)
		{
			// Just some quick hardcoded offset numbers in there
			float ValueOne = FMath::Cos((X + CurrentAnimationFrameX)*ScaleFactor) * FMath::Sin((Y + CurrentAnimationFrameY)*ScaleFactor);
			float ValueTwo = FMath::Cos((X + CurrentAnimationFrameX*0.7f)*ScaleFactor*2.5f) * FMath::Sin((Y - CurrentAnimationFrameY*0.7f)*ScaleFactor*2.5f);
			float AvgValue = ((ValueOne + ValueTwo) / 2) * Size.Z;
			HeightValues[PointIndex++] = AvgValue;

			if (AvgValue > MaxHeightValue)
			{
				MaxHeightValue = AvgValue;
			}
		}
	}
}

void AHeightFieldAnimatedActor::Tick(float DeltaSeconds)
{
	if (AnimateMesh)
	{
		CurrentAnimationFrameX += DeltaSeconds * AnimationSpeedX;
		CurrentAnimationFrameY += DeltaSeconds * AnimationSpeedY;
		GenerateMesh();
	}
}

void AHeightFieldAnimatedActor::GenerateMesh()
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

	// TODO Convert this to use fast-past updates instead of regenerating the mesh every frame
	GenerateGrid(Vertices, Triangles, FVector2D(Size.X, Size.Y), LengthSections, WidthSections, HeightValues);
	FBox BoundingBox = FBox(FVector(0, 0, -MaxHeightValue), FVector(Size.X, Size.Y, MaxHeightValue));
	MeshComponent->ClearAllMeshSections();
	MeshComponent->CreateMeshSection(0, Vertices, Triangles, BoundingBox, false, EUpdateFrequency::Frequent);
	MeshComponent->SetMaterial(0, Material);
}

void AHeightFieldAnimatedActor::GenerateGrid(TArray<FRuntimeMeshVertexSimple>& InVertices, TArray<int32>& InTriangles, FVector2D InSize, int32 InLengthSections, int32 InWidthSections, const TArray<float>& InHeightValues)
{
	// Note the coordinates are a bit weird here since I aligned it to the transform (X is forwards or "up", which Y is to the right)
	// Should really fix this up and use standard X, Y coords then transform into object space?
	FVector2D SectionSize = FVector2D(InSize.X / InLengthSections, InSize.Y / InWidthSections);
	int32 VertexIndex = 0;
	int32 TriangleIndex = 0;

	for (int32 X = 0; X < InLengthSections + 1; X++)
	{
		for (int32 Y = 0; Y < InWidthSections + 1; Y++)
		{
			// Create a new vertex
			int32 NewVertIndex = VertexIndex++;
			FVector newVertex = FVector(X * SectionSize.X, Y * SectionSize.Y, InHeightValues[NewVertIndex]);
			InVertices[NewVertIndex].Position = newVertex;

			// Note that Unreal UV origin (0,0) is top left
			float U = (float)X / (float)InLengthSections;
			float V = (float)Y / (float)InWidthSections;
			InVertices[NewVertIndex].UV0 = FVector2D(U, V);

			// Once we've created enough verts we can start adding polygons
			if (X > 0 && Y > 0)
			{
				// Each row is InWidthSections+1 number of points.
				// And we have InLength+1 rows
				// Index of current vertex in position is thus: (X * (InWidthSections + 1)) + Y;
				int32 bTopRightIndex = (X * (InWidthSections + 1)) + Y; // Should be same as VertIndex1!
				int32 bTopLeftIndex = bTopRightIndex - 1;
				int32 pBottomRightIndex = ((X - 1) * (InWidthSections + 1)) + Y;
				int32 pBottomLeftIndex = pBottomRightIndex - 1;

				// Now create two triangles from those four vertices
				// The order of these (clockwise/counter-clockwise) dictates which way the normal will face. 
				InTriangles[TriangleIndex++] = pBottomLeftIndex;
				InTriangles[TriangleIndex++] = bTopRightIndex;
				InTriangles[TriangleIndex++] = bTopLeftIndex;

				InTriangles[TriangleIndex++] = pBottomLeftIndex;
				InTriangles[TriangleIndex++] = pBottomRightIndex;
				InTriangles[TriangleIndex++] = bTopRightIndex;

				// Normals
				FVector NormalCurrent = FVector::CrossProduct(InVertices[pBottomLeftIndex].Position - InVertices[bTopLeftIndex].Position, InVertices[bTopLeftIndex].Position - InVertices[bTopRightIndex].Position).GetSafeNormal();
				InVertices[pBottomLeftIndex].Normal = InVertices[pBottomRightIndex].Normal = InVertices[bTopRightIndex].Normal = InVertices[bTopLeftIndex].Normal = FPackedNormal(NormalCurrent);
			}
		}
	}
}

#if WITH_EDITOR
void AHeightFieldAnimatedActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName MemberPropertyChanged = (PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None);

	if ((MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AHeightFieldAnimatedActor, Size)) || (MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AHeightFieldAnimatedActor, ScaleFactor)))
	{
		// Same vert count, so just regen mesh with same buffers
		GenerateMesh();
	}
	else if ((MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AHeightFieldAnimatedActor, LengthSections)) || (MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AHeightFieldAnimatedActor, WidthSections)))
	{
		// Vertice count has changed, so reset buffer and then regen mesh
		Vertices.Empty();
		Triangles.Empty();
		HeightValues.Empty();
		bHaveBuffersBeenInitialized = false;
		GenerateMesh();
	}
	else if ((MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(AHeightFieldAnimatedActor, Material)))
	{
		MeshComponent->SetMaterial(0, Material);
	}
}
#endif // WITH_EDITOR
