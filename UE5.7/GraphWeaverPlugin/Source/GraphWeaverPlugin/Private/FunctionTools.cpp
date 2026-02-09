// Copyright 2026 RainButterfly. All Rights Reserved.

#include "FunctionTools.h"
#include "GraphNode.h"
#include "GraphView.h"
#include "Engine/Engine.h"
#include "UObject/UObjectGlobals.h"
#include <functional>

UFunctionTools_GraphWeaver::UFunctionTools_GraphWeaver()
{
}

UFunctionTools_GraphWeaver::~UFunctionTools_GraphWeaver()
{
}

FString UFunctionTools_GraphWeaver::FlipString(FString& SourceString)
{
	FString rr;
	for (int32 i = SourceString.Len() - 1; i >= 0; i--)
		rr += SourceString[i];
	return rr;
}

TArray<FString> UFunctionTools_GraphWeaver::GetNodePath(FGraphNodeDescription& SourceDesc)
{
	TArray<FString> rr;
	UGraphView& SourceView = *SourceDesc.SourceGraphNode->SourceGraphView;

	//第一个int32记录分叉点元素是在AlreadyRecorded里面的Index,第二个int32记录下一步应该走的Parent的下标
	TArray<std::tuple<int32, int32>> ForkPoint;
	//只记录有父亲节点连接的.对于每个父亲的自己的路径,都应该反向Id拼接,最后在合并到rr的时候再进行一次反向
	TArray<FString> ForkPointPath;
	for (int32 i = 0 ; i < SourceDesc.Family.Parents.Num() ; i++)
	{
		ForkPoint.Emplace(SourceDesc.IndexInRecorded, i);
		ForkPointPath.Emplace(FlipString(SourceDesc.LHCode_G_InputMirror.SelfId));
	}
	int32 IndexOfForkPoint = 0;

	int32 IndexNowVisit;

	{
		std::function<void()> MostLeftAppend = [&]()
		{
			if (IndexNowVisit == 0)//根节点停止向上追溯
				return ;
			auto& NowDes = SourceView.RealNodes[IndexNowVisit];
			ForkPointPath[IndexOfForkPoint].Append(FlipString( NowDes.LHCode_G_InputMirror.SelfId));
			for (auto& LoseLinkedParentPath : NowDes.LHCode_G_InputMirror.ParentCodes)
			{
				rr.Emplace(LoseLinkedParentPath + FlipString(ForkPointPath[IndexOfForkPoint]));
			}

			int32 ExtraForkParentNum = NowDes.Family.Parents.Num() - 1;
			for (int32 i = 1 ; i <= ExtraForkParentNum ; i++)
			{
				ForkPoint.Emplace(NowDes.IndexInRecorded, i);
				ForkPointPath.Emplace(ForkPointPath[IndexOfForkPoint]);
			}

			if (NowDes.Family.Parents.Num() == 0)
			{
				ForkPoint.RemoveAt(IndexOfForkPoint);
				ForkPointPath.RemoveAt(IndexOfForkPoint);
				return ;
			}
			
			for (auto& Parent : NowDes.Family.Parents)
			{
				IndexNowVisit = Parent.IndexInRealNodes;
				break ;
			}

			MostLeftAppend();
		};

		
		//开启分支之旅.不包含分支节点本体
		auto f = [&]()
		{
			auto[IndexInAl, Ranking] = ForkPoint[IndexOfForkPoint];
			auto& NowDes = SourceView.RealNodes[IndexInAl];
			int32 i = -1;
			for (auto& Parent : NowDes.Family.Parents)
			{
				i++;
				if (i == Ranking)
				{
					IndexNowVisit = Parent.IndexInRealNodes;
					break ;
				}
			}

			MostLeftAppend();
		};
		
		while (IndexOfForkPoint < ForkPoint.Num())
		{
			f();
			IndexOfForkPoint++;
		}
	}

	for (auto& EvePath : ForkPointPath)
	{
		rr.Emplace(FlipString(EvePath));
	}

	for (auto& LoseLinkedParentPath : SourceDesc.LHCode_G_InputMirror.ParentCodes)
		rr.Emplace(LoseLinkedParentPath + SourceDesc.LHCode_G_InputMirror.SelfId);
	
	return rr;
}

TArray<int32> UFunctionTools_GraphWeaver::ObtainAllActivatedChildDescription(UGraphView* SourceGraphView,
	FGraphNodeDescription& Des)
{
	TArray<int32> rr;
	if (Des.IndexInRecorded >= SourceGraphView->RealNodes.Num())[[unlikely]]
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, FString::Printf(
			TEXT("Description Invalid. Des.IndexInRecorded is larger than SourceGraphView.RealNodes.Num.GraphViewName: %s, GraphViewOuter: %s, Des.ExplicitName: %s.Func: ObtainAllActivatedChildDescription"),
			*SourceGraphView->GraphViewName, *SourceGraphView->GetOuter()->GetName(), *Des.ExplicitName));
		UE_LOG(LogTemp, Error, TEXT("Description Invalid. Des.IndexInRecorded is larger than SourceGraphView.RealNodes.Num.GraphViewName: %s, GraphViewOuter: %s, Des.ExplicitName: %s.Func: ObtainAllActivatedChildDescription"),
			*SourceGraphView->GraphViewName, *SourceGraphView->GetOuter()->GetName(), *Des.ExplicitName);
		return rr;
	}
	if (Des.IndexInRecorded == -1)[[unlikely]]
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, FString::Printf(
			TEXT("Description's IndexInRecorded equal -1. Description Name: %s.Func: ObtainAllActivatedChildDescription"), *Des.ExplicitName));
		UE_LOG(LogTemp, Error, TEXT("Description's IndexInRecorded equal -1. Description Name: %s.Func: ObtainAllActivatedChildDescription"), *Des.ExplicitName);
		return rr;
	}
	if (!IsValid(SourceGraphView))[[unlikely]]
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("SourceGraphView is invalid.Func: ObtainAllActivatedChildDescription"));
		UE_LOG(LogTemp, Error, TEXT("SourceGraphView is invalid.Func: ObtainAllActivatedChildDescription"));
		return rr;
	}

	TArray<int32> ForkIndex;
	ForkIndex.Emplace(Des.IndexInRecorded);
	int32 ForkArrived = 0;

	std::function<void()> f = [&ForkArrived, SourceGraphView, &ForkIndex, &rr]()
	{
		auto& NowDes = SourceGraphView->RealNodes[ForkIndex[ForkArrived]];
		for (auto& Child : NowDes.Family.Children)
		{
			ForkIndex.Emplace(Child.IndexInRealNodes);
			if (SourceGraphView->RealNodes[Child.IndexInRealNodes].Activated == true)
				rr.Emplace(Child.IndexInRealNodes);
		}
	};
	while (ForkArrived < ForkIndex.Num())
	{
		f();
		ForkArrived++;
	}
	
	return rr;
}

TArray<int32> UFunctionTools_GraphWeaver::ObtainAllActivatedBroDescription(UGraphView* SourceGraphView, FGraphNodeDescription& Des)
{
	TArray<int32> rr;
	if (Des.IndexInRecorded >= SourceGraphView->RealNodes.Num())[[unlikely]]
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, FString::Printf(
			TEXT("Description Invalid. Des.IndexInRecorded is larger than SourceGraphView.RealNodes.Num.GraphViewName: %s, GraphViewOuter: %s, Des.ExplicitName: %s.Func: ObtainAllActivatedBroDescription"),
			*SourceGraphView->GraphViewName, *SourceGraphView->GetOuter()->GetName(), *Des.ExplicitName));
		UE_LOG(LogTemp, Error, TEXT("Description Invalid. Des.IndexInRecorded is larger than SourceGraphView.RealNodes.Num.GraphViewName: %s, GraphViewOuter: %s, Des.ExplicitName: %s.Func: ObtainAllActivatedBroDescription"),
			*SourceGraphView->GraphViewName, *SourceGraphView->GetOuter()->GetName(), *Des.ExplicitName);
		return rr;
	}
	if (Des.IndexInRecorded == -1)[[unlikely]]
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, FString::Printf(
			TEXT("Description's IndexInRecorded equal -1. Description Name: %s.Func: ObtainAllActivatedBroDescription"), *Des.ExplicitName));
		UE_LOG(LogTemp, Error, TEXT("Description's IndexInRecorded equal -1. Description Name: %s.Func: ObtainAllActivatedBroDescription"), *Des.ExplicitName);
		return rr;
	}
	if (!IsValid(SourceGraphView))[[unlikely]]
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("SourceGraphView is invalid.Func: ObtainAllActivatedBroDescription"));
		UE_LOG(LogTemp, Error, TEXT("SourceGraphView is invalid.Func: ObtainAllActivatedBroDescription"));
		return rr;
	}

	TArray<int32> ForkIndex;
	ForkIndex.Emplace(Des.IndexInRecorded);
	int32 ForkArrived = 0;
	auto f = [&]()
	{
		auto& NowDes = SourceGraphView->RealNodes[ForkIndex[ForkArrived]];
		for (auto& Bro : NowDes.Family.Brothers)
		{
			uint8 Matched = 0;
			for (int32 IndexRecorded : ForkIndex)
			{
				if (IndexRecorded == Bro.IndexInRealNodes)
				{
					Matched = 1;
					break ;
				}
			}
			if (Matched == 1)
				continue ;
			ForkIndex.Emplace(Bro.IndexInRealNodes);
			if (SourceGraphView->RealNodes[Bro.IndexInRealNodes].Activated == true)
			{
				rr.Emplace(Bro.IndexInRealNodes);
			}
		}
	};
	while (ForkArrived < ForkIndex.Num())
	{
		f();
		ForkArrived++;
	}
	return rr;
}

TArray<int32> UFunctionTools_GraphWeaver::ObtainDirectActivatedChildAndBroDes(UGraphView* SourceGraphView,
	FGraphNodeDescription& Des)
{
	TArray<int32> rr;
	if (Des.IndexInRecorded >= SourceGraphView->RealNodes.Num())[[unlikely]]
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, FString::Printf(
			TEXT("Description Invalid. Des.IndexInRecorded is larger than SourceGraphView.RealNodes.Num.GraphViewName: %s, GraphViewOuter: %s, Des.ExplicitName: %s.Func: ObtainDirectActivatedChildAndBroDes"),
			*SourceGraphView->GraphViewName, *SourceGraphView->GetOuter()->GetName(), *Des.ExplicitName));
		UE_LOG(LogTemp, Error, TEXT("Description Invalid. Des.IndexInRecorded is larger than SourceGraphView.RealNodes.Num.GraphViewName: %s, GraphViewOuter: %s, Des.ExplicitName: %s.Func: ObtainDirectActivatedChildAndBroDes"),
			*SourceGraphView->GraphViewName, *SourceGraphView->GetOuter()->GetName(), *Des.ExplicitName);
		return rr;
	}
	if (Des.IndexInRecorded == -1)[[unlikely]]
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, FString::Printf(
			TEXT("Description's IndexInRecorded equal -1. Description Name: %s.Func: ObtainDirectActivatedChildAndBroDes"), *Des.ExplicitName));
		UE_LOG(LogTemp, Error, TEXT("Description's IndexInRecorded equal -1. Description Name: %s.Func: ObtainDirectActivatedChildAndBroDes"), *Des.ExplicitName);
		return rr;
	}
	if (!IsValid(SourceGraphView))[[unlikely]]
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("SourceGraphView is invalid.Func: ObtainDirectActivatedChildAndBroDes"));
		UE_LOG(LogTemp, Error, TEXT("SourceGraphView is invalid.Func: ObtainDirectActivatedChildAndBroDes"));
		return rr;
	}

	for (auto& Bro : Des.Family.Brothers)
	{
		if (SourceGraphView->RealNodes[Bro.IndexInRealNodes].Activated == true)
			rr.Emplace(Bro.IndexInRealNodes);
	}
	for (auto& Child : Des.Family.Children)
	{
		if (SourceGraphView->RealNodes[Child.IndexInRealNodes].Activated == true)
			rr.Emplace(Child.IndexInRealNodes);
	}
	
	return rr;
}

TArray<int32> UFunctionTools_GraphWeaver::ObtainAllChildDes(UGraphView* SourceGraphView, FGraphNodeDescription& Des)
{
	TArray<int32> rr;
	if (Des.IndexInRecorded >= SourceGraphView->RealNodes.Num())[[unlikely]]
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, FString::Printf(
			TEXT("Description Invalid. Des.IndexInRecorded is larger than SourceGraphView.RealNodes.Num.GraphViewName: %s, GraphViewOuter: %s, Des.ExplicitName: %s.Func: ObtainAllChildDes"),
			*SourceGraphView->GraphViewName, *SourceGraphView->GetOuter()->GetName(), *Des.ExplicitName));
		UE_LOG(LogTemp, Error, TEXT("Description Invalid. Des.IndexInRecorded is larger than SourceGraphView.RealNodes.Num.GraphViewName: %s, GraphViewOuter: %s, Des.ExplicitName: %s.Func: ObtainAllChildDes"),
			*SourceGraphView->GraphViewName, *SourceGraphView->GetOuter()->GetName(), *Des.ExplicitName);
		return rr;
	}
	if (Des.IndexInRecorded == -1)[[unlikely]]
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, FString::Printf(
			TEXT("Description's IndexInRecorded equal -1. Description Name: %s.Func: ObtainAllChildDes"), *Des.ExplicitName));
		UE_LOG(LogTemp, Error, TEXT("Description's IndexInRecorded equal -1. Description Name: %s.Func: ObtainAllChildDes"), *Des.ExplicitName);
		return rr;
	}
	if (!IsValid(SourceGraphView))[[unlikely]]
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("SourceGraphView is invalid.Func: ObtainAllChildDes"));
		UE_LOG(LogTemp, Error, TEXT("SourceGraphView is invalid.Func: ObtainAllChildDes"));
		return rr;
	}

	TArray<int32> ForkIndex;
	int32 IndexArrived = 0;
	ForkIndex.Emplace(Des.IndexInRecorded);
	
	auto f = [&]()
	{
		auto& NowDes = SourceGraphView->RealNodes[ForkIndex[IndexArrived]];
		for (auto& Child : NowDes.Family.Children)
		{
			ForkIndex.Emplace(Child.IndexInRealNodes);
			rr.Emplace(Child.IndexInRealNodes);
		}
	};

	while (IndexArrived < ForkIndex.Num())
	{
		f();
		IndexArrived++;
	}
	return rr;
}

UGraphView* UFunctionTools_GraphWeaver::CreateGraphView_NotManuallyCall(UObject* Outer)
{
	UGraphView* Obj = NewObject<UGraphView>(
		Outer,
		UGraphView::StaticClass(),
		NAME_None,
		RF_Transactional);
	RealViewArray::Get().GetRealViews().Emplace(Obj);
	return Obj;
}

UGraphView* UFunctionTools_GraphWeaver::ModGraphViewBaseAttri_NotManuallyCall(UGraphView* Target, TEnumAsByte<NAConstructMethod::EConstructMethod> EnumValue, UObject* SelfOwner,
	const FString& ViewName,TEnumAsByte<NAWayToDealSameGraphNode::EWayToDealSameGraphNode> DealSameNode)
{
	Target->ConstructMethod = EnumValue;
	Target->SelfOwner = SelfOwner;
	Target->GraphViewName = ViewName;
	Target->WayToDealSameNode = DealSameNode;
	return Target;
}

UGraphView* UFunctionTools_GraphWeaver::ModGraphViewNaCon_NotManuallyCall(UGraphView* Target, FNamesConstructConfig& NamesValue)
{
	Target->NamesConstructConfig = NamesValue;
	return Target;
}

UGraphView* UFunctionTools_GraphWeaver::ModGraphViewFinalPhase_NotManuallyCall(UGraphView* Target, int32 SizeAllocate)
{
	if (SizeAllocate != 0)
		Target->AllocateGraphViewSize(SizeAllocate);
	return Target;
}

UGraphNode* UFunctionTools_GraphWeaver::CreateGraphNode_NotManuallyCall()
{
	UGraphNode* Obj = NewObject<UGraphNode>((UObject*)GetTransientPackage(), UGraphNode::StaticClass(),NAME_None, RF_Transactional);
	return Obj;
}


UGraphNode* UFunctionTools_GraphWeaver::ModNamesInput_NotManuallyCall(UGraphNode* Target, FNamesInputNode& Names)
{
	Target->NamesInput = Names;
	return Target;
}

UGraphNode* UFunctionTools_GraphWeaver::ModLHCodeInput_NotManuallyCall(UGraphNode* Target, FLHCode_G_Input& LHCode)
{
	Target->LHCode_G_Input = LHCode;
	return Target;
}

UGraphNode* UFunctionTools_GraphWeaver::CallProcessInformOrNot_NotManuallyCall(UGraphNode* Target, bool AutoBuild)
{
	if (AutoBuild)
		Target->ProcessInformAuto(Target->SourceGraphView);

	return Target;
}

UGraphNode* UFunctionTools_GraphWeaver::SetRealSourceViewForNode_NotManuallyCall(UGraphNode* Node, FString ViewName)
{
	for (auto View : RealViewArray::Get().GetRealViews())
	{
		if (View->GraphViewName == ViewName)
		{
			Node->SourceGraphView = View;
			break ;
		}
	}
	return Node;
}

UGraphNode* UFunctionTools_GraphWeaver::SetNodeOuter_NotManuallyCall(UGraphNode* Target, UObject* Outer)
{
	Target->SelfOuter = Outer;
	Target->ExplicitName = Outer->GetName() + TEXT("_GraphNode");
	return Target;
}


void UFunctionTools_GraphWeaver::NonFunction()
{
	
}

UGraphView* UFunctionTools_GraphWeaver::GetViewAndIndexFromNode(UGraphNode* Target, int32& Index)
{
	Index = Target->IndexInRealNodes;
	return Target->SourceGraphView;
}

TArray<UGraphView*>& RealViewArray::GetRealViews()
{
	return GraphViews;
}

/*
bool UFunctionTools::CanBePlacedInLevel(const UClass* Class)
{
	if (!Class)
		return false;
    
	// 1. 必须是AActor或其子类（只有Actor能放在关卡中）
	if (!Class->IsChildOf(AActor::StaticClass()))
		return false;
    
	// 2. 不能是抽象类（无法实例化）
	if (Class->HasAnyClassFlags(CLASS_Abstract))
		return false;
    
	// 3. 不能明确标记为不可放置
	if (Class->HasAnyClassFlags(CLASS_NotPlaceable))
		return false;
    
	// 4. 关键：满足"拖拽蓝图类型"要求
	//    这样会自动排除纯C++类，即使它们技术上可放置
	if (!Cast<UBlueprintGeneratedClass>(Class))
		return false;
    
	return true;
}
*/
