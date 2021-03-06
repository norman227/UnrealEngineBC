// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "WidgetComponent.h"
#include "HittestGrid.h"
#if !UE_SERVER
	#include "ISlateRHIRendererModule.h"
	#include "ISlate3DRenderer.h"
#endif // !UE_SERVER
#include "DynamicMeshBuilder.h"


class SVirtualWindow : public SWindow
{
	SLATE_BEGIN_ARGS( SVirtualWindow )
		: _Size( FVector2D(100,100) )
	{}

		SLATE_ARGUMENT( FVector2D, Size )
	SLATE_END_ARGS()

public:
	void Construct( const FArguments& InArgs )
	{
		bIsPopupWindow = true;
		SetCachedSize( InArgs._Size );
		SetNativeWindow( MakeShareable( new FGenericWindow() ) );
		SetContent( SNullWidget::NullWidget );
	}
};


/** Represents a billboard sprite to the scene manager. */
class FWidget3DSceneProxy : public FPrimitiveSceneProxy
{
public:
	/** Initialization constructor. */
	FWidget3DSceneProxy( UWidgetComponent* InComponent, ISlate3DRenderer& InRenderer )
		: FPrimitiveSceneProxy( InComponent )
		, Renderer( InRenderer )
		, RenderTarget( InComponent->GetRenderTarget() )
		, MaterialInstance( InComponent->GetMaterialInstance() )
		, BodySetup( InComponent->GetBodySetup() )
		, bIsOpaque( InComponent->IsOpaque() )
	{
		bWillEverBeLit = false;
	}

	~FWidget3DSceneProxy()
	{
	}

	// FPrimitiveSceneProxy interface.
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		const FMatrix& LocalToWorld = GetLocalToWorld();
	
		FDynamicMeshBuilder MeshBuilder;

		if( RenderTarget )
		{
			FTextureResource* TextureResource = RenderTarget->Resource;
			if(TextureResource)
			{
				float U = 0.0f;
				float V = 0.0f;
				float UL = RenderTarget->SizeX;
				float VL = RenderTarget->SizeY;

				int32 VertexIndices[4];

				VertexIndices[0] = MeshBuilder.AddVertex(FVector(U, V, 0), FVector2D(0, 0), FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), FColor::White);
				VertexIndices[1] = MeshBuilder.AddVertex(FVector(U, VL, 0), FVector2D(0, 1), FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), FColor::White);
				VertexIndices[2] = MeshBuilder.AddVertex(FVector(UL, VL, 0), FVector2D(1, 1), FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), FColor::White);
				VertexIndices[3] = MeshBuilder.AddVertex(FVector(UL, V, 0), FVector2D(1, 0), FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), FColor::White);

				MeshBuilder.AddTriangle(VertexIndices[0], VertexIndices[1], VertexIndices[2]);
				MeshBuilder.AddTriangle(VertexIndices[0], VertexIndices[2], VertexIndices[3]);
			}


			for(int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				if(VisibilityMap & (1 << ViewIndex))
				{
					FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

					MeshBuilder.Draw(PDI, LocalToWorld, MaterialInstance->GetRenderProxy(IsSelected()), SDPG_World);
				}
			}
		}

	}
	/** 
	 * Draw the scene proxy as a dynamic element
	 *
	 * @param	PDI - draw interface to render to
	 * @param	View - current view
	 */
	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI, const FSceneView* View) override
	{
		if( RenderTarget )
		{
			FTextureResource* TextureResource = RenderTarget->Resource;
			if(TextureResource)
			{
				float U = 0.0f;
				float V = 0.0f;
				float UL = RenderTarget->SizeX;
				float VL = RenderTarget->SizeY;

				FDynamicMeshBuilder MeshBuilder;

				int32 VertexIndices[4];

				VertexIndices[0] = MeshBuilder.AddVertex(FVector(U, V, 0), FVector2D(0, 0), FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), FColor::White);
				VertexIndices[1] = MeshBuilder.AddVertex(FVector(U, VL, 0), FVector2D(0, 1), FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), FColor::White);
				VertexIndices[2] = MeshBuilder.AddVertex(FVector(UL, VL, 0), FVector2D(1, 1), FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), FColor::White);
				VertexIndices[3] = MeshBuilder.AddVertex(FVector(UL, V, 0), FVector2D(1, 0), FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), FColor::White);

				MeshBuilder.AddTriangle(VertexIndices[0], VertexIndices[1], VertexIndices[2]);
				MeshBuilder.AddTriangle(VertexIndices[0], VertexIndices[2], VertexIndices[3]);

				MeshBuilder.Draw(PDI, GetLocalToWorld(), MaterialInstance->GetRenderProxy(IsSelected()), SDPG_World);
			}
		}
		
		// Visualize the collision geometry
		/*FColor CollisionColor = FColor(157,149,223,255);
		FTransform GeomTransform(GetLocalToWorld());
		BodySetup->AggGeom.DrawAggGeom(PDI, GeomTransform, GetSelectionColor(CollisionColor, false, IsHovered()), nullptr, false, false, true );*/
		
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		RenderBounds(PDI, View->Family->EngineShowFlags, GetBounds(), IsSelected());
#endif
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) override
	{
		bool bVisible = true;// View->Family->EngineShowFlags.BillboardSprites;

		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View) && bVisible;
		Result.bOpaqueRelevance = bIsOpaque;
		Result.bNormalTranslucencyRelevance = !bIsOpaque;
		Result.bSeparateTranslucencyRelevance = !bIsOpaque;
		Result.bDynamicRelevance = true;
		Result.bShadowRelevance = IsShadowCast(View);;
		Result.bEditorPrimitiveRelevance = false;
		return Result;
	}

	virtual void GetLightRelevance(const FLightSceneProxy* LightSceneProxy, bool& bDynamic, bool& bRelevant, bool& bLightMapped, bool& bShadowMapped) const override
	{
		bDynamic = false;
		bRelevant = false;
		bLightMapped = false;
		bShadowMapped = false;
	}

	virtual void OnTransformChanged() override
	{
		Origin = GetLocalToWorld().GetOrigin();
	}

	virtual uint32 GetMemoryFootprint(void) const override { return(sizeof(*this) + GetAllocatedSize()); }

	uint32 GetAllocatedSize(void) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

private:
	FVector Origin;
	ISlate3DRenderer& Renderer;
	UTextureRenderTarget2D* RenderTarget;
	UMaterialInstanceDynamic* MaterialInstance;
	UBodySetup* BodySetup;
	bool bIsOpaque;
};

/**
* The hit tester used by all Widget Component objects.
*/
class FWidget3DHitTester : public ICustomHitTestPath
{
public:
	FWidget3DHitTester( UWorld* InWorld )
		: World( InWorld )
	{}

	// ICustomHitTestPath implementation
	virtual TArray<FWidgetAndPointer> GetBubblePathAndVirtualCursors(const FGeometry& InGeometry, FVector2D DesktopSpaceCoordinate, bool bIgnoreEnabledStatus) const override
	{
		if( World.IsValid() && ensure( World->IsGameWorld() ) )
		{
			ULocalPlayer* const TargetPlayer = GEngine->GetLocalPlayerFromControllerId(World.Get(), 0);
			APlayerController* PlayerController = TargetPlayer->PlayerController;

			if (TargetPlayer && PlayerController)
			{
				FHitResult HitResult;
				if (PlayerController->GetHitResultAtScreenPosition(InGeometry.AbsoluteToLocal(DesktopSpaceCoordinate), ECC_Visibility, true, HitResult))
				{
					UWidgetComponent* WidgetComponent = Cast<UWidgetComponent>(HitResult.Component.Get());
					if ( WidgetComponent )
					{
						// Make sure the player is interacting with the front of the widget
						// For widget components, the "front" faces the Z (or Up vector) direction
						if ( FVector::DotProduct( WidgetComponent->GetUpVector(), HitResult.ImpactPoint - HitResult.TraceStart ) < 0.f )
						{
							// Make sure the player is close enough to the widget to interact with it
							if ( FVector::DistSquared( HitResult.TraceStart, HitResult.ImpactPoint ) <= FMath::Square( WidgetComponent->GetMaxInteractionDistance() ) )
							{
								return WidgetComponent->GetHitWidgetPath( HitResult, bIgnoreEnabledStatus );
							}
						}
					}
				}
			}
		}
		return TArray<FWidgetAndPointer>();
	}

	virtual void ArrangeChildren( FArrangedChildren& ArrangedChildren ) const override
	{
		for( TWeakObjectPtr<UWidgetComponent> Component : RegisteredComponents )
		{
			UWidgetComponent* WidgetComponent = Component.Get();
			// Check if visible;
			if ( WidgetComponent && WidgetComponent->GetSlateWidget().IsValid() )
			{
				FGeometry WidgetGeom;

				ArrangedChildren.AddWidget( FArrangedWidget( WidgetComponent->GetSlateWidget().ToSharedRef(), WidgetGeom.MakeChild( WidgetComponent->GetDrawSize(), FSlateLayoutTransform() ) ) );
			}
		}
	}

	virtual TSharedPtr<struct FVirtualPointerPosition> TranslateMouseCoordinateFor3DChild( const TSharedRef<SWidget>& ChildWidget, const FGeometry& ViewportGeometry, const FVector2D& ScreenSpaceMouseCoordinate, const FVector2D& LastScreenSpaceMouseCoordinate ) const override
	{
		if(World.IsValid() && ensure(World->IsGameWorld()))
		{
			ULocalPlayer* const TargetPlayer = GEngine->GetLocalPlayerFromControllerId(World.Get(), 0);
			if(TargetPlayer && TargetPlayer->PlayerController)
			{
				// Check for a hit against any widget components in the world
				for(TWeakObjectPtr<UWidgetComponent> Component : RegisteredComponents)
				{
					UWidgetComponent* WidgetComponent = Component.Get();
					// Check if visible;
					if ( WidgetComponent && WidgetComponent->GetSlateWidget() == ChildWidget )
					{
						FVector2D LocalMouseCoordinate = ViewportGeometry.AbsoluteToLocal(ScreenSpaceMouseCoordinate);
						FVector2D LocalLastMouseCoordinate = ViewportGeometry.AbsoluteToLocal( LastScreenSpaceMouseCoordinate );


						FHitResult HitResult;
						if(TargetPlayer->PlayerController->GetHitResultAtScreenPosition(LocalMouseCoordinate, ECC_Visibility, true, HitResult))
						{
							if ( WidgetComponent == HitResult.Component.Get() )
							{
								TSharedPtr<FVirtualPointerPosition> VirtualCursorPos = MakeShareable(new FVirtualPointerPosition);

								FVector2D LocalHitLocation = FVector2D( WidgetComponent->ComponentToWorld.InverseTransformPosition( HitResult.Location ) );

								VirtualCursorPos->CurrentCursorPosition = LocalHitLocation;
								VirtualCursorPos->LastCursorPosition = LocalHitLocation;

								return VirtualCursorPos;
							}
						}
					}
				}
			}
		}

		return nullptr;
	}
	// End ICustomHitTestPath

	void RegisterWidgetComponent( UWidgetComponent* InComponent )
	{
		RegisteredComponents.AddUnique( InComponent );
	}

	void UnregisterWidgetComponent( UWidgetComponent* InComponent )
	{
		RegisteredComponents.RemoveSingleSwap( InComponent );
	}

	uint32 GetNumRegisteredComponents() const { return RegisteredComponents.Num(); }
	
	UWorld* GetWorld() const { return World.Get(); }
private:
	TArray< TWeakObjectPtr<UWidgetComponent> > RegisteredComponents;
	TWeakObjectPtr<UWorld> World;
};

UWidgetComponent::UWidgetComponent( const class FObjectInitializer& PCIP )
	: Super( PCIP )
	, DrawSize( FIntPoint( 500, 500 ) )
	, MaxInteractionDistance( 1000.f )
	, BackgroundColor( FLinearColor::Transparent )
	, bIsOpaque( false )
	, bIsTwoSided( false )
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
	
	SetRelativeRotation( FRotator( 0.f, 0.f, 90.f ) );
	
	BodyInstance.SetCollisionProfileName(FName(TEXT("UI")));

	// Translucent material instances
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TranslucentMaterial_Finder( TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent") );
	TranslucentMaterial = TranslucentMaterial_Finder.Object;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TranslucentMaterial_OneSided_Finder( TEXT( "/Engine/EngineMaterials/Widget3DPassThrough_Translucent_OneSided" ) );
	TranslucentMaterial_OneSided = TranslucentMaterial_OneSided_Finder.Object;

	// Opaque material instances
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OpaqueMaterial_Finder( TEXT( "/Engine/EngineMaterials/Widget3DPassThrough_Opaque" ) );
	OpaqueMaterial = OpaqueMaterial_Finder.Object;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OpaqueMaterial_OneSided_Finder( TEXT( "/Engine/EngineMaterials/Widget3DPassThrough_Opaque_OneSided" ) );
	OpaqueMaterial_OneSided = OpaqueMaterial_OneSided_Finder.Object;

	LastLocalHitLocation = FVector2D::ZeroVector;
	//bGenerateOverlapEvents = false;
	bUseEditorCompositing = false;
}

FPrimitiveSceneProxy* UWidgetComponent::CreateSceneProxy()
{
	return new FWidget3DSceneProxy(this,*Renderer);
}

FBoxSphereBounds UWidgetComponent::CalcBounds(const FTransform & LocalToWorld) const
{
	return FBoxSphereBounds( FVector( DrawSize.X/2.0f, DrawSize.Y/2.0f, .5f), FVector(DrawSize.X/2,DrawSize.Y/2,1.0f), DrawSize.Size()/2 ).TransformBy( LocalToWorld );
}

UBodySetup* UWidgetComponent::GetBodySetup() 
{
	UpdateBodySetup();
	return BodySetup;
}

FCollisionShape UWidgetComponent::GetCollisionShape(float Inflation) const
{
	FVector	Extent = ( FVector( DrawSize.Y, DrawSize.Y, 1.0f ) * ComponentToWorld.GetScale3D() ) + Inflation;
	if( Inflation < 0.0f )
	{
		// Don't shrink below zero size.
		Extent = Extent.ComponentMax(FVector::ZeroVector);
	}

	return FCollisionShape::MakeBox(Extent);
}

void UWidgetComponent::OnRegister()
{
	Super::OnRegister();

	if( GetWorld()->IsGameWorld() )
	{
		TSharedPtr<SViewport> GameViewportWidget = GEngine->GetGameViewportWidget();

		if( GameViewportWidget.IsValid() )
		{
			TSharedPtr<ICustomHitTestPath> CustomHitTestPath = GameViewportWidget->GetCustomHitTestPath();
			if( !CustomHitTestPath.IsValid() )
			{
				CustomHitTestPath = MakeShareable(new FWidget3DHitTester(GetWorld()));
				GameViewportWidget->SetCustomHitTestPath( CustomHitTestPath );
			}

			TSharedPtr<FWidget3DHitTester> WidgetHitTester = StaticCastSharedPtr<FWidget3DHitTester>(CustomHitTestPath);
			if( WidgetHitTester->GetWorld() == GetWorld() )
			{
				WidgetHitTester->RegisterWidgetComponent( this );
			}
		}
	}

#if !UE_SERVER

	if( !MaterialInstance )
	{
		UMaterialInterface* Parent = nullptr;
		if (bIsOpaque)
		{
			Parent = bIsTwoSided ? OpaqueMaterial : OpaqueMaterial_OneSided;
		}
		else
		{
			Parent = bIsTwoSided ? TranslucentMaterial : TranslucentMaterial_OneSided;
		}

		MaterialInstance = UMaterialInstanceDynamic::Create(Parent, this);
	}

	UpdateWidget();

	if( !Renderer.IsValid() )
	{
		Renderer = FModuleManager::Get().GetModuleChecked<ISlateRHIRendererModule>("SlateRHIRenderer").CreateSlate3DRenderer();
	}

#endif // !UE_SERVER
}

void UWidgetComponent::OnUnregister()
{
	if( GetWorld()->IsGameWorld() )
	{
		TSharedPtr<SViewport> GameViewportWidget = GEngine->GetGameViewportWidget();
		if( GameViewportWidget.IsValid() )
		{
			TSharedPtr<ICustomHitTestPath> CustomHitTestPath = GameViewportWidget->GetCustomHitTestPath();
			if( CustomHitTestPath.IsValid() )
			{
				TSharedPtr<FWidget3DHitTester> WidgetHitTestPath = StaticCastSharedPtr<FWidget3DHitTester>(CustomHitTestPath);

				WidgetHitTestPath->UnregisterWidgetComponent( this );

				if ( WidgetHitTestPath->GetNumRegisteredComponents() == 0 )
				{
					GameViewportWidget->SetCustomHitTestPath( nullptr );
				}
			}
		}
	}

	if( Widget )
	{
		Widget->MarkPendingKill();
		Widget = NULL;
	}

	SlateWidget.Reset();
	Super::OnUnregister();
}

void UWidgetComponent::DestroyComponent()
{
	Super::DestroyComponent();
	Renderer.Reset();
}

void UWidgetComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
#if !UE_SERVER

	const float RenderTimeThreshold = .5f;
	if( IsVisible() && GetWorld()->TimeSince(LastRenderTime) <= RenderTimeThreshold ) // Don't bother ticking if it hasn't been rendered recently
	{
		SlateWidget->SlatePrepass();

		FGeometry WindowGeometry = FGeometry::MakeRoot( DrawSize, FSlateLayoutTransform() );

		SlateWidget->TickWidgetsRecursively(WindowGeometry, FApp::GetCurrentTime(), DeltaTime);

		// Ticking can cause geometry changes.  Recompute
		SlateWidget->SlatePrepass();

		// Get the free buffer & add our virtual window
		FSlateDrawBuffer& DrawBuffer = Renderer->GetDrawBuffer();
		FSlateWindowElementList& WindowElementList = DrawBuffer.AddWindowElementList(SlateWidget.ToSharedRef());

		// Prepare the test grid 
		HitTestGrid->BeginFrame(WindowGeometry.GetClippingRect());

		// Paint the window
		int32 MaxLayerId = 0;
		{
			MaxLayerId = SlateWidget->Paint(
				FPaintArgs(SlateWidget.ToSharedRef(), *HitTestGrid, FVector2D::ZeroVector),
				WindowGeometry, WindowGeometry.GetClippingRect(),
				WindowElementList,
				0,
				FWidgetStyle(),
				SlateWidget->IsEnabled());
		}

		Renderer->DrawWindow_GameThread(DrawBuffer);

		UpdateRenderTarget();

		// Enqueue a command to unlock the draw buffer after all windows have been drawn
		ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(UWidgetComponentRenderToTexture,
			FSlateDrawBuffer&, InDrawBuffer, DrawBuffer,
			UTextureRenderTarget2D*, InRenderTarget, RenderTarget,
			ISlate3DRenderer*, InRenderer, Renderer.Get(),
			{
				InRenderer->DrawWindowToTarget_RenderThread( RHICmdList, InRenderTarget, InDrawBuffer );	
			});

	}

#endif // !UE_SERVER
}

class FWidgetComponentInstanceData : public FComponentInstanceDataBase
{
public:
	FWidgetComponentInstanceData( const UWidgetComponent* SourceComponent )
		: FComponentInstanceDataBase(SourceComponent)
		, WidgetClass ( SourceComponent->GetWidgetClass() )
		, RenderTarget( SourceComponent->GetRenderTarget() )
	{}

	virtual bool MatchesComponent( const UActorComponent* Component ) const override
	{
		return (CastChecked<UWidgetComponent>(Component)->GetWidgetClass() == WidgetClass && FComponentInstanceDataBase::MatchesComponent(Component));
	}

public:
	TSubclassOf<UUserWidget> WidgetClass;
	UTextureRenderTarget2D* RenderTarget;
};

FName UWidgetComponent::GetComponentInstanceDataType() const
{
	static const FName InstanceDataName(TEXT("WidgetInstanceData"));
	return InstanceDataName;
}

FComponentInstanceDataBase* UWidgetComponent::GetComponentInstanceData() const
{
	return new FWidgetComponentInstanceData( this );
}

void UWidgetComponent::ApplyComponentInstanceData(FComponentInstanceDataBase* ComponentInstanceData)
{
	check(ComponentInstanceData);

	// Note: ApplyComponentInstanceData is called while the component is registered so the rendering thread is already using this component
	// That means all component state that is modified here must be mirrored on the scene proxy, which will be recreated to receive the changes later due to MarkRenderStateDirty.
	FWidgetComponentInstanceData* WidgetInstanceData  = static_cast<FWidgetComponentInstanceData*>(ComponentInstanceData);

	RenderTarget = WidgetInstanceData->RenderTarget;
	MaterialInstance->SetTextureParameterValue("SlateUI", RenderTarget);

	MarkRenderStateDirty();
}

#if WITH_EDITORONLY_DATA
void UWidgetComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* Property = PropertyChangedEvent.MemberProperty;

	if( Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive )
	{
		static FName DrawSizeName("DrawSize");
		static FName WidgetClassName("WidgetClass");
		static FName IsOpaqueName("bIsOpaque");
		static FName IsTwoSidedName("bIsTwoSided");
		static FName BackgroundColorName("BackgroundColor");

		auto PropertyName = Property->GetFName();

		if( PropertyName == DrawSizeName || PropertyName == WidgetClassName )
		{
			UpdateWidget();

			if( PropertyName == DrawSizeName )
			{
				UpdateBodySetup(true);
			}

			MarkRenderStateDirty();
		}
		else if ( PropertyName == IsOpaqueName  || PropertyName == BackgroundColorName || PropertyName == IsTwoSidedName)
		{
			MarkRenderStateDirty();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UWidgetComponent::UpdateWidget()
{
	// Don't do any work if Slate is not initialized
	if(FSlateApplication::IsInitialized())
	{		
		if(WidgetClass && !Widget && GetWorld() )
		{
			Widget = CreateWidget<UUserWidget>( GetWorld(), WidgetClass );
		}
		else if( !WidgetClass && Widget )
		{
			Widget = nullptr;
		}

		if(!SlateWidget.IsValid())
		{
			SlateWidget = SNew(SVirtualWindow).Size(DrawSize);

		}
	
		if( !HitTestGrid.IsValid() )
		{
			HitTestGrid = MakeShareable( new FHittestGrid );
		}

		{
			SlateWidget->Resize( DrawSize );

			if( Widget )
			{
				SlateWidget->SetContent( Widget->TakeWidget() );
			}
			else
			{
				SlateWidget->SetContent( SNullWidget::NullWidget );
			}

		}

		if( Widget && !GetWorld()->IsGameWorld() )
		{
			// Prevent native ticking of editor component previews
			Widget->SetIsDesignTime( true );
		}
	}
}

void UWidgetComponent::UpdateRenderTarget()
{
	bool bClearColorChanged = false;

	if(!RenderTarget && DrawSize != FIntPoint::ZeroValue)
	{
		RenderTarget = ConstructObject<UTextureRenderTarget2D>(UTextureRenderTarget2D::StaticClass(), this);
		RenderTarget->InitCustomFormat(DrawSize.X, DrawSize.Y, PF_B8G8R8A8, false);
		RenderTarget->ClearColor = BackgroundColor;
		if (bIsOpaque)
		{
			RenderTarget->ClearColor.A = 1.0f;
		}
		bClearColorChanged = true;
		
		MaterialInstance->SetTextureParameterValue( "SlateUI", RenderTarget );
		MarkRenderStateDirty();
	}
	else if ( DrawSize != FIntPoint::ZeroValue )
	{
		// Update the format
		if ( RenderTarget->SizeX != DrawSize.X || RenderTarget->SizeY != DrawSize.Y )
		{
			RenderTarget->InitCustomFormat( DrawSize.X, DrawSize.Y, PF_B8G8R8A8, false );
		}

		// Update the clear color
		if (!bIsOpaque && RenderTarget->ClearColor != BackgroundColor)
		{
			RenderTarget->ClearColor = BackgroundColor;
			bClearColorChanged = true;
		}
		else if ( bIsOpaque )
		{
			// If opaque, make sure the alpha channel is set to 1.0
			FLinearColor ClearColor( BackgroundColor.R, BackgroundColor.G, BackgroundColor.B, 1.0f );
			if ( RenderTarget->ClearColor != ClearColor )
			{
				RenderTarget->ClearColor = ClearColor;
				bClearColorChanged = true;
			}
		}
		
		MarkRenderStateDirty();
	}

	// If the clear color of the render target changed, update the BackColor of the material to match
	if ( bClearColorChanged )
	{
		MaterialInstance->SetVectorParameterValue( "BackColor", RenderTarget->ClearColor );
	}
}

void UWidgetComponent::UpdateBodySetup( bool bDrawSizeChanged )
{
	if( !BodySetup || bDrawSizeChanged )
	{
		BodySetup = ConstructObject<UBodySetup>( UBodySetup::StaticClass(), this );
		BodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
		BodySetup->AggGeom.BoxElems.Add(FKBoxElem());

		FKBoxElem* BoxElem = BodySetup->AggGeom.BoxElems.GetData();

		BoxElem->SetTransform(FTransform::Identity);
		BoxElem->Center = FVector(DrawSize.X/2.0f, DrawSize.Y/2.0f, .5f);
		BoxElem->X = DrawSize.X;
		BoxElem->Y = DrawSize.Y;
		BoxElem->Z = 1.0f;
	}	
}

UUserWidget* UWidgetComponent::GetUserWidgetObject() const
{
	return Widget;
}

TArray<FWidgetAndPointer> UWidgetComponent::GetHitWidgetPath( const FHitResult& HitResult, bool bIgnoreEnabledStatus  )
{
	FVector2D LocalHitLocation = FVector2D( ComponentToWorld.InverseTransformPosition( HitResult.Location ) );

	TSharedRef<FVirtualPointerPosition> VirtualMouseCoordinate = MakeShareable( new FVirtualPointerPosition );

	VirtualMouseCoordinate->CurrentCursorPosition = LocalHitLocation;
	VirtualMouseCoordinate->LastCursorPosition = LastLocalHitLocation;

	// Cache the location of the hit
	LastLocalHitLocation = LocalHitLocation;

	TArray<FWidgetAndPointer> ArrangedWidgets = HitTestGrid->GetBubblePath( LocalHitLocation, bIgnoreEnabledStatus );

	for( FWidgetAndPointer& Widget : ArrangedWidgets )
	{
		Widget.PointerPosition = VirtualMouseCoordinate;
	}

	return ArrangedWidgets;
}

TSharedPtr<SWidget> UWidgetComponent::GetSlateWidget() const
{
	return SlateWidget;
}
