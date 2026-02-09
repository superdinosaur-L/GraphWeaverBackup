// Copyright 2026 RainButterfly. All Rights Reserved.

#include "GraphView.h"
#include "Engine/Engine.h"
#include "FunctionTools.h"
#include "GraphNode.h"
#include <functional>


TArray<int32> Range(int32 MaxNum)
{
	TArray<int32> Value;
	for (int32 i = 0 ; i < MaxNum ; ++i)
		Value.Emplace(i);
	return Value;
}


void FNamesInputNodeMirror::operator=(const FNamesInputNode& Source)
{
	SelfName = Source.SelfName;
	ParentNodeNames = Source.ParentNodeNames;
	BroNames = Source.BroNames;
}

void FLHCode_G_InputMirror::operator=(const FLHCode_G_Input& Source)
{
	SelfId = Source.SelfId;
	ParentCodes = Source.ParentCodes;
	BroCodes = Source.BrotherCodes;
}

void FNamesConstructConfig::operator=(const FNamesConstructConfig& other)
{
	NamingOfRules = other.NamingOfRules;
	Precision = other.Precision;
}



FGraphViewSimpleMap::FGraphViewSimpleMap()
{
}

FGraphViewSimpleMap::FGraphViewSimpleMap(FString& TargetKey, int32 Index, UGraphView* TargetView)
{
	if (TargetView->NamesConstructConfig.Precision < 1)
		ClanName = TargetKey.Left(1);
	else
		ClanName = TargetKey.Left(TargetView->NamesConstructConfig.Precision);
	Indexs.Emplace(Index);
}

FGraphViewSimpleMap::~FGraphViewSimpleMap() = default;

FGraphViewSimplePair::FGraphViewSimplePair()
{
	IndexInRealNodes = -1;
	Ranking = -1;
}

FGraphViewSimplePair::FGraphViewSimplePair(int32 Index, int32 _Ranking)
{
	IndexInRealNodes = Index;
	Ranking = _Ranking;
}

bool FGraphViewSimplePair::operator==(int32 OtherIndex) const
{
	return IndexInRealNodes == OtherIndex;
}

FGraphNodeDescription::FGraphNodeDescription()
{
	Activated = false;
	IndexInRecorded = -1;
	SourceGraphNode = nullptr;
}

FGraphNodeDescription::FGraphNodeDescription(UGraphNode* Source, UGraphView* TargetView) : FGraphNodeDescription()
{
	ExplicitName = Source->ExplicitName + "_Script";
	SourceGraphNode = Source;
	if (TargetView->ConstructMethod == NAConstructMethod::Names)
		NamesInputNodeMirror = Source->NamesInput;
	if (TargetView->ConstructMethod == NAConstructMethod::LHCode_G)
		LHCode_G_InputMirror = Source->LHCode_G_Input;
}

FGraphNodeDescription::~FGraphNodeDescription() = default;






UGraphView::UGraphView(const FObjectInitializer& ObjectInitializer)
{
	RealNodes.Emplace();
	RealNodes[0].Activated = true;
	RealNodes[0].NamesInputNodeMirror.SelfName = "Root";
	RealNodes[0].ExplicitName = "Root_Script";
	RealNodes[0].IndexInRecorded = 0;
	ConstructMethod = NAConstructMethod::Names;
	ErrorCodeForConstructView = NAGraphConstructErrorCode::None;
	FString RootClanName = "RTest";
	Clans.Emplace(RootClanName, 0, this);
	SelfOwner = nullptr;
}


UGraphView::~UGraphView()
{
}

bool UGraphView::CheckSameNode_Names(UGraphNode* Node)
{
	FString& NewSelfName = Node->NamesInput.SelfName;
	if (NamesConstructConfig.NamingOfRules == true)
	{
		for (auto& Clan : Clans)
		{
			if (Clan.ClanName == NewSelfName.Left(NamesConstructConfig.Precision))
			{
				for (int32 IndexInAll : Clan.Indexs)
				{
					if (RealNodes[IndexInAll].NamesInputNodeMirror.SelfName == NewSelfName)
					{
						RealNodes[IndexInAll].SourceGraphNode = Node;
						Node->IndexInRealNodes = IndexInAll;
						if (WayToDealSameNode == NAWayToDealSameGraphNode::NothingToDo)[[likely]]
							return true;
						if (WayToDealSameNode == NAWayToDealSameGraphNode::OnlyWarningSameNode)
						{
							GEngine->AddOnScreenDebugMessage(-1, 20.0f, FColor::Red,FString::Printf(
							TEXT("You have added two or more 'GraphNode' instances with the same 'SelfName' : %s .GraphViewName : %s"), *NewSelfName, *GraphViewName));
							UE_LOG(LogTemp, Error, TEXT("You have added two or more 'GraphNode' instances with the same 'SelfName' : %s, GraphViewName : %s"), *NewSelfName, *GraphViewName);
							return true;
						}
						WAITING_MOD_LOG();
						return true;
					}
				}
			}
		}
	}

	{
		for (auto& RecordedNode : RealNodes)
		{
			if (NewSelfName == RecordedNode.NamesInputNodeMirror.SelfName)
			{
				RecordedNode.SourceGraphNode = Node;
				Node->IndexInRealNodes = RecordedNode.IndexInRecorded;
				if (WayToDealSameNode == NAWayToDealSameGraphNode::NothingToDo)[[likely]]
							return true;
				if (WayToDealSameNode == NAWayToDealSameGraphNode::OnlyWarningSameNode)
				{
					GEngine->AddOnScreenDebugMessage(-1, 20.0f, FColor::Red,FString::Printf(
					TEXT("You have added two or more 'GraphNode' instances with the same 'SelfName' : %s .GraphViewName : %s"), *NewSelfName, *GraphViewName));
					UE_LOG(LogTemp, Error, TEXT("You have added two or more 'GraphNode' instances with the same 'SelfName' : %s, GraphViewName : %s"), *NewSelfName, *GraphViewName);
					return true;
				}
				WAITING_MOD_LOG();
				return true;
			}
		}
	}
	return false;
}

bool UGraphView::CheckSameNode_LHCode(UGraphNode* Node)
{
	auto f = [this](FString& Path)
	{
		if (WayToDealSameNode == NAWayToDealSameGraphNode::NothingToDo)
			return ;
		if (WayToDealSameNode == NAWayToDealSameGraphNode::OnlyWarningSameNode)
		{
			GEngine->AddOnScreenDebugMessage(-1, 20.0f, FColor::Red,FString::Printf(
	TEXT("You have added two or more 'GraphNode' instances with the same PathId : %s .GraphViewName : %s"), *Path, *GraphViewName));
			UE_LOG(LogTemp, Error, TEXT("You have added two or more PathId instances with the same 'SelfId' : %s, GraphViewName : %s"), *Path, *GraphViewName);
			return ;
		}
		WAITING_MOD_LOG();
	};
	
	for (auto& RecordedNode : RealNodes)
	{
		if (RecordedNode.IndexInRecorded == 0)
			continue ;
		if (RecordedNode.LHCode_G_InputMirror.SelfId == Node->LHCode_G_Input.SelfId)
		{
			TArray<FString> Path = UFunctionTools_GraphWeaver::GetNodePath(RecordedNode);
			/*
			UE_LOG(LogTemp, Error, TEXT("SelfName : %s"), *Node->ExplicitName);
			UE_LOG(LogTemp, Error, TEXT("RecordedNodeName : %s"), *RecordedNode.ExplicitName)
			EMPTY_LOG();
			for (auto& PathNode : Path)
				UE_LOG(LogTemp, Error, TEXT("Path : %s"), *PathNode);
			EMPTY_LOG();
			*/
			if (Node->LHCode_G_Input.ParentCodes.Num() == 0)
			{
				for (auto& P : Path)
				{
					if (P == Node->LHCode_G_Input.SelfId)
					{
						RecordedNode.SourceGraphNode = Node;
						Node->IndexInRealNodes = RecordedNode.IndexInRecorded;
						f(P);
						return true;
					}
				}
			}
			for (auto& prefix : Node->LHCode_G_Input.ParentCodes)
			{
				for (auto& path : Path)
				{
					if (path == (prefix + Node->LHCode_G_Input.SelfId))
					{
						RecordedNode.SourceGraphNode = Node;
						Node->IndexInRealNodes = RecordedNode.IndexInRecorded;
						f(path);
						return true;
					}
				} 
			}
			
		}
	}
	return false;
}

void UGraphView::DealingWithParent_ChildRelationships(int32 ParentIndex, int32 ChildIndex)
{
	int32 NewElementIndex = RealNodes[ParentIndex].Family.Children.Emplace(ChildIndex, 1);
	RealNodes[ChildIndex].Family.Parents.Emplace(ParentIndex, RealNodes[ParentIndex].Family.Children.Num() - 1);
	RealNodes[ParentIndex].Family.Children[NewElementIndex].Ranking = RealNodes[ChildIndex].Family.Parents.Num() - 1;
}

void UGraphView::DealingWithBrothersRelationships(int32 BroIndex, int32 SelfIndex)
{
	int32 NewElementIndex = RealNodes[BroIndex].Family.Brothers.Emplace(SelfIndex, 1);
	RealNodes[SelfIndex].Family.Brothers.Emplace(BroIndex, RealNodes[BroIndex].Family.Brothers.Num() - 1);
	RealNodes[BroIndex].Family.Brothers[NewElementIndex].Ranking = RealNodes[SelfIndex].Family.Brothers.Num() - 1;
}


void UGraphView::NamesConstructWay(UGraphNode* TargetNode)
{
	FString TargetSelfName = RealNodes[TargetNode->IndexInRealNodes].NamesInputNodeMirror.SelfName;
	
	if (TargetSelfName.Len() == 0)
	{
		ErrorCodeForConstructView = NAGraphConstructErrorCode::NameSameAsRoot;
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, FString::Printf(
			TEXT("SelfName cannot be empty or Same As Root. ExplicitName: %s  , SelfName: %s"), *TargetNode->ExplicitName, *TargetNode->NamesInput.SelfName));
		UE_LOG(LogTemp, Error, TEXT("SelfName cannot be empty or Same As Root. ExplicitName: %s , SelfName: %s"), *TargetNode->ExplicitName,  *TargetNode->NamesInput.SelfName);
		return ;
	}

	if (TargetNode->NamesInput.ParentNodeNames.Num() == 0)
	{
		DealingWithParent_ChildRelationships(0, TargetNode->IndexInRealNodes);
	}
	
	//有添加父节点数组但是为空
	for (auto& Handle : TargetNode->NamesInput.ParentNodeNames)
	{
		if (Handle.Len() == 0)
		{
			DealingWithParent_ChildRelationships(0, TargetNode->IndexInRealNodes);
			RealNodes[TargetNode->IndexInRealNodes].NamesInputNodeMirror.ParentNodeNames.Remove(Handle);
			break ;
		}
	}
	//处理完父节点为Root的特殊情况

	//不允许兄弟节点里面出现Brother包含Root的情况.不希望出现乱伦理的事情
	for (auto& BroName : TargetNode->NamesInput.BroNames)
	{
		if (BroName.Len() == 0)
		{
			ErrorCodeForConstructView = NAGraphConstructErrorCode::BroNameSameAsRoot;
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, FString::Printf(
				TEXT("Brother nodes cannot contain a Root node.ExplicitName: %s , SelfName: %s"), *TargetNode->ExplicitName, *TargetNode->NamesInput.SelfName));
			UE_LOG(LogTemp, Error, TEXT("Brother nodes cannot contain a Root node.ExplicitName: %s , SelfName: %s"), *TargetNode->ExplicitName, *TargetNode->NamesInput.SelfName);
			return ;
		}
	}
	
	//有规则的命名,取代在AlreadyRecorded里面进行暴力搜索,提升速度.只能搜索到已经被记录的节点
	if (NamesConstructConfig.NamingOfRules)
	{
		bool FindSelfClan = false;

		for (int32 p_i = TargetNode->NamesInput.ParentNodeNames.Num() - 1; p_i >= 0; --p_i)
		{
			uint8 Find = 0;
			for (auto& Clan : Clans)
			{
				if (TargetNode->NamesInput.ParentNodeNames[p_i].Left(NamesConstructConfig.Precision) == Clan.ClanName)
				{
					for (int32 IndexInAll : Clan.Indexs)
					{
						if (RealNodes[IndexInAll].NamesInputNodeMirror.SelfName == TargetNode->NamesInput.ParentNodeNames[p_i])
						{
							DealingWithParent_ChildRelationships(IndexInAll, TargetNode->IndexInRealNodes);
							RealNodes[TargetNode->IndexInRealNodes].NamesInputNodeMirror.ParentNodeNames.RemoveAt(p_i);
							Find = 1;
							break ;
						}
					}
				}
				if (Find == 1)
					break ;
			}
		}

		for (int32 b_i = TargetNode->NamesInput.BroNames.Num() - 1; b_i >= 0; --b_i)
		{
			uint8 Find = 0;
			for (auto& Clan : Clans)
			{
				if (TargetNode->NamesInput.BroNames[b_i].Left(NamesConstructConfig.Precision) == Clan.ClanName)
				{
					for (int32 IndexInAll : Clan.Indexs)
					{
						if (RealNodes[IndexInAll].NamesInputNodeMirror.SelfName == TargetNode->NamesInput.BroNames[b_i])
						{
							DealingWithBrothersRelationships(IndexInAll, TargetNode->IndexInRealNodes);
							RealNodes[TargetNode->IndexInRealNodes].NamesInputNodeMirror.BroNames.RemoveAt(b_i);
							Find = 1;
							break;
						}
					}
					if (Find == 1)
						break ;
				}
			}
		}

		for (auto& Clan : Clans)
		{
			if (Clan.ClanName == TargetSelfName.Left(NamesConstructConfig.Precision))
			{
				FindSelfClan = true;
				Clan.Indexs.Emplace(TargetNode->IndexInRealNodes);
				break ;
			}
		}
		
		TArray<int32> NeedRemoveNum;//AlreadyRecorded里面的下标
		//处理连接情况的第一种情况，寻找自己的可能子节点。此时TargetNode扮演的角色是A2
		for (int32 IndirectChildIndex = 0 ; IndirectChildIndex < WillVerticalAwakeNode.Num() ; ++IndirectChildIndex)
		{
			for (FString& ParentName : RealNodes[WillVerticalAwakeNode[IndirectChildIndex]].NamesInputNodeMirror.ParentNodeNames)
			{
				if (ParentName == TargetSelfName)
				{
					DealingWithParent_ChildRelationships(TargetNode->IndexInRealNodes, WillVerticalAwakeNode[IndirectChildIndex]);
					RealNodes[WillVerticalAwakeNode[IndirectChildIndex]].NamesInputNodeMirror.ParentNodeNames.Remove(TargetSelfName);
					if (RealNodes[WillVerticalAwakeNode[IndirectChildIndex]].NamesInputNodeMirror.ParentNodeNames.Num() == 0)
						NeedRemoveNum.Emplace(WillVerticalAwakeNode[IndirectChildIndex]);
					break ;
				}
			}
		}
		for (int32 EveNum : NeedRemoveNum)
		{
			WillVerticalAwakeNode.Remove(EveNum);
		}

		NeedRemoveNum.Empty();
		//处理连接的第一种情况，A1指向兄弟A2。寻找潜在的兄弟节点
		for (int32 IndirectBroIndex = 0 ; IndirectBroIndex < WillHorizontalAwakeNode.Num() ; ++IndirectBroIndex)
		{
			for (FString& BroName : RealNodes[WillHorizontalAwakeNode[IndirectBroIndex]].NamesInputNodeMirror.BroNames)
			{
				if (BroName == TargetSelfName)
				{
					DealingWithBrothersRelationships(TargetNode->IndexInRealNodes, WillHorizontalAwakeNode[IndirectBroIndex]);
					RealNodes[WillHorizontalAwakeNode[IndirectBroIndex]].NamesInputNodeMirror.BroNames.Remove(TargetSelfName);
					if (RealNodes[WillHorizontalAwakeNode[IndirectBroIndex]].NamesInputNodeMirror.BroNames.Num() == 0)
						NeedRemoveNum.Emplace(WillHorizontalAwakeNode[IndirectBroIndex]);
					break ;
				}
			}
		}
		for (int32 EveNum : NeedRemoveNum)
		{
			WillHorizontalAwakeNode.Remove(EveNum);
		}
		
		//自己的父节点没有完全找完，需要加入到纵向等待序列
		if (RealNodes[TargetNode->IndexInRealNodes].NamesInputNodeMirror.ParentNodeNames.Num() != 0)
			WillVerticalAwakeNode.Emplace(TargetNode->IndexInRealNodes);

		//自己的兄弟节点还没有找完，需要加入横向等待队列
		if (RealNodes[TargetNode->IndexInRealNodes].NamesInputNodeMirror.BroNames.Num() != 0)
			WillHorizontalAwakeNode.Emplace(TargetNode->IndexInRealNodes);

		
		if (!FindSelfClan)
			Clans.Emplace(RealNodes[TargetNode->IndexInRealNodes].NamesInputNodeMirror.SelfName, TargetNode->IndexInRealNodes, this);
		
		return ;
	}//if (NamesConstructConfig.NamingOfRules)

	//不开启NamingOfRules，但是运行速度可能会降低
	//直接进行暴力寻找，因为缺失信息太多，貌似只能暴力查找
	for (auto& EveDes : RealNodes)
	{
		for (auto& EveParentName : TargetNode->NamesInput.ParentNodeNames)
		{
			if (EveDes.NamesInputNodeMirror.SelfName == EveParentName)
			{
				DealingWithParent_ChildRelationships(EveDes.IndexInRecorded, TargetNode->IndexInRealNodes);
				RealNodes[TargetNode->IndexInRealNodes].NamesInputNodeMirror.ParentNodeNames.Remove(EveParentName);
				break ;
			}
		}
	}
	for (auto& EveDes : RealNodes)
	{
		for (auto& EveBroName : TargetNode->NamesInput.BroNames)
		{
			if (EveDes.NamesInputNodeMirror.SelfName == EveBroName)
			{
				DealingWithBrothersRelationships(EveDes.IndexInRecorded, TargetNode->IndexInRealNodes);
				RealNodes[TargetNode->IndexInRealNodes].NamesInputNodeMirror.BroNames.Remove(EveBroName);
				break ;
			}
		}
	}
	//已记录的Parent和Bro已经寻找完了，开始寻找等待的Parent和Bro

	
	TArray<int32> NeedRemoveNum;//AlreadyRecorded里面的下标
	//处理连接情况的第一种情况，寻找自己的可能子节点。此时TargetNode扮演的角色是A2
	for (int32 IndirectChildIndex = 0 ; IndirectChildIndex < WillVerticalAwakeNode.Num() ; ++IndirectChildIndex)
	{
		for (FString& ParentName : RealNodes[WillVerticalAwakeNode[IndirectChildIndex]].NamesInputNodeMirror.ParentNodeNames)
		{
			if (ParentName == TargetSelfName)
			{
				DealingWithParent_ChildRelationships(TargetNode->IndexInRealNodes, WillVerticalAwakeNode[IndirectChildIndex]);
				RealNodes[WillVerticalAwakeNode[IndirectChildIndex]].NamesInputNodeMirror.ParentNodeNames.Remove(TargetSelfName);
				if (RealNodes[WillVerticalAwakeNode[IndirectChildIndex]].NamesInputNodeMirror.ParentNodeNames.Num() == 0)
					NeedRemoveNum.Emplace(WillVerticalAwakeNode[IndirectChildIndex]);
				break ;
			}
		}
	}
	for (int32 EveNum : NeedRemoveNum)
	{
		WillVerticalAwakeNode.Remove(EveNum);
	}

	NeedRemoveNum.Empty();
	//处理连接的第一种情况，A1指向兄弟A2。寻找潜在的兄弟节点
	for (int32 IndirectBroIndex = 0 ; IndirectBroIndex < WillHorizontalAwakeNode.Num() ; ++IndirectBroIndex)
	{
		for (FString& BroName : RealNodes[WillHorizontalAwakeNode[IndirectBroIndex]].NamesInputNodeMirror.BroNames)
		{
			if (BroName == TargetSelfName)
			{
				DealingWithBrothersRelationships(TargetNode->IndexInRealNodes, WillHorizontalAwakeNode[IndirectBroIndex]);
				RealNodes[WillHorizontalAwakeNode[IndirectBroIndex]].NamesInputNodeMirror.BroNames.Remove(TargetSelfName);
				if (RealNodes[WillHorizontalAwakeNode[IndirectBroIndex]].NamesInputNodeMirror.BroNames.Num() == 0)
					NeedRemoveNum.Emplace(WillHorizontalAwakeNode[IndirectBroIndex]);
				break ;
			}
		}
	}
	for (int32 EveNum : NeedRemoveNum)
	{
		WillHorizontalAwakeNode.Remove(EveNum);
	}

	//自己的父节点没有完全找完，需要加入到纵向等待序列
	if (RealNodes[TargetNode->IndexInRealNodes].NamesInputNodeMirror.ParentNodeNames.Num() != 0)
		WillVerticalAwakeNode.Emplace(TargetNode->IndexInRealNodes);

	//自己的兄弟节点还没有找完，需要加入横向等待队列
	if (RealNodes[TargetNode->IndexInRealNodes].NamesInputNodeMirror.BroNames.Num() != 0)
		WillHorizontalAwakeNode.Emplace(TargetNode->IndexInRealNodes);
}

std::tuple<int32, int32> UGraphView::TryGuidByLHCode(FString& Code)
{
	//当前成功导航到的最深深度(Code的下标).例如传入15326,最终导航到3,在继续向下导航2的时候没有找到，那么IndexDeeplyArrived的值就是2(3的下标)
	int32 IndexDeeplyArrived = -1;
	int32 IndexArrivedInAlready = 0;
	//当前尝试匹配的节点.不使用引用而是拷贝，是防止后续修改TryNode的时候实际上在反复修改AlreadyRecorded[0]而不是真正修改当前执行到的节点
	auto TryNode = RealNodes[0];
	uint8 Matched = 1;

	auto f = [&]()
	{
		if (IndexDeeplyArrived == Code.Len() - 1)
			return false;//导航成功，停止导航
		
		for (auto& Child : TryNode.Family.Children)
		{
			//ChildDes必定有效，因为无效的Child在本质上就不会被记录
			auto& ChildDes = RealNodes[Child.IndexInRealNodes];
			int32 BeforehandIndex = IndexDeeplyArrived + 1;
			Matched = 1;

			//SelfId为空的特殊情况由函数LHCode_G_ConstructWay处理,会直接退出构建,无法执行该函数
			for (auto Letter : ChildDes.LHCode_G_InputMirror.SelfId)
			{
				if (BeforehandIndex < Code.Len() && Letter == Code[BeforehandIndex])
				{
					BeforehandIndex++;
					continue ;
				}
				Matched = 0;
				break ;
			}
			if (Matched == 0)
				continue ;

			//当前孩子匹配成功
			{
				IndexDeeplyArrived = BeforehandIndex - 1;
				IndexArrivedInAlready = ChildDes.IndexInRecorded;
				TryNode = ChildDes;
				return true;
			}
		}
		return false;
	};

	while (f())
	{
		;
	}
	std::tuple<int32, int32> rr(IndexDeeplyArrived, IndexArrivedInAlready);
	return rr;
}

void UGraphView::LHCode_G_ConstructWay(UGraphNode* TargetNode)
{
	//后续构建前提:1,每个节点的ParentCodes和BroCodes里面不含有相同的元素.2,AlreadyRecorded[0]总是存在且代表根节点
	
	auto& Describe = RealNodes[TargetNode->IndexInRealNodes];
	
	auto& SelfId = Describe.LHCode_G_InputMirror.SelfId;

	//如果SelfId为空或者和Root名字一样那么就直接退出构建，不再执行后续内容
	if (SelfId.Len() == 0)
	{
		ErrorCodeForConstructView = NAGraphConstructErrorCode::NameSameAsRoot;
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, FString::Printf(
			TEXT("SelfName cannot be empty or Same As Root. ExplicitName: %s  , SelfName: %s"), *TargetNode->ExplicitName, *TargetNode->NamesInput.SelfName));
		UE_LOG(LogTemp, Error, TEXT("SelfName cannot be empty or Same As Root. ExplicitName: %s , SelfName: %s"), *TargetNode->ExplicitName,  *TargetNode->NamesInput.SelfName);
		return ;
	}

	TArray<FString> SelfWholeId;
	
	//如果ParentCodes为空,那么说明AlreadyRecorded[0]这个Root节点作为它的唯一父节点
	if (Describe.LHCode_G_InputMirror.ParentCodes.Num() == 0)
	{
		SelfWholeId.Emplace(SelfId);
		DealingWithParent_ChildRelationships(0, TargetNode->IndexInRealNodes);
	}
	
	for (auto& Prefix : Describe.LHCode_G_InputMirror.ParentCodes)
		SelfWholeId.Emplace(Prefix + SelfId);
	
	for (int32 i = 0 ; i < Describe.LHCode_G_InputMirror.ParentCodes.Num() ; ++i)
	{
		auto& ParentId = Describe.LHCode_G_InputMirror.ParentCodes[i];
		if (ParentId.Len() == 0)
		{
			DealingWithParent_ChildRelationships(0, TargetNode->IndexInRealNodes);
			Describe.LHCode_G_InputMirror.ParentCodes.RemoveAt(i);
			break ;
		}
	}
	//上面处理完父节点为Root的情况
	
	//如果检测到有BroCodes里面的某个字符串为空或者为根节点的情况,直接退出构建
	for (int32 i = 0 ; i < Describe.LHCode_G_InputMirror.BroCodes.Num() ; ++i)
	{
		auto& BroId = Describe.LHCode_G_InputMirror.BroCodes[i];
		if (BroId.Len() == 0)
		{
			ErrorCodeForConstructView = NAGraphConstructErrorCode::BroNameSameAsRoot;
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, FString::Printf(
				TEXT("Brother nodes cannot contain a Root node.ExplicitName: %s , SelfName: %s"), *TargetNode->ExplicitName, *TargetNode->NamesInput.SelfName));
			UE_LOG(LogTemp, Error, TEXT("Brother nodes cannot contain a Root node.ExplicitName: %s , SelfName: %s"), *TargetNode->ExplicitName, *TargetNode->NamesInput.SelfName);
			return ;
		}
	}
	
	//开始寻找子节点和兄弟节点
	{
		//寻找子节点
		uint8 Matched = 0;
		for (int32 i = WillVerticalAwakeNode.Num() - 1; i >= 0; --i)
		{
			Matched = 0;
			auto& MaybeChildDes = RealNodes[WillVerticalAwakeNode[i]];
			for (int32 ParentIdIndex = MaybeChildDes.LHCode_G_InputMirror.ParentCodes.Num() - 1 ; ParentIdIndex >= 0 ; --ParentIdIndex)
			{
				for (auto& EveSelfWholeId : SelfWholeId)
				{
					if (EveSelfWholeId == MaybeChildDes.LHCode_G_InputMirror.ParentCodes[ParentIdIndex])
					{
						//UE_LOG(LogTemp, Error, TEXT("FindChild,ChildWholeId: %s"), *(EveSelfWholeId + MaybeChildDes.LHCode_G_InputMirror.SelfId))
						DealingWithParent_ChildRelationships(Describe.IndexInRecorded, MaybeChildDes.IndexInRecorded);
						MaybeChildDes.LHCode_G_InputMirror.ParentCodes.RemoveAt(ParentIdIndex);

						//处理因为路径缺失而导致找不到兄弟节点的情况
						{
							if (WillHorizontalAwakeNode.Num() > 0)
							{
								TArray<FString> NowWholeChildId;
								for (auto& SelfWhole : SelfWholeId)
									NowWholeChildId.Emplace(SelfWhole + MaybeChildDes.LHCode_G_InputMirror.SelfId);
								
								for (int32 j = WillHorizontalAwakeNode.Num() - 1; j >= 0; --j)
								{
									uint8 Find = 0;
									auto& LoseBroLinkedNode = RealNodes[WillHorizontalAwakeNode[j]];
									for (auto& BroName : LoseBroLinkedNode.LHCode_G_InputMirror.BroCodes)
									{
										for (auto& EveChildWholeId : NowWholeChildId)
										{
											if (EveChildWholeId == BroName)
											{
												Find = 1;
												DealingWithBrothersRelationships(MaybeChildDes.IndexInRecorded, LoseBroLinkedNode.IndexInRecorded);
												LoseBroLinkedNode.LHCode_G_InputMirror.BroCodes.Remove(EveChildWholeId);
												if (LoseBroLinkedNode.LHCode_G_InputMirror.BroCodes.Num() == 0)
													WillHorizontalAwakeNode.RemoveAt(j);
												break ;
											}
										}
										if (Find == 1)
											break ;
									}
								}
							}
						}
						
						Matched = 1;
						if (MaybeChildDes.LHCode_G_InputMirror.ParentCodes.Num() == 0)
							WillVerticalAwakeNode.RemoveAt(i);
						break ;
					}
				}
				if (Matched == 1)
					break ;
			}
		}

		//寻找兄弟节点(寻找指向端 节点)
		for (int32 i = WillHorizontalAwakeNode.Num() - 1; i >= 0; --i)
		{
			Matched = 0;
			auto& MaybeBroDes = RealNodes[WillHorizontalAwakeNode[i]];
			for (int32 BroIdIndex = MaybeBroDes.LHCode_G_InputMirror.BroCodes.Num() - 1 ; BroIdIndex >= 0 ; --BroIdIndex)
			{
				for (auto& EveSelfWholeId : SelfWholeId)
				{
					if (EveSelfWholeId == MaybeBroDes.LHCode_G_InputMirror.BroCodes[BroIdIndex])
					{
						//UE_LOG(LogTemp, Error, TEXT("FindBro"))
						DealingWithBrothersRelationships(Describe.IndexInRecorded, MaybeBroDes.IndexInRecorded);
						MaybeBroDes.LHCode_G_InputMirror.BroCodes.RemoveAt(BroIdIndex);
						Matched = 1;
						if (MaybeBroDes.LHCode_G_InputMirror.BroCodes.Num() == 0)
							WillHorizontalAwakeNode.RemoveAt(i);
						break ;
					}
				}
				if (Matched == 1)
					break ;
			}
		}
	}
	

	//开始连接父亲节点(能够从根节点顺利追踪路径下来(已连接))
	for (int32 i = Describe.LHCode_G_InputMirror.ParentCodes.Num() - 1 ; i >= 0 ; --i)
	{
		//在上面的代码里面已经特意清除了ParentId长度为0的空字符串
		auto& ParentId = Describe.LHCode_G_InputMirror.ParentCodes[i];
		auto[IndexDeeplyArrived, IndexArrivedInAlready] = TryGuidByLHCode(ParentId);
		//UE_LOG(LogTemp, Error, TEXT("ParentId: %s,:: IndexDeeplyArrived: %d, IndexArrivedInAlready: %d"), *ParentId ,IndexDeeplyArrived, IndexArrivedInAlready)
		if (IndexDeeplyArrived == ParentId.Len() - 1)//导航成功
		{
			DealingWithParent_ChildRelationships(IndexArrivedInAlready, Describe.IndexInRecorded);
			Describe.LHCode_G_InputMirror.ParentCodes.RemoveAt(i);
		}
	}

	//寻找未连接父节点(不能从根节点顺利跟踪下来)
	for (int32 i = WillVerticalAwakeNode.Num() - 1; i >= 0; --i)
	{
		auto& MaybeParentDes = RealNodes[WillVerticalAwakeNode[i]];
		for (auto& ParentParentId : MaybeParentDes.LHCode_G_InputMirror.ParentCodes)
		{
			FString WholeParentId = ParentParentId + MaybeParentDes.LHCode_G_InputMirror.SelfId;
			for (int32 j = Describe.LHCode_G_InputMirror.ParentCodes.Num() - 1; j >= 0; --j)
			{
				if (Describe.LHCode_G_InputMirror.ParentCodes[j] == WholeParentId)
				{
					DealingWithParent_ChildRelationships(MaybeParentDes.IndexInRecorded, Describe.IndexInRecorded);
					Describe.LHCode_G_InputMirror.ParentCodes.RemoveAt(j);
					break ;
				}
			}
		}
	}
	
	if (Describe.LHCode_G_InputMirror.ParentCodes.Num() != 0)
		WillVerticalAwakeNode.Emplace(Describe.IndexInRecorded);
	

	//开始寻找兄弟节点(可以顺利从根节点追踪下来(连接成功)), 寻找被指向端 节点 
	for (int32 i = Describe.LHCode_G_InputMirror.BroCodes.Num() - 1 ; i >= 0 ; --i)
	{
		auto[IndexDeeplyArrived, IndexArrivedInAlready] = TryGuidByLHCode(Describe.LHCode_G_InputMirror.BroCodes[i]);
		if (IndexDeeplyArrived == Describe.LHCode_G_InputMirror.BroCodes[i].Len() - 1)
		{
			//UE_LOG(LogTemp, Error, TEXT("BroId: %s,:: IndexDeeplyArrived: %d, IndexArrivedInAlready: %d"), *Describe.LHCode_G_InputMirror.BroCodes[i] ,IndexDeeplyArrived, IndexArrivedInAlready)
			DealingWithBrothersRelationships(IndexArrivedInAlready, Describe.IndexInRecorded);
			Describe.LHCode_G_InputMirror.BroCodes.RemoveAt(i);
		}
	}
	
	if (Describe.LHCode_G_InputMirror.BroCodes.Num() != 0)
		WillHorizontalAwakeNode.Emplace(Describe.IndexInRecorded);
}

void UGraphView::AllocateGraphViewSize(int32 Size)
{
	if (RealNodes.Num() == 1)
		RealNodes.Reserve(Size + 1);
}

bool UGraphView::AddNewNodeIntelligent(UGraphNode* NewNode)
{
	if (ErrorCodeForConstructView == NAGraphConstructErrorCode::None)[[likely]]
	{
		if (ConstructMethod == NAConstructMethod::Names)
			if (CheckSameNode_Names(NewNode))
				return true;
		
		if (ConstructMethod == NAConstructMethod::LHCode_G)
			if (CheckSameNode_LHCode(NewNode))
				return true;
				
		int32 IndexInRecorded = RealNodes.Emplace(NewNode, this);
		RealNodes[IndexInRecorded].IndexInRecorded = IndexInRecorded;
		NewNode->IndexInRealNodes = IndexInRecorded;
	
		//采用Names名字的方式进行连接
		if (ConstructMethod == NAConstructMethod::Names)
		{
			NamesConstructWay(NewNode);
			return true;
		}
		if (ConstructMethod == NAConstructMethod::LHCode_G)
		{
			LHCode_G_ConstructWay(NewNode);
			return true;
		}
		WAITING_MOD_LOG();
		return false;
	}
	
	switch (ErrorCodeForConstructView)
	{
	case NAGraphConstructErrorCode::NameSameAsRoot:
		{
			
		}
		break ;
	case NAGraphConstructErrorCode::BroNameSameAsRoot:
		{
			
		}
		break ;
	default:
		WAITING_MOD_LOG();
	}
	return false;
}

void UGraphView::SetWayToDealSameNode(TEnumAsByte<NAWayToDealSameGraphNode::EWayToDealSameGraphNode> Way)
{
	WayToDealSameNode = Way;
}


















