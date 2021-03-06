// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"
#include "DynamicCastHandler.h"

#define LOCTEXT_NAMESPACE "DynamicCastHandler"

//////////////////////////////////////////////////////////////////////////
// FKCHandler_DynamicCast

void FKCHandler_DynamicCast::RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node)
{
	FNodeHandlingFunctor::RegisterNets(Context, Node);

	// Create a term to determine if the cast was successful or not
	FBPTerminal* BoolTerm = Context.CreateLocalTerminal();
	BoolTerm->Type.PinCategory = CompilerContext.GetSchema()->PC_Boolean;
	BoolTerm->Source = Node;
	BoolTerm->Name = Context.NetNameMap->MakeValidName(Node) + TEXT("_CastSuccess");
	BoolTermMap.Add(Node, BoolTerm);
}

void FKCHandler_DynamicCast::RegisterNet(FKismetFunctionContext& Context, UEdGraphPin* Net)
{
	FBPTerminal* Term = Context.CreateLocalTerminalFromPinAutoChooseScope(Net, Context.NetNameMap->MakeValidName(Net));
	Context.NetMap.Add(Net, Term);
}

void FKCHandler_DynamicCast::Compile(FKismetFunctionContext& Context, UEdGraphNode* Node)
{
	const UK2Node_DynamicCast* DynamicCastNode = CastChecked<UK2Node_DynamicCast>(Node);

	if (DynamicCastNode->TargetType == NULL)
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("BadCastNoTargetType_Error", "Node @@ has an invalid target type, please delete and recreate it").ToString(), Node);
	}

	// Self Pin
	UEdGraphPin* SourceObjectPin = DynamicCastNode->GetCastSourcePin();
	UEdGraphPin* PinToTry = FEdGraphUtilities::GetNetFromPin(SourceObjectPin);
	FBPTerminal** ObjectToCast = Context.NetMap.Find(PinToTry);

	if (!ObjectToCast)
	{
		ObjectToCast = Context.LiteralHackMap.Find(PinToTry);

		if (!ObjectToCast || !(*ObjectToCast))
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("InvalidConnectionOnNode_Error", "Node @@ has an invalid connection on @@").ToString(), Node, SourceObjectPin);
			return;
		}
	}

	// Output pin
	const UEdGraphPin* CastOutputPin = DynamicCastNode->GetCastResultPin();
	if( !CastOutputPin )
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("InvalidDynamicCastClass_Error", "Node @@ has an invalid target class").ToString(), Node);
		return;
	}

	FBPTerminal** CastResultTerm = Context.NetMap.Find(CastOutputPin);
	if (!CastResultTerm || !(*CastResultTerm))
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("InvalidDynamicCastClass_CompilerError", "Node @@ has an invalid target class. (Inner compiler error?)").ToString(), Node);
		return;
	}

	// Create a literal term from the class specified in the node
	FBPTerminal* ClassTerm = Context.CreateLocalTerminal(ETerminalSpecification::TS_Literal);
	ClassTerm->Name = DynamicCastNode->TargetType->GetName();
	ClassTerm->bIsLiteral = true;
	ClassTerm->Source = Node;
	ClassTerm->ObjectLiteral = DynamicCastNode->TargetType;

	// Find the boolean intermediate result term, so we can track whether the cast was successful
	FBPTerminal* BoolTerm = *BoolTermMap.Find(DynamicCastNode);

	UClass const* const InputObjClass  = Cast<UClass>((*ObjectToCast)->Type.PinSubCategoryObject.Get());
	UClass const* const OutputObjClass = Cast<UClass>((*CastResultTerm)->Type.PinSubCategoryObject.Get());

	const bool bIsOutputInterface = ((OutputObjClass != NULL) && OutputObjClass->HasAnyClassFlags(CLASS_Interface));
	const bool bIsInputInterface  = ((InputObjClass != NULL) && InputObjClass->HasAnyClassFlags(CLASS_Interface));

	EKismetCompiledStatementType CastOpType = KCST_DynamicCast;
	if (bIsInputInterface)
	{
		if (bIsOutputInterface)
		{
			CastOpType = KCST_CrossInterfaceCast;
		}
		else
		{
			CastOpType = KCST_CastInterfaceToObj;
		}
	}
	else if (bIsOutputInterface)
	{
		CastOpType = KCST_CastObjToInterface;
	}

	if (KCST_MetaCast == CastType)
	{
		if (bIsInputInterface || bIsOutputInterface)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("InvalidClassDynamicCastClass_Error", "Node @@ has an invalid target class. Interfaces are not supported.").ToString(), Node);
			return;
		}
		CastOpType = KCST_MetaCast;
	}

	// Cast Statement
	FBlueprintCompiledStatement& CastStatement = Context.AppendStatementForNode(Node);
	CastStatement.Type = CastOpType;
	CastStatement.LHS = *CastResultTerm;
	CastStatement.RHS.Add(ClassTerm);
	CastStatement.RHS.Add(*ObjectToCast);

	UEdGraphPin* SuccessPin = DynamicCastNode->GetValidCastPin();
	bool const bIsPureCast = (SuccessPin == nullptr);

	if (!bIsPureCast)
	{
		// Check result of cast statement
		FBlueprintCompiledStatement& CheckResultStatement = Context.AppendStatementForNode(Node);
		const UClass* SubObjectClass = Cast<UClass>((*CastResultTerm)->Type.PinSubCategoryObject.Get());
		const bool bIsInterfaceCast = (SubObjectClass != NULL && SubObjectClass->HasAnyClassFlags(CLASS_Interface));

		CheckResultStatement.Type = KCST_ObjectToBool;
		CheckResultStatement.LHS = BoolTerm;
		CheckResultStatement.RHS.Add(*CastResultTerm);
	
		UEdGraphPin* FailurePin = DynamicCastNode->GetInvalidCastPin();
		check(FailurePin != nullptr);
		// Failure condition...skip to the failed output
		FBlueprintCompiledStatement& FailCastGoto = Context.AppendStatementForNode(Node);
		FailCastGoto.Type = KCST_GotoIfNot;
		FailCastGoto.LHS = BoolTerm;
		Context.GotoFixupRequestMap.Add(&FailCastGoto, FailurePin);

		// Successful cast...hit the success output node
		FBlueprintCompiledStatement& SuccessCastGoto = Context.AppendStatementForNode(Node);
		SuccessCastGoto.Type = KCST_UnconditionalGoto;
		SuccessCastGoto.LHS = BoolTerm;
		Context.GotoFixupRequestMap.Add(&SuccessCastGoto, SuccessPin);
	}
	
}

#undef LOCTEXT_NAMESPACE
