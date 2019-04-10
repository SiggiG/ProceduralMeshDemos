// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example cylinder mesh

#include "ProceduralMeshesPrivatePCH.h"
#include "SimpleCylinderActor.h"

ASimpleCylinderActor::ASimpleCylinderActor()
{
	RootNode = CreateDefaultSubobject<USceneComponent>("Root");
	RootComponent = RootNode;

	MeshComponent = CreateDefaultSubobject<URuntimeMeshComponent>(TEXT("ProceduralMesh"));
	MeshComponent->GetOrCreateRuntimeMesh()->SetShouldSerializeMeshData(false);
	MeshComponent->SetupAttachment(RootComponent);
}

// This is called when actor is spawned (at runtime or when you drop it into the world in editor)
void ASimpleCylinderActor::PostActorCreated()
{
	Super::PostActorCreated();
	GenerateMesh();
}

// This is called when actor is already in level and map is opened
void ASimpleCylinderActor::PostLoad()
{
	Super::PostLoad();
	GenerateMesh();
}

void ASimpleCylinderActor::SetupMeshBuffers()
{
	int32 VertexCount = RadialSegmentCount * 4; // 4 verts per face
	int32 TriangleCount = RadialSegmentCount * 2 * 3; // 2 triangles per face

	// Count extra vertices if double sided
	if (bDoubleSided)
	{
		VertexCount = VertexCount * 2;
		TriangleCount = TriangleCount * 2;
	}

	// Count vertices for caps if set
	if (bCapEnds)
	{
		// Each cap adds as many verts as there are verts in a circle
		// 2x Number of vertices x3
		VertexCount += 2 * (RadialSegmentCount - 1) * 3;
		TriangleCount += 2 * (RadialSegmentCount - 1) * 3;
	}

	Vertices.AddUninitialized(VertexCount);
	Triangles.AddUninitialized(TriangleCount);
}

void ASimpleCylinderActor::GenerateMesh()
{
	if (Height <= 0)
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

	FBox BoundingBox = FBox(FVector(-Radius, -Radius, 0), FVector(Radius, Radius, Height));
	GenerateCylinder(Vertices, Triangles, Height, Radius, RadialSegmentCount, bCapEnds, bDoubleSided, bSmoothNormals);
	
	MeshComponent->ClearAllMeshSections();
	MeshComponent->CreateMeshSection(0, Vertices, Triangles, BoundingBox, false, EUpdateFrequency::Infrequent);
	MeshComponent->SetMaterial(0, Material);
}

void ASimpleCylinderActor::GenerateCylinder(TArray<FRuntimeMeshVertexSimple>& InVertices, TArray<int32>& InTriangles, float InHeight, float InWidth, int32 InCrossSectionCount, bool bInCapEnds, bool bInDoubleSided, bool bInSmoothNormals/* = true*/)
{
	// -------------------------------------------------------
	// Basic setup
	int32 VertexIndex = 0;
	int32 TriangleIndex = 0;

	// -------------------------------------------------------
	// Make a cylinder section
	const float AngleBetweenQuads = (2.0f / (float)(InCrossSectionCount)) * PI;
	const float VMapPerQuad = 1.0f / (float)InCrossSectionCount;
	FVector Offset = FVector(0, 0, InHeight);

	// Start by building up vertices that make up the cylinder sides
	for (int32 QuadIndex = 0; QuadIndex < InCrossSectionCount; QuadIndex++)
	{
		float Angle = (float)QuadIndex * AngleBetweenQuads;
		float NextAngle = (float)(QuadIndex + 1) * AngleBetweenQuads;

		// Set up the vertices
		FVector p0 = FVector(FMath::Cos(Angle) * InWidth, FMath::Sin(Angle) * InWidth, 0.f);
		FVector p1 = FVector(FMath::Cos(NextAngle) * InWidth, FMath::Sin(NextAngle) * InWidth, 0.f);
		FVector p2 = p1 + Offset;
		FVector p3 = p0 + Offset;

		// Set up the quad triangles
		int32 VertIndex1 = VertexIndex++;
		int32 VertIndex2 = VertexIndex++;
		int32 VertIndex3 = VertexIndex++;
		int32 VertIndex4 = VertexIndex++;

		InVertices[VertIndex1].Position = p0;
		InVertices[VertIndex2].Position = p1;
		InVertices[VertIndex3].Position = p2;
		InVertices[VertIndex4].Position = p3;

		// Now create two triangles from those four vertices
		// The order of these (clockwise/counter-clockwise) dictates which way the normal will face. 
		InTriangles[TriangleIndex++] = VertIndex4;
		InTriangles[TriangleIndex++] = VertIndex3;
		InTriangles[TriangleIndex++] = VertIndex1;

		InTriangles[TriangleIndex++] = VertIndex3;
		InTriangles[TriangleIndex++] = VertIndex2;
		InTriangles[TriangleIndex++] = VertIndex1;

		// UVs.  Note that Unreal UV origin (0,0) is top left
		InVertices[VertIndex1].UV0 = FVector2D(1.0f - (VMapPerQuad * QuadIndex), 1.0f);
		InVertices[VertIndex2].UV0 = FVector2D(1.0f - (VMapPerQuad * (QuadIndex + 1)), 1.0f);
		InVertices[VertIndex3].UV0 = FVector2D(1.0f - (VMapPerQuad * (QuadIndex + 1)), 0.0f);
		InVertices[VertIndex4].UV0 = FVector2D(1.0f - (VMapPerQuad * QuadIndex), 0.0f);

		// Normals
		FVector NormalCurrent = FVector::CrossProduct(InVertices[VertIndex1].Position - InVertices[VertIndex3].Position, InVertices[VertIndex2].Position - InVertices[VertIndex3].Position).GetSafeNormal();

		if (bInSmoothNormals)
		{
			// To smooth normals we give the vertices different values than the polygon they belong to.
			// GPUs know how to interpolate between those.
			// I do this here as an average between normals of two adjacent polygons
			float NextNextAngle = (float)(QuadIndex + 2) * AngleBetweenQuads;
			FVector p4 = FVector(FMath::Cos(NextNextAngle) * InWidth, FMath::Sin(NextNextAngle) * InWidth, 0.f);

			// p1 to p4 to p2
			FVector NormalNext = FVector::CrossProduct(p1 - p2, p4 - p2).GetSafeNormal();
			FVector AverageNormalRight = (NormalCurrent + NormalNext) / 2;
			AverageNormalRight = AverageNormalRight.GetSafeNormal();

			float PreviousAngle = (float)(QuadIndex - 1) * AngleBetweenQuads;
			FVector pMinus1 = FVector(FMath::Cos(PreviousAngle) * InWidth, FMath::Sin(PreviousAngle) * InWidth, 0.f);

			// p0 to p3 to pMinus1
			FVector NormalPrevious = FVector::CrossProduct(p0 - pMinus1, p3 - pMinus1).GetSafeNormal();
			FVector AverageNormalLeft = (NormalCurrent + NormalPrevious) / 2;
			AverageNormalLeft = AverageNormalLeft.GetSafeNormal();

			InVertices[VertIndex1].Normal = FPackedNormal(AverageNormalLeft);
			InVertices[VertIndex2].Normal = FPackedNormal(AverageNormalRight);
			InVertices[VertIndex3].Normal = FPackedNormal(AverageNormalRight);
			InVertices[VertIndex4].Normal = FPackedNormal(AverageNormalLeft);
		}
		else
		{
			// If not smoothing we just set the vertex normal to the same normal as the polygon they belong to
			InVertices[VertIndex1].Normal = InVertices[VertIndex2].Normal = InVertices[VertIndex3].Normal = InVertices[VertIndex4].Normal = FPackedNormal(NormalCurrent);
		}

		// Tangents (perpendicular to the surface)
		FVector SurfaceTangent = p0 - p1;
		SurfaceTangent = SurfaceTangent.GetSafeNormal();
		InVertices[VertIndex1].Tangent = InVertices[VertIndex2].Tangent = InVertices[VertIndex3].Tangent = InVertices[VertIndex4].Tangent = FPackedNormal(SurfaceTangent);

		// -------------------------------------------------------
		// If double sided, create extra polygons but face the normals the other way.
		if (bInDoubleSided)
		{
			VertIndex1 = VertexIndex++;
			VertIndex2 = VertexIndex++;
			VertIndex3 = VertexIndex++;
			VertIndex4 = VertexIndex++;

			InVertices[VertIndex1].Position = p0;
			InVertices[VertIndex2].Position = p1;
			InVertices[VertIndex3].Position = p2;
			InVertices[VertIndex4].Position = p3;

			// Reverse the poly order to face them the other way
			InTriangles[TriangleIndex++] = VertIndex4;
			InTriangles[TriangleIndex++] = VertIndex1;
			InTriangles[TriangleIndex++] = VertIndex3;

			InTriangles[TriangleIndex++] = VertIndex3;
			InTriangles[TriangleIndex++] = VertIndex1;
			InTriangles[TriangleIndex++] = VertIndex2;

			// UVs  (Unreal 1,1 is top left)
			InVertices[VertIndex1].UV0 = FVector2D(1.0f - (VMapPerQuad * QuadIndex), 1.0f);
			InVertices[VertIndex2].UV0 = FVector2D(1.0f - (VMapPerQuad * (QuadIndex + 1)), 1.0f);
			InVertices[VertIndex3].UV0 = FVector2D(1.0f - (VMapPerQuad * (QuadIndex + 1)), 0.0f);
			InVertices[VertIndex4].UV0 = FVector2D(1.0f - (VMapPerQuad * QuadIndex), 0.0f);

			// Just simple (unsmoothed) normal for these
			InVertices[VertIndex1].Normal = InVertices[VertIndex2].Normal = InVertices[VertIndex3].Normal = InVertices[VertIndex4].Normal = FPackedNormal(NormalCurrent);

			// Tangents (perpendicular to the surface)
			FVector SurfaceTangentDbl = p0 - p1;
			SurfaceTangentDbl = SurfaceTangentDbl.GetSafeNormal();
			InVertices[VertIndex1].Tangent = InVertices[VertIndex2].Tangent = InVertices[VertIndex3].Tangent = InVertices[VertIndex4].Tangent = FPackedNormal(SurfaceTangentDbl);
		}

		// -------------------------------------------------------
		// Caps are closed here by triangles that start at 0, then use the points along the circle for the other two corners.
		// A better looking method uses a vertex in the center of the circle, but uses two more polygons.  We will demonstrate that in a different sample.
		if (QuadIndex != 0 && QuadIndex != InCrossSectionCount - 1 && bInCapEnds)
		{
			// Bottom cap
			FVector capVertex0 = FVector(FMath::Cos(0) * InWidth, FMath::Sin(0) * InWidth, 0.f);
			FVector capVertex1 = FVector(FMath::Cos(Angle) * InWidth, FMath::Sin(Angle) * InWidth, 0.f);
			FVector capVertex2 = FVector(FMath::Cos(NextAngle) * InWidth, FMath::Sin(NextAngle) * InWidth, 0.f);

			VertIndex1 = VertexIndex++;
			VertIndex2 = VertexIndex++;
			VertIndex3 = VertexIndex++;
			InVertices[VertIndex1].Position = capVertex0;
			InVertices[VertIndex2].Position = capVertex1;
			InVertices[VertIndex3].Position = capVertex2;

			InTriangles[TriangleIndex++] = VertIndex1;
			InTriangles[TriangleIndex++] = VertIndex2;
			InTriangles[TriangleIndex++] = VertIndex3;

			InVertices[VertIndex1].UV0 = FVector2D(0.5f - (FMath::Cos(0) / 2.0f), 0.5f - (FMath::Sin(0) / 2.0f));
			InVertices[VertIndex2].UV0 = FVector2D(0.5f - (FMath::Cos(-Angle) / 2.0f), 0.5f - (FMath::Sin(-Angle) / 2.0f));
			InVertices[VertIndex3].UV0 = FVector2D(0.5f - (FMath::Cos(-NextAngle) / 2.0f), 0.5f - (FMath::Sin(-NextAngle) / 2.0f));

			FVector CapNormalCurrent = FVector::CrossProduct(InVertices[VertIndex1].Position - InVertices[VertIndex3].Position, InVertices[VertIndex2].Position - InVertices[VertIndex3].Position).GetSafeNormal();
			InVertices[VertIndex1].Normal = InVertices[VertIndex2].Normal = InVertices[VertIndex3].Normal = FPackedNormal(CapNormalCurrent);

			// Tangents (perpendicular to the surface)
			FVector SurfaceTangentCap = p0 - p1;
			SurfaceTangentCap = SurfaceTangentCap.GetSafeNormal();
			InVertices[VertIndex1].Tangent = InVertices[VertIndex2].Tangent = InVertices[VertIndex3].Tangent = FPackedNormal(SurfaceTangentCap);

			// Top cap
			capVertex0 = capVertex0 + Offset;
			capVertex1 = capVertex1 + Offset;
			capVertex2 = capVertex2 + Offset;

			VertIndex1 = VertexIndex++;
			VertIndex2 = VertexIndex++;
			VertIndex3 = VertexIndex++;
			InVertices[VertIndex1].Position = capVertex0;
			InVertices[VertIndex2].Position = capVertex1;
			InVertices[VertIndex3].Position = capVertex2;

			InTriangles[TriangleIndex++] = VertIndex3;
			InTriangles[TriangleIndex++] = VertIndex2;
			InTriangles[TriangleIndex++] = VertIndex1;

			InVertices[VertIndex1].UV0 = FVector2D(0.5f - (FMath::Cos(0) / 2.0f), 0.5f - (FMath::Sin(0) / 2.0f));
			InVertices[VertIndex2].UV0 = FVector2D(0.5f - (FMath::Cos(Angle) / 2.0f), 0.5f - (FMath::Sin(Angle) / 2.0f));
			InVertices[VertIndex3].UV0 = FVector2D(0.5f - (FMath::Cos(NextAngle) / 2.0f), 0.5f - (FMath::Sin(NextAngle) / 2.0f));

			CapNormalCurrent = FVector::CrossProduct(InVertices[VertIndex1].Position - InVertices[VertIndex3].Position, InVertices[VertIndex2].Position - InVertices[VertIndex3].Position).GetSafeNormal();
			InVertices[VertIndex1].Normal = InVertices[VertIndex2].Normal = InVertices[VertIndex3].Normal = FPackedNormal(CapNormalCurrent);

			// Tangents (perpendicular to the surface)
			SurfaceTangentCap = p0 - p1;
			SurfaceTangentCap = SurfaceTangentCap.GetSafeNormal();
			InVertices[VertIndex1].Tangent = InVertices[VertIndex2].Tangent = InVertices[VertIndex3].Tangent = FPackedNormal(SurfaceTangentCap);
		}
	}
}

#if WITH_EDITOR
void ASimpleCylinderActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName MemberPropertyChanged = (PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None);

	if ((MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(ASimpleCylinderActor, Radius)) || (MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(ASimpleCylinderActor, Height)) || (MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(ASimpleCylinderActor, bSmoothNormals)))
	{
		// Same vert count, so just regen mesh with same buffers
		GenerateMesh();
	}
	else if ((MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(ASimpleCylinderActor, RadialSegmentCount)) || (MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(ASimpleCylinderActor, bCapEnds)) || (MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(ASimpleCylinderActor, bDoubleSided)))
	{
		// Vertice count has changed, so reset buffer and then regen mesh
		Vertices.Empty();
		Triangles.Empty();
		SetupMeshBuffers();
		GenerateMesh();
	}
	else if ((MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(ASimpleCylinderActor, Material)))
	{
		MeshComponent->SetMaterial(0, Material);
	}
}
#endif // WITH_EDITOR
