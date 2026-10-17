// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example heightfield generated with noise

#include "HeightFieldNoiseActor.h"

AHeightFieldNoiseActor::AHeightFieldNoiseActor()
{
	PrimaryActorTick.bCanEverTick = false;
	MeshComponent = CreateDefaultSubobject<URuntimeProceduralMeshComponent>(TEXT("ProceduralMesh"));
	SetRootComponent(MeshComponent);
}

void AHeightFieldNoiseActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (bRequiresMeshRebuild || MeshComponent->GetNumSections() == 0)
	{
		GenerateMesh();
		bRequiresMeshRebuild = false;
	}
}

#if WITH_EDITOR
void AHeightFieldNoiseActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// A null MemberProperty during undo/redo means any of our properties may have changed. Elsewhere (actor
	// placement, paste) it follows construction directly, so rebuilding there would only duplicate work.
	const FProperty* MemberProperty = PropertyChangedEvent.MemberProperty;
	if (MemberProperty ? MemberProperty->GetOwnerClass()->IsChildOf(StaticClass()) : GIsTransacting)
	{
		bRequiresMeshRebuild = true;
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void AHeightFieldNoiseActor::PostLoad()
{
	Super::PostLoad();

	// Templates never render; generating here would also make new instances inherit the template's
	// mesh sections, so OnConstruction would skip generating from the instance's own parameters
	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return;
	}

	GenerateMesh();
	bRequiresMeshRebuild = false;
}

void AHeightFieldNoiseActor::SetupMeshBuffers()
{
	const int32 NumberOfPoints = (LengthSections + 1) * (WidthSections + 1);
	const int32 VertexCount = LengthSections * WidthSections * 6; // 2 triangles per quad/section, each with its own 3 vertices
	const int32 TriangleCount = LengthSections * WidthSections * 2 * 3; // 2x3 vertex indexes per quad

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

	if (NumberOfPoints != HeightValues.Num())
	{
		HeightValues.Empty();
		HeightValues.AddUninitialized(NumberOfPoints);
	}
}

void AHeightFieldNoiseActor::GeneratePoints()
{
	RngStream.Initialize(RandomSeed);

	// Setup example height data
	const int32 NumberOfPoints = (LengthSections + 1) * (WidthSections + 1);

	// Fill height data with random values
	for (int32 i = 0; i < NumberOfPoints; i++)
	{
		HeightValues[i] = RngStream.FRandRange(0, Size.Z);
	}
}

void AHeightFieldNoiseActor::GenerateMesh()
{
	if (!IsValid(MeshComponent))
	{
		return;
	}

	// ClampMin/ClampMax only limit the details panel; Blueprints can write any value
	LengthSections = FMath::Clamp(LengthSections, 1, 512);
	WidthSections = FMath::Clamp(WidthSections, 1, 512);

	MeshComponent->ClearAllMeshSections();

	if (Size.X < 1 || Size.Y < 1)
	{
		return;
	}

	SetupMeshBuffers();
	GeneratePoints();
	GenerateGrid(Positions, Triangles, Normals, Tangents, TexCoords, FVector2D(Size.X, Size.Y), LengthSections, WidthSections, HeightValues);

	MeshComponent->CreateMeshSection_LinearColor(0, Positions, Triangles, Normals, TexCoords, {}, {}, {}, {}, Tangents, false);
	// Also when null, so clearing Material removes the previous override
	MeshComponent->SetMaterial(0, Material);
}

void AHeightFieldNoiseActor::GenerateGrid(TArray<FVector>& InVertices, TArray<int32>& InTriangles, TArray<FVector>& InNormals, TArray<FProcMeshTangent>& InTangents, TArray<FVector2D>& InTexCoords, const FVector2D InSize, const int32 InLengthSections, const int32 InWidthSections, const TArray<float>& InHeightValues)
{
	// Note the coordinates are a bit weird here since I aligned it to the transform (X is forwards or "up", which Y is to the right)
	// Should really fix this up and use standard X, Y coords then transform into object space?
	const FVector2D SectionSize = FVector2D(InSize.X / InLengthSections, InSize.Y / InWidthSections);
	int32 VertexIndex = 0;
	int32 TriangleIndex = 0;

	for (int32 X = 0; X < InLengthSections; X++)
	{
		for (int32 Y = 0; Y < InWidthSections; Y++)
		{
			const int32 NoiseIndex_BottomLeft = (X * (InWidthSections + 1)) + Y;
			const int32 NoiseIndex_BottomRight = NoiseIndex_BottomLeft + 1;
			const int32 NoiseIndex_TopLeft = ((X + 1) * (InWidthSections + 1)) + Y;
			const int32 NoiseIndex_TopRight = NoiseIndex_TopLeft + 1;

			const FVector PBottomLeft = FVector(X * SectionSize.X, Y * SectionSize.Y, InHeightValues[NoiseIndex_BottomLeft]);
			const FVector PBottomRight = FVector(X * SectionSize.X, (Y+1) * SectionSize.Y, InHeightValues[NoiseIndex_BottomRight]);
			const FVector PTopRight = FVector((X + 1) * SectionSize.X, (Y + 1) * SectionSize.Y, InHeightValues[NoiseIndex_TopRight]);
			const FVector PTopLeft = FVector((X+1) * SectionSize.X, Y * SectionSize.Y, InHeightValues[NoiseIndex_TopLeft]);

			// Note that Unreal UV origin (0,0) is top left
			const FVector2D UVBottomLeft = FVector2D(static_cast<float>(X) / static_cast<float>(InLengthSections), static_cast<float>(Y) / static_cast<float>(InWidthSections));
			const FVector2D UVBottomRight = FVector2D(static_cast<float>(X) / static_cast<float>(InLengthSections), static_cast<float>(Y + 1) / static_cast<float>(InWidthSections));
			const FVector2D UVTopRight = FVector2D(static_cast<float>(X + 1) / static_cast<float>(InLengthSections), static_cast<float>(Y + 1) / static_cast<float>(InWidthSections));
			const FVector2D UVTopLeft = FVector2D(static_cast<float>(X + 1) / static_cast<float>(InLengthSections), static_cast<float>(Y) / static_cast<float>(InWidthSections));

			// Tangents (perpendicular to the surface)
			const FVector SurfaceTangentVec = (PBottomLeft - PBottomRight).GetSafeNormal();
			const FProcMeshTangent SurfaceTangent(SurfaceTangentVec, /*bFlipTangentY=*/ false);

			// Create two triangles for the quad, each with its own three vertices. Random heights make the quad's corners
			// non-coplanar, so the two triangles need separate flat normals: one normal for the whole quad can face far
			// enough away from a triangle to light it from behind.
			// The order of these (clockwise/counter-clockwise) dictates which way the normal will face.
			const FVector TrianglePoints[2][3] = { { PBottomLeft, PTopRight, PTopLeft }, { PBottomLeft, PBottomRight, PTopRight } };
			const FVector2D TriangleUVs[2][3] = { { UVBottomLeft, UVTopRight, UVTopLeft }, { UVBottomLeft, UVBottomRight, UVTopRight } };

			for (int32 TriangleInQuad = 0; TriangleInQuad < 2; TriangleInQuad++)
			{
				const FVector* Points = TrianglePoints[TriangleInQuad];
				const FVector NormalCurrent = FVector::CrossProduct(Points[2] - Points[0], Points[1] - Points[0]).GetSafeNormal();

				for (int32 Corner = 0; Corner < 3; Corner++)
				{
					const int32 NewVertIndex = VertexIndex++;
					InVertices[NewVertIndex] = Points[Corner];
					InTexCoords[NewVertIndex] = TriangleUVs[TriangleInQuad][Corner];
					InNormals[NewVertIndex] = NormalCurrent;
					InTangents[NewVertIndex] = SurfaceTangent;
					InTriangles[TriangleIndex++] = NewVertIndex;
				}
			}
		}
	}
}
