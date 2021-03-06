// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalCommands.cpp: Metal RHI commands implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"

static const bool GUsesInvertedZ = true;

static MTLPrimitiveType TranslatePrimitiveType(uint32 PrimitiveType)
{
	switch (PrimitiveType)
	{
		case PT_TriangleList:	return MTLPrimitiveTypeTriangle;
		case PT_TriangleStrip:	return MTLPrimitiveTypeTriangleStrip;
		case PT_LineList:		return MTLPrimitiveTypeLine;
		default:				UE_LOG(LogMetal, Fatal, TEXT("Unsupported primitive type %d"), (int32)PrimitiveType); return MTLPrimitiveTypeTriangle;
	}
}

void FMetalDynamicRHI::RHISetStreamSource(uint32 StreamIndex,FVertexBufferRHIParamRef VertexBufferRHI,uint32 Stride,uint32 Offset)
{
	DYNAMIC_CAST_METALRESOURCE(VertexBuffer,VertexBuffer);

	if (VertexBuffer != NULL)
	{
		[FMetalManager::GetContext() setVertexBuffer:VertexBuffer->Buffer offset:VertexBuffer->Offset + Offset atIndex:UNREAL_TO_METAL_BUFFER_INDEX(StreamIndex)];
	}
}

void FMetalDynamicRHI::RHISetStreamOutTargets(uint32 NumTargets, const FVertexBufferRHIParamRef* VertexBuffers, const uint32* Offsets)
{
	NOT_SUPPORTED("RHISetStreamOutTargets");

}

void FMetalDynamicRHI::RHISetRasterizerState(FRasterizerStateRHIParamRef NewStateRHI)
{
	DYNAMIC_CAST_METALRESOURCE(RasterizerState,NewState);

	NewState->Set();
}

void FMetalDynamicRHI::RHISetComputeShader(FComputeShaderRHIParamRef ComputeShaderRHI)
{
	NOT_SUPPORTED("RHISetComputeShader");
}

void FMetalDynamicRHI::RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) 
{ 
	NOT_SUPPORTED("RHIDispatchComputeShader");
}

void FMetalDynamicRHI::RHIDispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBufferRHI, uint32 ArgumentOffset) 
{ 
	NOT_SUPPORTED("RHIDispatchIndirectComputeShader");
}

void FMetalDynamicRHI::RHISetViewport(uint32 MinX,uint32 MinY,float MinZ,uint32 MaxX,uint32 MaxY,float MaxZ)
{
	MTLViewport Viewport;
	Viewport.originX = MinX;
	Viewport.originY = MinY;
	Viewport.width = MaxX - MinX;
	Viewport.height = MaxY - MinY;
	Viewport.znear = MinZ;
	Viewport.zfar = MaxZ;
	
	[FMetalManager::GetContext() setViewport:Viewport];
}

void FMetalDynamicRHI::RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data) 
{ 
	NOT_SUPPORTED("RHISetMultipleViewports");
}

void FMetalDynamicRHI::RHISetScissorRect(bool bEnable,uint32 MinX,uint32 MinY,uint32 MaxX,uint32 MaxY)
{
	MTLScissorRect Scissor;
	Scissor.x = MinX;
	Scissor.y = MinY;
	Scissor.width = MaxX - MinX;
	Scissor.height = MaxY - MinY;

	// metal doesn't support 0 sized scissor rect
	if (Scissor.width == 0 || Scissor.height == 0)
	{
		return;
	}
	[FMetalManager::GetContext() setScissorRect:Scissor];
}

void FMetalDynamicRHI::RHISetBoundShaderState( FBoundShaderStateRHIParamRef BoundShaderStateRHI)
{
	DYNAMIC_CAST_METALRESOURCE(BoundShaderState,BoundShaderState);

	FMetalManager::Get()->SetBoundShaderState(BoundShaderState);
	BoundShaderStateHistory.Add(BoundShaderState);
}


void FMetalDynamicRHI::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAVRHI)
{
	NOT_SUPPORTED("RHISetUAVParameter");
}

void FMetalDynamicRHI::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 UAVIndex,FUnorderedAccessViewRHIParamRef UAVRHI, uint32 InitialCount)
{
	NOT_SUPPORTED("RHISetUAVParameter");
}


void FMetalDynamicRHI::RHISetShaderTexture(FVertexShaderRHIParamRef VertexShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	FMetalSurface& Surface = GetMetalSurfaceFromRHITexture(NewTextureRHI);
	[FMetalManager::GetContext() setVertexTexture:Surface.Texture atIndex:TextureIndex];
}

void FMetalDynamicRHI::RHISetShaderTexture(FHullShaderRHIParamRef HullShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	NOT_SUPPORTED("RHISetShaderTexture-Hull");
}

void FMetalDynamicRHI::RHISetShaderTexture(FDomainShaderRHIParamRef DomainShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	NOT_SUPPORTED("RHISetShaderTexture-Domain");
}

void FMetalDynamicRHI::RHISetShaderTexture(FGeometryShaderRHIParamRef GeometryShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	NOT_SUPPORTED("RHISetShaderTexture-Geometry");

}

void FMetalDynamicRHI::RHISetShaderTexture(FPixelShaderRHIParamRef PixelShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	FMetalSurface& Surface = GetMetalSurfaceFromRHITexture(NewTextureRHI);
	[FMetalManager::GetContext() setFragmentTexture:Surface.Texture atIndex:TextureIndex];
}

void FMetalDynamicRHI::RHISetShaderTexture(FComputeShaderRHIParamRef ComputeShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	NOT_SUPPORTED("RHISetShaderTexture-Compute");
}


void FMetalDynamicRHI::RHISetShaderResourceViewParameter(FVertexShaderRHIParamRef VertexShaderRHI, uint32 TextureIndex, FShaderResourceViewRHIParamRef SRVRHI)
{
	DYNAMIC_CAST_METALRESOURCE(ShaderResourceView, SRV);

	FRHITexture* Texture = SRV->SourceTexture.GetReference();
	if (Texture)
	{
		FMetalSurface& Surface = GetMetalSurfaceFromRHITexture(Texture);
		[FMetalManager::GetContext() setVertexTexture:Surface.Texture atIndex:TextureIndex];
	}
	else
	{
		FMetalVertexBuffer* VB = SRV->SourceVertexBuffer.GetReference();
		if (VB)
		{
			[FMetalManager::GetContext() setVertexBuffer:VB->Buffer offset:VB->Offset atIndex:TextureIndex];
		}
	}
}

void FMetalDynamicRHI::RHISetShaderResourceViewParameter(FHullShaderRHIParamRef HullShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	NOT_SUPPORTED("RHISetShaderResourceViewParameter");
}

void FMetalDynamicRHI::RHISetShaderResourceViewParameter(FDomainShaderRHIParamRef DomainShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	NOT_SUPPORTED("RHISetShaderResourceViewParameter");
}

void FMetalDynamicRHI::RHISetShaderResourceViewParameter(FGeometryShaderRHIParamRef GeometryShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	NOT_SUPPORTED("RHISetShaderResourceViewParameter");
}

void FMetalDynamicRHI::RHISetShaderResourceViewParameter(FPixelShaderRHIParamRef PixelShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	DYNAMIC_CAST_METALRESOURCE(ShaderResourceView, SRV);

	FRHITexture* Texture = SRV->SourceTexture.GetReference();
	if (Texture)
	{
		FMetalSurface& Surface = GetMetalSurfaceFromRHITexture(Texture);
		[FMetalManager::GetContext() setFragmentTexture:Surface.Texture atIndex:TextureIndex];
	}
	else
	{
		FMetalVertexBuffer* VB = SRV->SourceVertexBuffer.GetReference();
		if (VB)
		{
			[FMetalManager::GetContext() setFragmentBuffer:VB->Buffer offset:VB->Offset atIndex:TextureIndex];
		}
	}
}

void FMetalDynamicRHI::RHISetShaderResourceViewParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	NOT_SUPPORTED("RHISetShaderResourceViewParameter");
}


void FMetalDynamicRHI::RHISetShaderSampler(FVertexShaderRHIParamRef VertexShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	DYNAMIC_CAST_METALRESOURCE(SamplerState,NewState);

	[FMetalManager::GetContext() setVertexSamplerState:NewState->State atIndex:SamplerIndex];
}

void FMetalDynamicRHI::RHISetShaderSampler(FHullShaderRHIParamRef HullShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	DYNAMIC_CAST_METALRESOURCE(SamplerState,NewState);

	NOT_SUPPORTED("RHISetSamplerState-Hull");
}

void FMetalDynamicRHI::RHISetShaderSampler(FDomainShaderRHIParamRef DomainShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	DYNAMIC_CAST_METALRESOURCE(SamplerState,NewState);

	NOT_SUPPORTED("RHISetSamplerState-Domain");
}

void FMetalDynamicRHI::RHISetShaderSampler(FGeometryShaderRHIParamRef GeometryShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	DYNAMIC_CAST_METALRESOURCE(SamplerState,NewState);

	NOT_SUPPORTED("RHISetSamplerState-Geometry");
}

void FMetalDynamicRHI::RHISetShaderSampler(FPixelShaderRHIParamRef PixelShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	DYNAMIC_CAST_METALRESOURCE(SamplerState,NewState);

	[FMetalManager::GetContext() setFragmentSamplerState:NewState->State atIndex:SamplerIndex];
}

void FMetalDynamicRHI::RHISetShaderSampler(FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	DYNAMIC_CAST_METALRESOURCE(SamplerState,NewState);

	NOT_SUPPORTED("RHISetSamplerState-Compute");
}


void FMetalDynamicRHI::RHISetShaderParameter(FVertexShaderRHIParamRef VertexShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	FMetalManager::Get()->GetShaderParameters(CrossCompiler::SHADER_STAGE_VERTEX).Set(BufferIndex, BaseIndex, NumBytes, NewValue);
}

void FMetalDynamicRHI::RHISetShaderParameter(FHullShaderRHIParamRef HullShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	NOT_SUPPORTED("RHISetShaderParameter-Hull");

}

void FMetalDynamicRHI::RHISetShaderParameter(FPixelShaderRHIParamRef PixelShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	FMetalManager::Get()->GetShaderParameters(CrossCompiler::SHADER_STAGE_PIXEL).Set(BufferIndex, BaseIndex, NumBytes, NewValue);
}

void FMetalDynamicRHI::RHISetShaderParameter(FDomainShaderRHIParamRef DomainShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	NOT_SUPPORTED("RHISetShaderParameter-Domain");

}

void FMetalDynamicRHI::RHISetShaderParameter(FGeometryShaderRHIParamRef GeometryShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	NOT_SUPPORTED("RHISetShaderParameter-Geometry");

}

void FMetalDynamicRHI::RHISetShaderParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	NOT_SUPPORTED("RHISetShaderParameter-Compute");
}

void FMetalDynamicRHI::RHISetShaderUniformBuffer(FVertexShaderRHIParamRef VertexShaderRHI, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	DYNAMIC_CAST_METALRESOURCE(VertexShader, VertexShader);
	VertexShader->BoundUniformBuffers[BufferIndex] = BufferRHI;
	VertexShader->DirtyUniformBuffers |= 1 << BufferIndex;

	auto& Bindings = VertexShader->Bindings;
	check(BufferIndex < Bindings.NumUniformBuffers);
	if (Bindings.bHasRegularUniformBuffers)
	{
		auto* UB = (FMetalUniformBuffer*)BufferRHI;
		[FMetalManager::GetContext() setVertexBuffer:UB->Buffer offset:UB->Offset atIndex:BufferIndex];
	}
}

void FMetalDynamicRHI::RHISetShaderUniformBuffer(FHullShaderRHIParamRef HullShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	NOT_SUPPORTED("RHISetShaderUniformBuffer-Hull");
}

void FMetalDynamicRHI::RHISetShaderUniformBuffer(FDomainShaderRHIParamRef DomainShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	NOT_SUPPORTED("RHISetShaderUniformBuffer-Domain");
}

void FMetalDynamicRHI::RHISetShaderUniformBuffer(FGeometryShaderRHIParamRef GeometryShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	NOT_SUPPORTED("RHISetShaderUniformBuffer-Geometry");
}

void FMetalDynamicRHI::RHISetShaderUniformBuffer(FPixelShaderRHIParamRef PixelShaderRHI, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	DYNAMIC_CAST_METALRESOURCE(PixelShader, PixelShader);
	PixelShader->BoundUniformBuffers[BufferIndex] = BufferRHI;
	PixelShader->DirtyUniformBuffers |= 1 << BufferIndex;

	auto& Bindings = PixelShader->Bindings;
	check(BufferIndex < Bindings.NumUniformBuffers);
	if (Bindings.bHasRegularUniformBuffers)
	{
		auto* UB = (FMetalUniformBuffer*)BufferRHI;
		[FMetalManager::GetContext() setFragmentBuffer:UB->Buffer offset:UB->Offset atIndex:BufferIndex];
	}
}

void FMetalDynamicRHI::RHISetShaderUniformBuffer(FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	NOT_SUPPORTED("RHISetShaderUniformBuffer-Compute");
}


void FMetalDynamicRHI::RHISetDepthStencilState(FDepthStencilStateRHIParamRef NewStateRHI, uint32 StencilRef)
{
	DYNAMIC_CAST_METALRESOURCE(DepthStencilState,NewState);

	NewState->Set();
}

void FMetalDynamicRHI::RHISetBlendState(FBlendStateRHIParamRef NewStateRHI, const FLinearColor& BlendFactor)
{
	DYNAMIC_CAST_METALRESOURCE(BlendState,NewState);

	NewState->Set();
}


void FMetalDynamicRHI::RHISetRenderTargets(uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargets,
	FTextureRHIParamRef NewDepthStencilTargetRHI, uint32 NumUAVs, const FUnorderedAccessViewRHIParamRef* UAVs)
{
	FMetalManager* Manager = FMetalManager::Get();

	FRHIDepthRenderTargetView DepthView(NewDepthStencilTargetRHI, ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::ENoAction);
	FRHISetRenderTargetsInfo Info(NumSimultaneousRenderTargets, NewRenderTargets, DepthView);
	RHISetRenderTargetsAndClear(Info);
}

void FMetalDynamicRHI::RHIDiscardRenderTargets(bool Depth, bool Stencil, uint32 ColorBitMask)
{
}

void FMetalDynamicRHI::RHISetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo)
{
	FMetalManager* Manager = FMetalManager::Get();

#if 1
	Manager->SetRenderTargetsInfo(RenderTargetsInfo);
#else
	for (int32 RenderTargetIndex = 0; RenderTargetIndex < MaxMetalRenderTargets; RenderTargetIndex++)
	{
		const FRHIRenderTargetView& RenderTargetView = RenderTargetsInfo.ColorRenderTarget[RenderTargetIndex];
		// update the current RTs
		if (RenderTargetIndex < RenderTargetsInfo.NumColorRenderTargets && RenderTargetView.Texture != NULL)
		{
			FMetalSurface& Surface = GetMetalSurfaceFromRHITexture(RenderTargetView.Texture);
			Manager->SetCurrentRenderTarget(&Surface, RenderTargetIndex, RenderTargetView.MipIndex, RenderTargetView.ArraySliceIndex, GetMetalRTLoadAction(RenderTargetView.LoadAction), GetMetalRTStoreAction(RenderTargetView.StoreAction), RenderTargetsInfo.NumColorRenderTargets);
		}
		else
		{
			Manager->SetCurrentRenderTarget(NULL, RenderTargetIndex, 0, 0, MTLLoadActionDontCare, MTLStoreActionStore, RenderTargetsInfo.NumColorRenderTargets);
		}
	}

	if (RenderTargetsInfo.DepthStencilRenderTarget.Texture)
	{
		FMetalSurface& Surface = GetMetalSurfaceFromRHITexture(RenderTargetsInfo.DepthStencilRenderTarget.Texture);
		Manager->SetCurrentDepthStencilTarget(&Surface, 
			GetMetalRTLoadAction(RenderTargetsInfo.DepthStencilRenderTarget.DepthLoadAction), 
			GetMetalRTStoreAction(RenderTargetsInfo.DepthStencilRenderTarget.DepthStoreAction), 
			RenderTargetsInfo.DepthClearValue,
			GetMetalRTLoadAction(RenderTargetsInfo.DepthStencilRenderTarget.StencilLoadAction),
			GetMetalRTStoreAction(RenderTargetsInfo.DepthStencilRenderTarget.StencilStoreAction),
			RenderTargetsInfo.StencilClearValue);
	}
	else
	{
		Manager->SetCurrentDepthStencilTarget(NULL);
	}

	// now that we have a new render target, we need a new context to render to it!
	Manager->UpdateContext();
#endif


	// Set the viewport to the full size of render target 0.
	if (RenderTargetsInfo.ColorRenderTarget[0].Texture)
	{
		const FRHIRenderTargetView& RenderTargetView = RenderTargetsInfo.ColorRenderTarget[0];
		FMetalSurface& RenderTarget = GetMetalSurfaceFromRHITexture(RenderTargetView.Texture);

		uint32 Width = FMath::Max((uint32)(RenderTarget.Texture.width >> RenderTargetView.MipIndex), (uint32)1);
		uint32 Height = FMath::Max((uint32)(RenderTarget.Texture.height >> RenderTargetView.MipIndex), (uint32)1);

		RHISetViewport(0, 0, 0.0f, Width, Height, 1.0f);
	}
}

// Occlusion/Timer queries.
void FMetalDynamicRHI::RHIBeginRenderQuery(FRenderQueryRHIParamRef QueryRHI)
{
	DYNAMIC_CAST_METALRESOURCE(RenderQuery,Query);

	Query->Begin();
}

void FMetalDynamicRHI::RHIEndRenderQuery(FRenderQueryRHIParamRef QueryRHI)
{
	DYNAMIC_CAST_METALRESOURCE(RenderQuery,Query);

	Query->End();
}


void FMetalDynamicRHI::RHIDrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	SCOPE_CYCLE_COUNTER(STAT_MetalDrawCallTime);
	//checkf(NumInstances == 1, TEXT("Currently only 1 instance is supported"));
	
	RHI_DRAW_CALL_STATS(PrimitiveType,NumInstances*NumPrimitives);

	// how many verts to render
	uint32 NumVertices = GetVertexCountForPrimitiveCount(NumPrimitives, PrimitiveType);

	// finalize any pending state
	FMetalManager::Get()->PrepareToDraw(NumVertices);

	// draw!
	[FMetalManager::GetContext() drawPrimitives:TranslatePrimitiveType(PrimitiveType)
									vertexStart:BaseVertexIndex
									vertexCount:NumVertices
								  instanceCount:NumInstances];
}

void FMetalDynamicRHI::RHIDrawPrimitiveIndirect(uint32 PrimitiveType, FVertexBufferRHIParamRef ArgumentBufferRHI, uint32 ArgumentOffset)
{
	NOT_SUPPORTED("RHIDrawPrimitiveIndirect");
}


void FMetalDynamicRHI::RHIDrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 MinIndex,
	uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	SCOPE_CYCLE_COUNTER(STAT_MetalDrawCallTime);
	//checkf(NumInstances == 1, TEXT("Currently only 1 instance is supported"));
	checkf(BaseVertexIndex  == 0, TEXT("BaseVertexIndex must be 0, see GRHISupportsBaseVertexIndex"));

	RHI_DRAW_CALL_STATS(PrimitiveType,NumInstances*NumPrimitives);

	DYNAMIC_CAST_METALRESOURCE(IndexBuffer,IndexBuffer);

	// finalize any pending state
	FMetalManager::Get()->PrepareToDraw(NumVertices);
	
	uint32 NumIndices = GetVertexCountForPrimitiveCount(NumPrimitives, PrimitiveType);

	[FMetalManager::GetContext() drawIndexedPrimitives:TranslatePrimitiveType(PrimitiveType)
											indexCount:NumIndices
											 indexType:IndexBuffer->IndexType
										   indexBuffer:IndexBuffer->Buffer
									 indexBufferOffset:IndexBuffer->Offset + StartIndex * IndexBuffer->GetStride()
										 instanceCount:NumInstances];
}

void FMetalDynamicRHI::RHIDrawIndexedIndirect(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, FStructuredBufferRHIParamRef ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances)
{
	NOT_SUPPORTED("RHIDrawIndexedIndirect");
}

void FMetalDynamicRHI::RHIDrawIndexedPrimitiveIndirect(uint32 PrimitiveType,FIndexBufferRHIParamRef IndexBufferRHI,FVertexBufferRHIParamRef ArgumentBufferRHI,uint32 ArgumentOffset)
{
	NOT_SUPPORTED("RHIDrawIndexedPrimitiveIndirect");
}


/** Some locally global variables to track the pending primitive information uised in RHIEnd*UP functions */
static uint32 GPendingVertexBufferOffset = 0xFFFFFFFF;
static uint32 GPendingVertexDataStride;

static uint32 GPendingIndexBufferOffset = 0xFFFFFFFF;
static uint32 GPendingIndexDataStride;

static uint32 GPendingPrimitiveType;
static uint32 GPendingNumPrimitives;

void FMetalDynamicRHI::RHIBeginDrawPrimitiveUP( uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData)
{
	checkSlow(GPendingVertexBufferOffset == 0xFFFFFFFF);

	// allocate space
	GPendingVertexBufferOffset = FMetalManager::Get()->AllocateFromRingBuffer(VertexDataStride * NumVertices);

	// get the pointer to send back for writing
	uint8* RingBufferBytes = (uint8*)[FMetalManager::Get()->GetRingBuffer() contents];
	OutVertexData = RingBufferBytes + GPendingVertexBufferOffset;
	
	GPendingPrimitiveType = PrimitiveType;
	GPendingNumPrimitives = NumPrimitives;
	GPendingVertexDataStride = VertexDataStride;
}


void FMetalDynamicRHI::RHIEndDrawPrimitiveUP()
{
	SCOPE_CYCLE_COUNTER(STAT_MetalDrawCallTime);

	RHI_DRAW_CALL_STATS(GPendingPrimitiveType,GPendingNumPrimitives);

	// set the vertex buffer
	[FMetalManager::GetContext() setVertexBuffer:FMetalManager::Get()->GetRingBuffer() offset:GPendingVertexBufferOffset atIndex:UNREAL_TO_METAL_BUFFER_INDEX(0)];
	
	// how many to draw
	uint32 NumVertices = GetVertexCountForPrimitiveCount(GPendingNumPrimitives, GPendingPrimitiveType);

	// last minute draw setup
	FMetalManager::Get()->PrepareToDraw(0);
	
	[FMetalManager::GetContext() drawPrimitives:TranslatePrimitiveType(GPendingPrimitiveType)
									vertexStart:0
									vertexCount:NumVertices
								  instanceCount:1];
	
	// mark temp memory as usable
	GPendingVertexBufferOffset = 0xFFFFFFFF;
}

void FMetalDynamicRHI::RHIBeginDrawIndexedPrimitiveUP( uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData, uint32 MinVertexIndex, uint32 NumIndices, uint32 IndexDataStride, void*& OutIndexData)
{
	checkSlow(GPendingVertexBufferOffset == 0xFFFFFFFF);
	checkSlow(GPendingIndexBufferOffset == 0xFFFFFFFF);

	// allocate space
	GPendingVertexBufferOffset = FMetalManager::Get()->AllocateFromRingBuffer(VertexDataStride * NumVertices);
	GPendingIndexBufferOffset = FMetalManager::Get()->AllocateFromRingBuffer(IndexDataStride * NumIndices);
	
	// get the pointers to send back for writing
	uint8* RingBufferBytes = (uint8*)[FMetalManager::Get()->GetRingBuffer() contents];
	OutVertexData = RingBufferBytes + GPendingVertexBufferOffset;
	OutIndexData = RingBufferBytes + GPendingIndexBufferOffset;
	
	GPendingPrimitiveType = PrimitiveType;
	GPendingNumPrimitives = NumPrimitives;
	GPendingIndexDataStride = IndexDataStride;

	GPendingVertexDataStride = VertexDataStride;
}

void FMetalDynamicRHI::RHIEndDrawIndexedPrimitiveUP()
{
	SCOPE_CYCLE_COUNTER(STAT_MetalDrawCallTime);

	RHI_DRAW_CALL_STATS(GPendingPrimitiveType,GPendingNumPrimitives);

	// set the vertex buffer
	[FMetalManager::GetContext() setVertexBuffer:FMetalManager::Get()->GetRingBuffer() offset:GPendingVertexBufferOffset atIndex:UNREAL_TO_METAL_BUFFER_INDEX(0)];

	// how many to draw
	uint32 NumIndices = GetVertexCountForPrimitiveCount(GPendingNumPrimitives, GPendingPrimitiveType);

	// last minute draw setup
	FMetalManager::Get()->PrepareToDraw(0);
	
	[FMetalManager::GetContext() drawIndexedPrimitives:TranslatePrimitiveType(GPendingPrimitiveType)
											indexCount:NumIndices
											 indexType:(GPendingIndexDataStride == 2) ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32
										   indexBuffer:FMetalManager::Get()->GetRingBuffer()
									 indexBufferOffset:GPendingIndexBufferOffset
										 instanceCount:1];
	
	// mark temp memory as usable
	GPendingVertexBufferOffset = 0xFFFFFFFF;
	GPendingIndexBufferOffset = 0xFFFFFFFF;
}


void FMetalDynamicRHI::RHIClear(bool bClearColor,const FLinearColor& Color, bool bClearDepth,float Depth, bool bClearStencil,uint32 Stencil, FIntRect ExcludeRect)
{
	FMetalDynamicRHI::RHIClearMRT(bClearColor, 1, &Color, bClearDepth, Depth, bClearStencil, Stencil, ExcludeRect);
}

void FMetalDynamicRHI::RHIClearMRT(bool bClearColor,int32 NumClearColors,const FLinearColor* ClearColorArray,bool bClearDepth,float Depth,bool bClearStencil,uint32 Stencil, FIntRect ExcludeRect)
{
	// this needs to be handled in SetRenderTarget LoadAction 
}



void FMetalDynamicRHI::RHISuspendRendering()
{
	// Not supported
}

void FMetalDynamicRHI::RHIResumeRendering()
{
	// Not supported
}

bool FMetalDynamicRHI::RHIIsRenderingSuspended()
{
	// Not supported
	return false;
}

void FMetalDynamicRHI::RHIBlockUntilGPUIdle()
{
	NOT_SUPPORTED("RHIBlockUntilGPUIdle");
}

uint32 FMetalDynamicRHI::RHIGetGPUFrameCycles()
{
	return GGPUFrameTime;
}

void FMetalDynamicRHI::RHIAutomaticCacheFlushAfterComputeShader(bool bEnable) 
{
	NOT_SUPPORTED("RHIAutomaticCacheFlushAfterComputeShader");
}

void FMetalDynamicRHI::RHIFlushComputeShaderCache()
{
	NOT_SUPPORTED("RHIFlushComputeShaderCache");
}

void FMetalDynamicRHI::RHIExecuteCommandList(FRHICommandList* RHICmdList)
{
	NOT_SUPPORTED("RHIExecuteCommandList");
}

void FMetalDynamicRHI::RHIEnableDepthBoundsTest(bool bEnable, float MinDepth, float MaxDepth)
{
	// not supported
}

