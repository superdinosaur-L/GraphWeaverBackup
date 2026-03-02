// Copyright 2026 RainButterfly. All Rights Reserved.

#include "FunctionTools.h"
#include "GraphNode.h"
#include "GraphView.h"
#include "Engine/Engine.h"
#include "UObject/UObjectGlobals.h"
#include <functional>
#include "Algo/Sort.h"
#include "Misc/MemStack.h"

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

TArray<FString> UFunctionTools_GraphWeaver::GetNodePath(UGraphView* SourceGraphView, FGraphNodeDescription& SourceDesc)
{
	TArray<FString> rr;
	UGraphView& SourceView = *SourceGraphView;

	if (!SourceGraphView->ValidateRankingConsistencyLight(TArray<int32>{}))
		FixupRanking(SourceGraphView);
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

	FMemMark Mar(FMemStack::Get());
	FMemStack& Stack = FMemStack::Get();

	uint8* RecordedHash = (uint8*)Stack.Alloc(SourceGraphView->RealNodes.Num() * sizeof(uint8), alignof(uint8));
	FMemory::Memzero(RecordedHash, SourceGraphView->RealNodes.Num() * sizeof(uint8));
	
	std::function<void()> f = [&]()
	{
		auto& NowDes = SourceGraphView->RealNodes[ForkIndex[ForkArrived]];
		for (auto& Child : NowDes.Family.Children)
		{
			if (RecordedHash[Child.IndexInRealNodes] == 1)
				continue ;
			RecordedHash[Child.IndexInRealNodes] = 1;
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

	FMemMark Mar(FMemStack::Get());
	FMemStack& Stack = FMemStack::Get();

	uint8* RecordedHash = (uint8*)Stack.Alloc(SourceGraphView->RealNodes.Num() * sizeof(uint8), alignof(uint8));
	FMemory::Memzero(RecordedHash, SourceGraphView->RealNodes.Num() * sizeof(uint8));
	
	auto f = [&]()
	{
		auto& NowDes = SourceGraphView->RealNodes[ForkIndex[IndexArrived]];
		for (auto& Child : NowDes.Family.Children)
		{
			if (RecordedHash[Child.IndexInRealNodes] == 1)
				continue ;
			ForkIndex.Emplace(Child.IndexInRealNodes);
			rr.Emplace(Child.IndexInRealNodes);
			RecordedHash[Child.IndexInRealNodes] = 1;
		}
	};

	while (IndexArrived < ForkIndex.Num())
	{
		f();
		IndexArrived++;
	}
	return rr;
}

void UFunctionTools_GraphWeaver::FixupRanking(UGraphView* DisorderedView)
{
	auto& RealNodes = DisorderedView->RealNodes;
	
	for (auto& Des : RealNodes)
	{
		//Parent
		{
			for (int32 i = 0 ; i < Des.Family.Parents.Num(); i++)
			{
				for (auto& Child : RealNodes[Des.Family.Parents[i].IndexInRealNodes].Family.Children)
				{
					if (Child.IndexInRealNodes == Des.IndexInRecorded)
					{
						Child.Ranking = i;
						break ;
					}
				}
			}
		}
		//Bro
		{
			for (int32 i = 0 ; i < Des.Family.Brothers.Num(); i++)
			{
				for (auto& Bro : RealNodes[Des.Family.Brothers[i].IndexInRealNodes].Family.Brothers)
				{
					if (Bro.IndexInRealNodes == Des.IndexInRecorded)
					{
						Bro.Ranking = i;
						break ;
					}
				}
			}
		}
		//Child
		{
			for (int32 i = 0 ; i < Des.Family.Children.Num(); i++)
			{
				for (auto& Parent : RealNodes[Des.Family.Children[i].IndexInRealNodes].Family.Parents)
				{
					if (Parent.IndexInRealNodes == Des.IndexInRecorded)
					{
						Parent.Ranking = i;
						break ;
					}
				}
			}
		}
	}
}

void UFunctionTools_GraphWeaver::RemoveNodes(UGraphView* SourceGraphView, FGraphNodeDescription& Des, bool RemoveChildren, bool ReorderRanking)
{
	auto& RealNodes = SourceGraphView->RealNodes;
	if (RemoveChildren)[[likely]]
	{
		TArray<int32> IndexToRemove;
		IndexToRemove.Emplace(Des.IndexInRecorded);
		TArray<int32> ChildIndex = ObtainAllChildDes(SourceGraphView, Des);
		for (int32 Child : ChildIndex)
			IndexToRemove.Emplace(Child);
		
		Algo::Sort(IndexToRemove, [](int32 A, int32 B) {
			return A < B;  // 升序, 小数字在前
		});
		//上面收集信息
		//假设收集到了IndexToRemove = {1, 3, 5, 6, 9}  (0作为根不能被删除)
	
		int32 NodesNum = SourceGraphView->RealNodes.Num() + 1; //NodesNum = 10  多一个1是为了作为dp数组的启动元素
#if 1
		//创建标记点
		FMemMark Mark(FMemStack::Get());
		// === 使用 FMemStack 分配临时内存 ===
		FMemStack& MemStack = FMemStack::Get();

		// DeleteHash[i] = 1 表示节点 i 被删除
		int32* DeleteHash = (int32*)MemStack.Alloc((NodesNum - 1) * sizeof(int32), alignof(int32));
		FMemory::Memzero(DeleteHash, (NodesNum - 1) * sizeof(int32));

		//数组用于哈希 ->  DeleteNum[IndexToRemove[IndexArrived]] 为前面被删除的NodeDes的元素个数,用于约定变化
		int32* DeleteNum = (int32*)MemStack.Alloc(NodesNum * sizeof(int32), alignof(int32));
		DeleteNum[NodesNum - 1] = IndexToRemove.Num();
#endif

#if 0
		//下面的数字和数组只是我开发的时候调试使用的,正式发布应该使用上面的版本
		int32 DeleteHash[17] = {};
		int32 DeleteNum[18];
		DeleteNum[NodesNum - 1] = IndexToRemove.Num();
#endif
		int32 IndexArrived = IndexToRemove.Num() - 1;
	
		for (int32 Num = NodesNum - 2; Num >= 0; --Num)
		{
			if (IndexArrived == -1)
			{
				DeleteNum[Num] = 0;
				continue ;
			}
			DeleteNum[Num] = DeleteNum[Num + 1];//向后复制,可以成功利用最后一个方块
			
			if (Num == IndexToRemove[IndexArrived])
			{
				DeleteHash[Num] = 1;
				DeleteNum[Num]--;
				IndexArrived--;
				RealNodes[Num].SourceGraphNode->IndexInRealNodes = -1;
				SourceGraphView->AddRemovedNodeName(RealNodes[Num]);
				if (!ReorderRanking)//交给分支预测器，应该损失不了什么速度
					RealNodes.RemoveAt(Num);
			}
		}
		//上面完成信息收集.第一次for循环不真正删除元素,方便下面第二个for循环正确找到Family指向的节点

		if (ReorderRanking)
		{
			if (!SourceGraphView->ValidateRankingConsistencyLight(TArray<int32>{}))
				FixupRanking(SourceGraphView);
			for (int32 Num = NodesNum - 2; Num >= 0; --Num)
			{
				if (DeleteHash[Num] == 1)
					continue ;
				
				{
					int32 RemovedBefore = 0;
					for (auto& Pair : RealNodes[Num].Family.Children)
					{
						if (DeleteHash[Pair.IndexInRealNodes] == 1)
						{
							RemovedBefore++;
							continue ;
						}
						auto& Node = RealNodes[Pair.IndexInRealNodes];
						Node.Family.Parents[Pair.Ranking].Ranking -= RemovedBefore;
					}
				}
				{
					int32 RemovedBefore = 0;
					for (auto& Pair : RealNodes[Num].Family.Brothers)
					{
						if (DeleteHash[Pair.IndexInRealNodes] == 1)
						{
							RemovedBefore++;
							continue ;
						}
						auto& Node = RealNodes[Pair.IndexInRealNodes];
						Node.Family.Brothers[Pair.Ranking].Ranking -= RemovedBefore;
					}
				}
			}
			for (int32 Num = NodesNum - 2; Num >= 0; --Num)
			{
				if (DeleteHash[Num] == 1)
					RealNodes.RemoveAt(Num);
			}
		}//if(RecordedRanking)
		

		auto f = [&](TArray<FGraphViewSimplePair>& Relationship)
		{
			for (int32 i = Relationship.Num() - 1; i >= 0; i--)
			{
				if (DeleteHash[Relationship[i].IndexInRealNodes] == 1)
				{
					Relationship.RemoveAt(i);
					continue ;
				}
				Relationship[i].IndexInRealNodes = Relationship[i].IndexInRealNodes - DeleteNum[Relationship[i].IndexInRealNodes];
			}
		};
		
		for (auto& NewDes : RealNodes)
		{
			NewDes.IndexInRecorded = NewDes.IndexInRecorded - DeleteNum[NewDes.IndexInRecorded];
			if (NewDes.SourceGraphNode != nullptr)
				NewDes.SourceGraphNode->IndexInRealNodes = NewDes.IndexInRecorded;
			f(NewDes.Family.Children);
			f(NewDes.Family.Parents);
			f(NewDes.Family.Brothers);
		}
		//修复SourceGraphView::Clans::Indexs的错误指向
		for (auto& Clan : SourceGraphView->Clans)
		{
			for (int32 i = Clan.Indexs.Num() - 1; i >= 0; i--)
			{
				if (DeleteHash[Clan.Indexs[i]] == 1)
				{
					Clan.Indexs.RemoveAt(i);
					continue ;
				}
				Clan.Indexs[i] -= DeleteNum[Clan.Indexs[i]];
			}
		}
		return ;
	}//if(RemoveChildren)

	//if(!RemoveChildren)
	{
		//强制开启RecordedRanking,否则下一次调用该函数且RemoveChild为false的时候该函数会错误运行甚至报错
		//if(RecordedRanking)
		if (!SourceGraphView->ValidateRankingConsistencyLight(TArray<int32>{}))
			FixupRanking(SourceGraphView);
		{
			for (auto& Parent : Des.Family.Parents)
			{
				if (Parent.Ranking == (RealNodes[Parent.IndexInRealNodes].Family.Children.Num() - 1))
				{
					RealNodes[Parent.IndexInRealNodes].Family.Children.RemoveAt(Parent.Ranking);
					continue ;
				}
				for (int32 i = Parent.Ranking + 1 ; i < RealNodes[Parent.IndexInRealNodes].Family.Children.Num() ; i++)
				{
					RealNodes[RealNodes[Parent.IndexInRealNodes].Family.Children[i].IndexInRealNodes].Family.Parents[RealNodes[Parent.IndexInRealNodes].Family.Children[i].Ranking].Ranking--;
				}
				RealNodes[Parent.IndexInRealNodes].Family.Children.RemoveAt(Parent.Ranking);
			}
			for (auto& Child : Des.Family.Children)
			{
				if (Child.Ranking == RealNodes[Child.IndexInRealNodes].Family.Parents.Num() - 1)
				{
					RealNodes[Child.IndexInRealNodes].Family.Parents.RemoveAt(Child.Ranking);
					continue ;
				}
				for (int32 i = Child.Ranking + 1 ; i < RealNodes[Child.IndexInRealNodes].Family.Parents.Num() ; i++)
				{
					RealNodes[RealNodes[Child.IndexInRealNodes].Family.Parents[i].IndexInRealNodes].Family.Children[RealNodes[Child.IndexInRealNodes].Family.Parents[i].Ranking].Ranking--;
				}
				RealNodes[Child.IndexInRealNodes].Family.Parents.RemoveAt(Child.Ranking);
			}
			for (auto& Bro : Des.Family.Brothers)
			{
				if (Bro.Ranking == RealNodes[Bro.IndexInRealNodes].Family.Brothers.Num() - 1)
				{
					RealNodes[Bro.IndexInRealNodes].Family.Brothers.RemoveAt(Bro.Ranking);
					continue ;
				}
				for (int32 i = Bro.Ranking + 1 ; i < RealNodes[Bro.IndexInRealNodes].Family.Brothers.Num() ; i++)
				{
					RealNodes[RealNodes[Bro.IndexInRealNodes].Family.Brothers[i].IndexInRealNodes].Family.Brothers[RealNodes[Bro.IndexInRealNodes].Family.Brothers[i].Ranking].Ranking--;
				}
				RealNodes[Bro.IndexInRealNodes].Family.Brothers.RemoveAt(Bro.Ranking);
			}
			//上面三个for循环只是处理了和被移除节点相关节点的相关Family的Ranking,不处理IndexInRealNodes
			if (Des.SourceGraphNode != nullptr)
				Des.SourceGraphNode->IndexInRealNodes = -1;

			for (int32 i = Des.IndexInRecorded + 1 ; i < RealNodes.Num() ; i++)
			{
				RealNodes[i].IndexInRecorded--;
				if (RealNodes[i].SourceGraphNode != nullptr)
					RealNodes[i].SourceGraphNode->IndexInRealNodes--;
				for (auto& Parent : RealNodes[i].Family.Parents)
				{
					if (Parent.IndexInRealNodes >= Des.IndexInRecorded && Parent.IndexInRealNodes < i)
						RealNodes[Parent.IndexInRealNodes + 1].Family.Children[Parent.Ranking].IndexInRealNodes--;
					else
						RealNodes[Parent.IndexInRealNodes].Family.Children[Parent.Ranking].IndexInRealNodes--;
				}
				for (auto& Child : RealNodes[i].Family.Children)
				{
					if (Child.IndexInRealNodes >= Des.IndexInRecorded && Child.IndexInRealNodes < i)
						RealNodes[Child.IndexInRealNodes + 1].Family.Parents[Child.Ranking].IndexInRealNodes--;
					else
						RealNodes[Child.IndexInRealNodes].Family.Parents[Child.Ranking].IndexInRealNodes--;
				}
				for (auto& Bro : RealNodes[i].Family.Brothers)
				{
					if (Bro.IndexInRealNodes >= Des.IndexInRecorded && Bro.IndexInRealNodes < i)
						RealNodes[Bro.IndexInRealNodes + 1].Family.Brothers[Bro.Ranking].IndexInRealNodes++;
					else
						RealNodes[Bro.IndexInRealNodes].Family.Brothers[Bro.Ranking].IndexInRealNodes--;
				}
			}
			//修复SourceGraphView::Clans::Indexs的错误指向
			if (SourceGraphView->NamesConstructConfig.NamingOfRules == true)
			{
				for (auto& Clan : SourceGraphView->Clans)
				{
					for (int32 i = Clan.Indexs.Num() - 1; i >= 0; i--)
					{
						if (Clan.Indexs[i] == Des.IndexInRecorded)
						{
							Clan.Indexs.RemoveAt(i);
							continue ;
						}
						if (Clan.Indexs[i] > Des.IndexInRecorded)
							Clan.Indexs[i]--;
					}
				}
			}
			SourceGraphView->AddRemovedNodeName(Des);
			RealNodes.RemoveAt(Des.IndexInRecorded);
		}
	}
}

UGraphView* UFunctionTools_GraphWeaver::GetViewAndIndexFromNode(UGraphNode* Target, int32& Index)
{
	Index = Target->IndexInRealNodes;
	return Target->SourceGraphView;
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
    
	if (NamesValue.Precision < 1)
	{
		Target->NamesConstructConfig.Precision = 1;
	}
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

TArray<int32> UFunctionTools_GraphWeaver::GetEmptyIntArray()
{
	return TArray<int32>{};
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
