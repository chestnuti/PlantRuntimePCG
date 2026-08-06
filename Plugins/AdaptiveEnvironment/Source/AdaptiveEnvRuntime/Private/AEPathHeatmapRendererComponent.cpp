#include "AEPathHeatmapRendererComponent.h"

#include "AdaptiveEnvLog.h"
#include "AdaptiveEnvSettings.h"
#include "AdaptiveEnvWorldSubsystem.h"
#include "CanvasItem.h"
#include "Components/MeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "RenderUtils.h"

namespace AEPathHeatmapRendererPrivate
{
	/* Append one untextured triangle with an explicit RGBA vertex value. */
	void AddSolidTriangle(
		TArray<FCanvasUVTri>& Triangles,
		const FVector2D& A,
		const FVector2D& B,
		const FVector2D& C,
		const FLinearColor& Color)
	{
		FCanvasUVTri& Triangle = Triangles.AddDefaulted_GetRef();
		Triangle.V0_Pos = A;
		Triangle.V1_Pos = B;
		Triangle.V2_Pos = C;
		Triangle.V0_UV = FVector2D::ZeroVector;
		Triangle.V1_UV = FVector2D::ZeroVector;
		Triangle.V2_UV = FVector2D::ZeroVector;
		Triangle.V0_Color = Color;
		Triangle.V1_Color = Color;
		Triangle.V2_Color = Color;
	}

	/* Reports whether one material parameter list contains an exact configured name. */
	bool ContainsParameterName(
		const TConstArrayView<FMaterialParameterInfo> Parameters,
		const FName RequiredName)
	{
		return Parameters.ContainsByPredicate(
			[RequiredName](const FMaterialParameterInfo& Parameter)
			{
				return Parameter.Name == RequiredName;
			});
	}
}

/* Create a visual consumer whose scheduling remains owned by the World Subsystem. */
UAEPathHeatmapRendererComponent::UAEPathHeatmapRendererComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

/* Register this renderer with the current World Subsystem. */
void UAEPathHeatmapRendererComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* World = GetWorld())
	{
		if (UAEAdaptiveEnvWorldSubsystem* Subsystem =
			World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>())
		{
			Subsystem->RegisterPathHeatmapRenderer(this);
		}
	}
}

/* Disable the material contract and unregister before component teardown. */
void UAEPathHeatmapRendererComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	DisableMaterialOutput();
	ReleaseMeshMaterialBindings();
	BoundLandscape.Reset();
	BoundMaterialOutputCount = 0;
	if (UWorld* World = GetWorld())
	{
		if (UAEAdaptiveEnvWorldSubsystem* Subsystem =
			World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>())
		{
			Subsystem->UnregisterPathHeatmapRenderer(this);
		}
	}
	PendingCommands.Reset();
	PendingCommandPositions.Reset();
	EncodedCellValues.Reset();
	PathStateRenderTarget = nullptr;
	PathVisualRenderTarget = nullptr;
	Super::EndPlay(EndPlayReason);
}

/* Create separate Cell-state and supersampled material-facing Render Targets. */
bool UAEPathHeatmapRendererComponent::InitializeVisualOutput(
	const FIntPoint& GridDimensions,
	const FBox2D& GridWorldBounds)
{
	check(IsInGameThread());
	if (GridDimensions.X <= 0 || GridDimensions.Y <= 0
		|| !GridWorldBounds.bIsValid)
	{
		DisableMaterialOutput();
		return false;
	}

	const UAdaptiveEnvSettings* Settings = GetDefault<UAdaptiveEnvSettings>();
	const int32 VisualPixelsPerCell = FMath::Clamp(Settings->M6VisualPixelsPerCell, 1, 8);
	const int64 VisualWidth = static_cast<int64>(GridDimensions.X) * VisualPixelsPerCell;
	const int64 VisualHeight = static_cast<int64>(GridDimensions.Y) * VisualPixelsPerCell;
	if (VisualWidth > 16384 || VisualHeight > 16384)
	{
		UE_LOG(
			LogAdaptiveEnv,
			Error,
			TEXT("M6 supersampled Render Target exceeds the supported 16384-pixel edge. Grid=%dx%d PixelsPerCell=%d"),
			GridDimensions.X,
			GridDimensions.Y,
			VisualPixelsPerCell);
		DisableMaterialOutput();
		return false;
	}

	const FLinearColor NeutralVisualValue(128.0f / 255.0f, 128.0f / 255.0f, 0.0f, 0.0f);
	StateTextureDimensions = GridDimensions;
	VisualTextureDimensions = FIntPoint(
		static_cast<int32>(VisualWidth),
		static_cast<int32>(VisualHeight));
	PathStateRenderTarget = NewObject<UTextureRenderTarget2D>(this);
	PathVisualRenderTarget = NewObject<UTextureRenderTarget2D>(this);
	if (!IsValid(PathStateRenderTarget) || !IsValid(PathVisualRenderTarget))
	{
		return false;
	}
	PathStateRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
	PathStateRenderTarget->ClearColor = NeutralVisualValue;
	PathStateRenderTarget->Filter = TF_Nearest;
	PathStateRenderTarget->bAutoGenerateMips = false;
	PathStateRenderTarget->InitAutoFormat(StateTextureDimensions.X, StateTextureDimensions.Y);
	PathStateRenderTarget->UpdateResourceImmediate(true);
	PathVisualRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
	PathVisualRenderTarget->ClearColor = NeutralVisualValue;
	PathVisualRenderTarget->Filter = TF_Bilinear;
	PathVisualRenderTarget->bAutoGenerateMips = false;
	PathVisualRenderTarget->InitAutoFormat(VisualTextureDimensions.X, VisualTextureDimensions.Y);
	PathVisualRenderTarget->UpdateResourceImmediate(true);
	UKismetRenderingLibrary::ClearRenderTarget2D(this, PathStateRenderTarget, NeutralVisualValue);
	UKismetRenderingLibrary::ClearRenderTarget2D(this, PathVisualRenderTarget, NeutralVisualValue);

	// Encode the stable World XY to texture UV transform.
	const FVector2D WorldSize = GridWorldBounds.GetSize();
	if (WorldSize.X <= UE_DOUBLE_SMALL_NUMBER || WorldSize.Y <= UE_DOUBLE_SMALL_NUMBER)
	{
		PathStateRenderTarget = nullptr;
		PathVisualRenderTarget = nullptr;
		return false;
	}
	GridCellSizeCm = static_cast<float>(FMath::Min(
		WorldSize.X / GridDimensions.X,
		WorldSize.Y / GridDimensions.Y));
	GridTransform = FLinearColor(
		GridWorldBounds.Min.X,
		GridWorldBounds.Min.Y,
		1.0 / WorldSize.X,
		1.0 / WorldSize.Y);
	PendingCommands.Reset();
	PendingCommandPositions.Reset();
	EncodedCellValues.Init(FColor(128, 128, 0, 0), GridDimensions.X * GridDimensions.Y);
	BindMaterialOutputs();
	return true;
}

/* Rebind configured outputs to the current Render Target without changing its contents. */
bool UAEPathHeatmapRendererComponent::RefreshMaterialBindings()
{
	check(IsInGameThread());
	if (!IsValid(PathVisualRenderTarget))
	{
		UE_LOG(
			LogAdaptiveEnv,
			Warning,
			TEXT("M6 material outputs cannot refresh before Render Target initialization. World=%s Owner=%s"),
			*GetNameSafe(GetWorld()),
			*GetNameSafe(GetOwner()));
		return false;
	}
	return BindMaterialOutputs() > 0;
}

/* Keep only the newest command for each Cell while preserving bounded storage. */
void UAEPathHeatmapRendererComponent::EnqueueVisualCommands(
	const TConstArrayView<FAEPathHeatmapVisualCommand> Commands)
{
	for (const FAEPathHeatmapVisualCommand& Command : Commands)
	{
		if (Command.CellIndex < 0
			|| Command.Coordinate.X < 0 || Command.Coordinate.Y < 0
			|| Command.Coordinate.X >= StateTextureDimensions.X
			|| Command.Coordinate.Y >= StateTextureDimensions.Y)
		{
			continue;
		}
		if (const int32* ExistingPosition = PendingCommandPositions.Find(Command.CellIndex))
		{
			FAEPathHeatmapVisualCommand& Existing = PendingCommands[*ExistingPosition];
			if (Command.PathVisualRevision >= Existing.PathVisualRevision)
			{
				Existing = Command;
			}
		}
		else
		{
			const int32 NewPosition = PendingCommands.Add(Command);
			PendingCommandPositions.Add(Command.CellIndex, NewPosition);
		}
	}
}

/* Draw a bounded stable prefix of one-texel updates into the persistent Render Target. */
void UAEPathHeatmapRendererComponent::ApplyVisualBudget(
	const int32 MaxCommands)
{
	check(IsInGameThread());
	if (!IsValid(PathStateRenderTarget) || !IsValid(PathVisualRenderTarget)
		|| MaxCommands <= 0 || PendingCommands.IsEmpty())
	{
		return;
	}

	// Open one Canvas pass so every applied command updates the same texture resource.
	UCanvas* Canvas = nullptr;
	FVector2D RenderTargetSize;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(
		this,
		PathStateRenderTarget,
		Canvas,
		RenderTargetSize,
		Context);
	if (Canvas == nullptr || Canvas->Canvas == nullptr)
	{
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
		return;
	}

	const int32 ApplyCount = FMath::Min(MaxCommands, PendingCommands.Num());
	for (int32 CommandIndex = 0; CommandIndex < ApplyCount; ++CommandIndex)
	{
		const FAEPathHeatmapVisualCommand& Command = PendingCommands[CommandIndex];
		if (EncodedCellValues.IsValidIndex(Command.CellIndex))
		{
			EncodedCellValues[Command.CellIndex] = Command.EncodedValue;
		}
		// Preserve the shared Grid XY orientation so material UVs need no hidden axis correction.
		const int32 PixelY = Command.Coordinate.Y;
		const FLinearColor EncodedValue(
			static_cast<float>(Command.EncodedValue.R) / 255.0f,
			static_cast<float>(Command.EncodedValue.G) / 255.0f,
			static_cast<float>(Command.EncodedValue.B) / 255.0f,
			static_cast<float>(Command.EncodedValue.A) / 255.0f);
		FCanvasTileItem Tile(
			FVector2D(Command.Coordinate.X, PixelY),
			FVector2D(1.0, 1.0),
			EncodedValue);
		Tile.BlendMode = SE_BLEND_Opaque;
		Canvas->Canvas->DrawItem(Tile);
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
	RebuildVisualOutput();

	// Remove the applied stable prefix and rebuild lookup positions.
	PendingCommands.RemoveAt(0, ApplyCount, EAllowShrinking::No);
	RebuildPendingCommandLookup();
}

/* Clear transient visual state and turn off material sampling. */
void UAEPathHeatmapRendererComponent::ResetVisualOutput()
{
	check(IsInGameThread());
	PendingCommands.Reset();
	PendingCommandPositions.Reset();
	EncodedCellValues.Init(FColor(128, 128, 0, 0), EncodedCellValues.Num());
	if (IsValid(PathStateRenderTarget))
	{
		const FLinearColor NeutralVisualValue(128.0f / 255.0f, 128.0f / 255.0f, 0.0f, 0.0f);
		UKismetRenderingLibrary::ClearRenderTarget2D(
			this,
			PathStateRenderTarget,
			NeutralVisualValue);
	}
	if (IsValid(PathVisualRenderTarget))
	{
		const FLinearColor NeutralVisualValue(128.0f / 255.0f, 128.0f / 255.0f, 0.0f, 0.0f);
		UKismetRenderingLibrary::ClearRenderTarget2D(
			this,
			PathVisualRenderTarget,
			NeutralVisualValue);
	}
	DisableMaterialOutput();
}

/* Draw fixed-width Flow-oriented capsules from cached Cell state. */
void UAEPathHeatmapRendererComponent::RebuildVisualOutput()
{
	check(IsInGameThread());
	if (!IsValid(PathVisualRenderTarget)
		|| StateTextureDimensions.X <= 0 || StateTextureDimensions.Y <= 0
		|| VisualTextureDimensions.X <= 0 || VisualTextureDimensions.Y <= 0
		|| GridCellSizeCm <= UE_SMALL_NUMBER)
	{
		return;
	}

	const FLinearColor NeutralVisualValue(128.0f / 255.0f, 128.0f / 255.0f, 0.0f, 0.0f);
	UKismetRenderingLibrary::ClearRenderTarget2D(
		this,
		PathVisualRenderTarget,
		NeutralVisualValue);

	UCanvas* Canvas = nullptr;
	FVector2D RenderTargetSize;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(
		this,
		PathVisualRenderTarget,
		Canvas,
		RenderTargetSize,
		Context);
	if (Canvas == nullptr || Canvas->Canvas == nullptr)
	{
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
		return;
	}

	const UAdaptiveEnvSettings* Settings = GetDefault<UAdaptiveEnvSettings>();
	const float PixelsPerCell = static_cast<float>(VisualTextureDimensions.X)
		/ StateTextureDimensions.X;
	const float PathRadiusPixels = FMath::Max(
		Settings->M6PathHalfWidthCm / GridCellSizeCm * PixelsPerCell,
		0.5f);
	const float HalfLengthPixels = FMath::Clamp(
		Settings->M6SegmentHalfLengthCells,
		0.5f,
		1.0f) * PixelsPerCell;

	TArray<int32> ActiveIndices;
	ActiveIndices.Reserve(EncodedCellValues.Num());
	for (int32 CellIndex = 0; CellIndex < EncodedCellValues.Num(); ++CellIndex)
	{
		if (EncodedCellValues[CellIndex].A > 0)
		{
			ActiveIndices.Add(CellIndex);
		}
	}
	ActiveIndices.Sort(
		[this](const int32 Left, const int32 Right)
		{
			return EncodedCellValues[Left].A < EncodedCellValues[Right].A;
		});

	for (const int32 CellIndex : ActiveIndices)
	{
		const FColor EncodedValue = EncodedCellValues[CellIndex];
		const FIntPoint Coordinate(
			CellIndex % StateTextureDimensions.X,
			CellIndex / StateTextureDimensions.X);
		const FVector2D Center(
			(Coordinate.X + 0.5) * PixelsPerCell,
			(Coordinate.Y + 0.5) * PixelsPerCell);
		const FVector2D Flow(
			static_cast<float>(EncodedValue.R) / 255.0f * 2.0f - 1.0f,
			static_cast<float>(EncodedValue.G) / 255.0f * 2.0f - 1.0f);
		const float FlowMagnitude = Flow.Size();
		const FVector2D Direction = FlowMagnitude > (2.0f / 255.0f)
			? Flow / FlowMagnitude
			: FVector2D(1.0f, 0.0f);
		const float EffectiveHalfLength = FlowMagnitude > (2.0f / 255.0f)
			? HalfLengthPixels
			: FMath::Max(PathRadiusPixels * 0.05f, 0.05f);
		const FVector2D Offset = Direction * EffectiveHalfLength;
		const FLinearColor DrawColor(
			static_cast<float>(EncodedValue.R) / 255.0f,
			static_cast<float>(EncodedValue.G) / 255.0f,
			0.0f,
			static_cast<float>(EncodedValue.A) / 255.0f);

		const FVector2D Perpendicular(-Direction.Y, Direction.X);
		const FVector2D RadiusOffset = Perpendicular * PathRadiusPixels;
		const FVector2D Start = Center - Offset;
		const FVector2D End = Center + Offset;
		const FVector2D StartLeft = Start - RadiusOffset;
		const FVector2D StartRight = Start + RadiusOffset;
		const FVector2D EndLeft = End - RadiusOffset;
		const FVector2D EndRight = End + RadiusOffset;

		TArray<FCanvasUVTri> SegmentTriangles;
		SegmentTriangles.Reserve(2);
		AEPathHeatmapRendererPrivate::AddSolidTriangle(
			SegmentTriangles,
			StartLeft,
			EndLeft,
			EndRight,
			DrawColor);
		AEPathHeatmapRendererPrivate::AddSolidTriangle(
			SegmentTriangles,
			StartLeft,
			EndRight,
			StartRight,
			DrawColor);
		FCanvasTriangleItem SegmentItem(SegmentTriangles, GWhiteTexture);
		SegmentItem.BlendMode = SE_BLEND_Opaque;
		Canvas->Canvas->DrawItem(SegmentItem);
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
}

/* Replace all material outputs while preserving the shared Render Target contents. */
int32 UAEPathHeatmapRendererComponent::BindMaterialOutputs()
{
	check(IsInGameThread());
	DisableMaterialOutput();
	ReleaseMeshMaterialBindings();
	BoundLandscape.Reset();
	BoundMaterialOutputCount = 0;

	if (BindLandscapeMaterialParameters())
	{
		++BoundMaterialOutputCount;
	}
	int32 RejectedMeshBindingCount = 0;
	BoundMaterialOutputCount += BindMeshMaterialParameters(RejectedMeshBindingCount);

	if (BoundMaterialOutputCount == 0)
	{
		UE_LOG(
			LogAdaptiveEnv,
			Warning,
			TEXT("M6 Render Target is ready but no material output was bound. World=%s Owner=%s MeshRejected=%d"),
			*GetNameSafe(GetWorld()),
			*GetNameSafe(GetOwner()),
			RejectedMeshBindingCount);
	}
	else if (RejectedMeshBindingCount > 0)
	{
		UE_LOG(
			LogAdaptiveEnv,
			Warning,
			TEXT("M6 material binding partially succeeded. World=%s Owner=%s Bound=%d MeshRejected=%d"),
			*GetNameSafe(GetWorld()),
			*GetNameSafe(GetOwner()),
			BoundMaterialOutputCount,
			RejectedMeshBindingCount);
	}
	return BoundMaterialOutputCount;
}

/* Push the documented M6 texture and spatial constants into the optional Landscape material. */
bool UAEPathHeatmapRendererComponent::BindLandscapeMaterialParameters()
{
	if (!IsValid(TargetLandscape) || !IsValid(PathVisualRenderTarget))
	{
		return false;
	}
	const UAdaptiveEnvSettings* Settings = GetDefault<UAdaptiveEnvSettings>();
	TargetLandscape->SetLandscapeMaterialTextureParameterValue(
		Settings->M6PathTextureParameterName,
		PathVisualRenderTarget);
	TargetLandscape->SetLandscapeMaterialVectorParameterValue(
		Settings->M6GridTransformParameterName,
		GridTransform);
	TargetLandscape->SetLandscapeMaterialScalarParameterValue(
		Settings->M6EnabledParameterName,
		1.0f);
	BoundLandscape = TargetLandscape;
	return true;
}

/* Create one owned MID for every valid unique Mesh material-slot binding. */
int32 UAEPathHeatmapRendererComponent::BindMeshMaterialParameters(
	int32& OutRejectedBindingCount)
{
	OutRejectedBindingCount = 0;
	int32 BoundCount = 0;
	for (const FAEPathMeshMaterialBinding& Binding : TargetMeshMaterials)
	{
		UMeshComponent* MeshComponent = Binding.MeshComponent.Get();
		const bool bDuplicate = RuntimeMeshMaterialBindings.ContainsByPredicate(
			[MeshComponent, &Binding](const FRuntimeMeshMaterialBinding& RuntimeBinding)
			{
				return RuntimeBinding.MeshComponent.Get() == MeshComponent
					&& RuntimeBinding.MaterialSlotIndex == Binding.MaterialSlotIndex;
			});
		if (!IsValid(MeshComponent)
			|| MeshComponent->GetWorld() != GetWorld()
			|| Binding.MaterialSlotIndex < 0
			|| Binding.MaterialSlotIndex >= MeshComponent->GetNumMaterials()
			|| bDuplicate)
		{
			++OutRejectedBindingCount;
			continue;
		}

		UMaterialInterface* OriginalMaterial = MeshComponent->GetMaterial(Binding.MaterialSlotIndex);
		if (!IsValid(OriginalMaterial)
			|| !SupportsRequiredMeshMaterialParameters(*OriginalMaterial))
		{
			++OutRejectedBindingCount;
			continue;
		}

		UMaterialInstanceDynamic* DynamicMaterial = MeshComponent->CreateDynamicMaterialInstance(
			Binding.MaterialSlotIndex,
			OriginalMaterial);
		if (!IsValid(DynamicMaterial))
		{
			++OutRejectedBindingCount;
			continue;
		}

		ApplyMeshMaterialParameters(*DynamicMaterial);
		FRuntimeMeshMaterialBinding& RuntimeBinding = RuntimeMeshMaterialBindings.AddDefaulted_GetRef();
		RuntimeBinding.MeshComponent = MeshComponent;
		RuntimeBinding.MaterialSlotIndex = Binding.MaterialSlotIndex;
		RuntimeBinding.OriginalMaterial = OriginalMaterial;
		RuntimeBinding.DynamicMaterial = DynamicMaterial;
		++BoundCount;
	}
	return BoundCount;
}

/* Apply the shared texture, World XY transform, and enable gate to one Mesh MID. */
void UAEPathHeatmapRendererComponent::ApplyMeshMaterialParameters(
	UMaterialInstanceDynamic& Material) const
{
	const UAdaptiveEnvSettings* Settings = GetDefault<UAdaptiveEnvSettings>();
	Material.SetTextureParameterValue(Settings->M6PathTextureParameterName, PathVisualRenderTarget);
	Material.SetVectorParameterValue(Settings->M6GridTransformParameterName, GridTransform);
	Material.SetScalarParameterValue(Settings->M6EnabledParameterName, 1.0f);
}

/* Require exact runtime parameter names before replacing a user Mesh material slot. */
bool UAEPathHeatmapRendererComponent::SupportsRequiredMeshMaterialParameters(
	const UMaterialInterface& Material) const
{
	const UAdaptiveEnvSettings* Settings = GetDefault<UAdaptiveEnvSettings>();
	TArray<FMaterialParameterInfo> Parameters;
	TArray<FGuid> ParameterIds;

	Material.GetAllTextureParameterInfo(Parameters, ParameterIds);
	const bool bHasTexture = AEPathHeatmapRendererPrivate::ContainsParameterName(
		Parameters,
		Settings->M6PathTextureParameterName);
	Parameters.Reset();
	ParameterIds.Reset();
	Material.GetAllVectorParameterInfo(Parameters, ParameterIds);
	const bool bHasTransform = AEPathHeatmapRendererPrivate::ContainsParameterName(
		Parameters,
		Settings->M6GridTransformParameterName);
	Parameters.Reset();
	ParameterIds.Reset();
	Material.GetAllScalarParameterInfo(Parameters, ParameterIds);
	const bool bHasEnabled = AEPathHeatmapRendererPrivate::ContainsParameterName(
		Parameters,
		Settings->M6EnabledParameterName);
	return bHasTexture && bHasTransform && bHasEnabled;
}

/* Disable material sampling without clearing authoritative M6 state. */
void UAEPathHeatmapRendererComponent::DisableMaterialOutput()
{
	if (ALandscapeProxy* Landscape = BoundLandscape.Get())
	{
		const UAdaptiveEnvSettings* Settings = GetDefault<UAdaptiveEnvSettings>();
		Landscape->SetLandscapeMaterialScalarParameterValue(
			Settings->M6EnabledParameterName,
			0.0f);
	}
	const FName EnabledParameterName = GetDefault<UAdaptiveEnvSettings>()->M6EnabledParameterName;
	for (const FRuntimeMeshMaterialBinding& Binding : RuntimeMeshMaterialBindings)
	{
		if (UMaterialInstanceDynamic* DynamicMaterial = Binding.DynamicMaterial.Get())
		{
			DynamicMaterial->SetScalarParameterValue(EnabledParameterName, 0.0f);
		}
	}
}

/* Restore only slots that still contain a material instance owned by this Renderer. */
void UAEPathHeatmapRendererComponent::ReleaseMeshMaterialBindings()
{
	for (const FRuntimeMeshMaterialBinding& Binding : RuntimeMeshMaterialBindings)
	{
		UMeshComponent* MeshComponent = Binding.MeshComponent.Get();
		UMaterialInstanceDynamic* DynamicMaterial = Binding.DynamicMaterial.Get();
		UMaterialInterface* OriginalMaterial = Binding.OriginalMaterial.Get();
		if (IsValid(MeshComponent)
			&& IsValid(DynamicMaterial)
			&& IsValid(OriginalMaterial)
			&& Binding.MaterialSlotIndex >= 0
			&& Binding.MaterialSlotIndex < MeshComponent->GetNumMaterials()
			&& MeshComponent->GetMaterial(Binding.MaterialSlotIndex) == DynamicMaterial)
		{
			MeshComponent->SetMaterial(Binding.MaterialSlotIndex, OriginalMaterial);
		}
	}
	RuntimeMeshMaterialBindings.Reset();
}

/* Restore command positions after stable-prefix removal. */
void UAEPathHeatmapRendererComponent::RebuildPendingCommandLookup()
{
	PendingCommandPositions.Reset();
	for (int32 Position = 0; Position < PendingCommands.Num(); ++Position)
	{
		PendingCommandPositions.Add(PendingCommands[Position].CellIndex, Position);
	}
}
