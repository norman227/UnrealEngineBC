// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "MessageLog.h"
#include "UObjectToken.h"
#include "MapErrors.h"

#define LOCTEXT_NAMESPACE "CameraComponent"

//////////////////////////////////////////////////////////////////////////
// UCameraComponent

UCameraComponent::UCameraComponent(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> EditorCameraMesh(TEXT("/Engine/EditorMeshes/MatineeCam_SM"));
		CameraMesh = EditorCameraMesh.Object;
	}

	ProxyMeshComponent = PCIP.CreateEditorOnlyDefaultSubobject<UStaticMeshComponent>(GetOuter(), TEXT("CameraProxy"), true);
	if (ProxyMeshComponent)
	{
		ProxyMeshComponent->AttachParent = this;
		ProxyMeshComponent->StaticMesh = CameraMesh;
		ProxyMeshComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		ProxyMeshComponent->bHiddenInGame = true;
		ProxyMeshComponent->CastShadow = false;
		ProxyMeshComponent->PostPhysicsComponentTick.bCanEverTick = false;
		ProxyMeshComponent->bCreatedByConstructionScript = bCreatedByConstructionScript;
	}

	DrawFrustum = PCIP.CreateEditorOnlyDefaultSubobject<UDrawFrustumComponent>(GetOuter(), TEXT("FrustumVisualizer"), true);
	if (DrawFrustum)
	{
		DrawFrustum->AttachParent = this;
		DrawFrustum->AlwaysLoadOnClient = false;
		DrawFrustum->AlwaysLoadOnServer = false;
		DrawFrustum->bCreatedByConstructionScript = bCreatedByConstructionScript;
	}
#endif

	FieldOfView = 90.0f;
	AspectRatio = 1.777778f;
	OrthoWidth = 512.0f;
	bConstrainAspectRatio = false;
	PostProcessBlendWeight = 1.0f;
	bUseControllerViewRotation = true;
	bAutoActivate = true;
}

void UCameraComponent::OnRegister()
{
#if WITH_EDITORONLY_DATA
	RefreshVisualRepresentation();
#endif

	Super::OnRegister();
}

#if WITH_EDITORONLY_DATA
void UCameraComponent::RefreshVisualRepresentation()
{
	if (DrawFrustum != NULL)
	{
		DrawFrustum->FrustumAngle = (ProjectionMode == ECameraProjectionMode::Perspective) ? FieldOfView : 0.0f;
		DrawFrustum->FrustumStartDist = 10.f;
		DrawFrustum->FrustumEndDist = 1000.f;
		DrawFrustum->FrustumAspectRatio = AspectRatio;
	}
}

void UCameraComponent::OverrideFrustumColor(FColor OverrideColor)
{
	if (DrawFrustum != NULL)
	{
		DrawFrustum->FrustumColor = OverrideColor;
	}
}

void UCameraComponent::RestoreFrustumColor()
{
	if (DrawFrustum != NULL)
	{
		//@TODO: 
		const FColor DefaultFrustumColor(255, 0, 255, 255);
		DrawFrustum->FrustumColor = DefaultFrustumColor;
		//ACameraActor* DefCam = Cam->GetClass()->GetDefaultObject<ACameraActor>();
		//Cam->DrawFrustum->FrustumColor = DefCam->DrawFrustum->FrustumColor;
	}
}

void UCameraComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	RefreshVisualRepresentation();
}
#endif

void UCameraComponent::GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView)
{
	if (bUseControllerViewRotation)
	{
		if (APawn* OwningPawn = Cast<APawn>(GetOwner()))
		{
			const FRotator PawnViewRotation = OwningPawn->GetViewRotation();
			if (PawnViewRotation != GetComponentRotation())
			{
				SetWorldRotation(PawnViewRotation);
			}
		}
	}

	DesiredView.Location = GetComponentLocation();
	DesiredView.Rotation = GetComponentRotation();

	DesiredView.FOV = FieldOfView;
	DesiredView.AspectRatio = AspectRatio;
	DesiredView.bConstrainAspectRatio = bConstrainAspectRatio;
	DesiredView.ProjectionMode = ProjectionMode;
	DesiredView.OrthoWidth = OrthoWidth;

	// See if the CameraActor wants to override the PostProcess settings used.
	DesiredView.PostProcessBlendWeight = PostProcessBlendWeight;
	if (PostProcessBlendWeight > 0.0f)
	{
		DesiredView.PostProcessSettings = PostProcessSettings;
	}
}

#if WITH_EDITOR
void UCameraComponent::CheckForErrors()
{
	Super::CheckForErrors();

	if (AspectRatio <= 0.0f)
	{
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(LOCTEXT( "MapCheck_Message_CameraAspectRatioIsZero", "Camera has AspectRatio=0 - please set this to something non-zero" ) ))
			->AddToken(FMapErrorToken::Create(FMapErrors::CameraAspectRatioIsZero));
	}
}
#endif
#undef LOCTEXT_NAMESPACE
