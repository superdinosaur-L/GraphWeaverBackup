// Copyright 2026 RainButterfly. All Rights Reserved.

#include "K2NodeForGraph.h"
#include "GraphNode.h"
#include "Engine/Engine.h"
#include <functional>
#include "Math/RandomStream.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "EdGraphUtilities.h"
#include "FunctionTools.h"
#include "GraphView.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Self.h"
#include "KismetCompiler.h"
#include "K2Node_MakeStruct.h"
#include "Editor.h"  // 用于GEditor
#include "EdGraph/EdGraphPin.h"
#include "K2Node_VariableGet.h"

FString UK2Node_SpawnGraphView::GenerateRandomString(int32 Length)
{
	const FString ValidChars = TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
    
	// 创建随机数生成器
	FRandomStream RandomStream;
	RandomStream.GenerateNewSeed(); // 生成新的随机种子
    
	FString Result;
	Result.Reserve(Length); // 预分配内存提高性能
    
	for (int32 i = 0; i < Length; ++i)
	{
		// 从有效字符中随机选择一个
		int32 RandomIndex = RandomStream.RandHelper(ValidChars.Len());
		Result.AppendChar(ValidChars[RandomIndex]);
	}
    
	return Result;
}

std::suspend_never FEditorLinkerTask::promise_type::initial_suspend() noexcept
{
	return {};
}

std::suspend_never FEditorLinkerTask::promise_type::final_suspend() noexcept
{
	return {};
}

FEditorLinkerTask FEditorLinkerTask::promise_type::get_return_object() noexcept
{
	return {};
}

void FEditorLinkerTask::promise_type::return_void() noexcept
{
	
}

void FEditorLinkerTask::promise_type::unhandled_exception() noexcept
{
	UE_LOG(LogTemp, Error, TEXT("Coroutine exception occurred."));
	WAITING_MOD_LOG_UK2NODE();
}



bool FPollAwaiter::IsConditionMet(std::shared_ptr<PollState> State)
{
	UK2Node_SpawnGraphView* View = State->WeakOwner.Get();
	if (!View || !IsValid(View))
		return false;

	UBlueprint* Blueprint = View->GetBlueprint();
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Warning, TEXT("等待蓝图初始化..."));
		return false;
	}

	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);
	for (auto Graph : AllGraphs)
	{
		if (Graph->Nodes.Contains(View))
			return true;
	}
	
	return false;
}

void FPollAwaiter::StartPolling(std::shared_ptr<PollState> State, float ElapsedTime)
{
	//如果执行完了寻找任务或者还没有在Blueprint->Nodes里面找到目标View，目标View就直接被删除了，此时不需要再执行协程函数了
	if (!State->bIsActive || !State->WeakOwner.IsValid())
		return ;

	//更新剩余时间
	State->ElapsedTime = ElapsedTime;

	//如果在Nodes里找到了目标View或者直接用户电脑运行太慢，超过了最大运行上限时间，就直接恢复协程执行最后一个阶段
	if (IsConditionMet(State) || State->ElapsedTime >= State->TimeoutSeconds)
	{
		State->bIsActive = false;
		AsyncTask(ENamedThreads::GameThread,
			[State]()
			{
				//检查Handle是否还有效，Handle控制的任务是否已经完成
				if (State->Handle && !State->Handle.done())
					State->Handle.resume();
			});
		return ;
	}

	//使用编辑器全局定时器
	if (UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr ; EditorWorld)
	{
		FTimerManager& TimerManager = EditorWorld->GetTimerManager();
		//设置定时器
		TimerManager.SetTimer(State->TimerHandle,
			[State, NextElapsedTime = ElapsedTime + 0.1f]() mutable 
			{
				AsyncTask(ENamedThreads::GameThread,
					[State, NextElapsedTime]()
					{
						StartPolling(State, NextElapsedTime);
					});
			},
			0.1f,
			false
			);
	}
}

bool FPollAwaiter::await_ready() noexcept
{
	return TimeoutSeconds <= 0.f;
}

void FPollAwaiter::await_suspend(std::coroutine_handle<> handle) noexcept
{
	// 创建共享状态对象，延续生命周期
	auto SharedState = std::make_shared<PollState>();
	SharedState->Handle = handle;
	SharedState->TimeoutSeconds = TimeoutSeconds;
	SharedState->WeakOwner = WeakOwner;
	SharedState->bIsActive = bIsActive.load();
    	
	StartPolling(SharedState, 0.0f);
}

void FPollAwaiter::await_resume() noexcept
{
}



AllGraphViewArray::AllGraphViewArray()
{
	Views.Empty();
}

// 添加节点（线程安全）
void AllGraphViewArray::AddView(UK2Node_SpawnGraphView* View)
{
	if (!IsValid(View))[[unlikely]]  // 安全检查
		return;

	// 自动加锁，出作用域解锁
	FScopeLock Lock(&ArrayMutex);
	int32 Index = 0;
	for ( ; Index < Views.Num(); ++Index)
	{
		if (Views[Index].Get() == View)
			break;
	}
	if (Index == Views.Num())
	{
		Views.Emplace(View);
		View->IndexInViewArray = Index;
	}
}

void AllGraphViewArray::RemoveView(UK2Node_SpawnGraphView* View)
{
	FScopeLock Lock(&ArrayMutex);
	for (int32 Index = Views.Num() - 1; Index >= 0; --Index)
	{
		if (Views[Index].Get() == View)
		{
			Views.RemoveAt(Index);
			return ;
		}
	}
}

UK2Node_SpawnGraphView* AllGraphViewArray::FindViewByCommonName(const FString& Name) const
{
	FScopeLock Lock(&ArrayMutex);
	
	for (auto View : Views)
	{
		if (IsValid(View.Get()) && View->GraphViewName == Name)
		{
			return View.Get();
		}
	}
    
	return nullptr;
}

TArray<TStrongObjectPtr<UK2Node_SpawnGraphView>>& AllGraphViewArray::GetAllViews()
{
	return  Views;
}

void AllGraphViewArray::UpdateAllViewIndex()
{
	for (int32 Index = 0 ; Index < Views.Num(); ++Index)
	{
		Views[Index]->IndexInViewArray = Index;
	}
}

void AllGraphViewArray::UpdateChildIndexForParent(int32 RemovedIndex)
{
	for (auto ParentView : Views)
	{
		auto RealView = ParentView.Get();
		for (auto& ChildIndex : RealView->ChildNodeIndex)
		{
			if (ChildIndex > RemovedIndex)
				ChildIndex -= 1;
		}
	}
}


#define LLOCTEXT_NAMESPACE "GraphKN"


class FKCHandler_SpawnGraphView : public FNodeHandlingFunctor
{
public:
	FKCHandler_SpawnGraphView(FKismetCompilerContext& InCompilerContext):
		FNodeHandlingFunctor(InCompilerContext){}

	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		FBlueprintCompiledStatement& Nop = Context.AppendStatementForNode(Node);
		Nop.Type = KCST_Nop;
	}
};


UEdGraphPin* UK2Node_SpawnGraphView::GetEnumPin() const
{
	for (auto Pin : Pins)
	{
		if (Pin->PinName == "ConstructMethod" && Pin->Direction == EGPD_Input)
			return Pin;
	}
	return nullptr;
}

UEdGraphPin* UK2Node_SpawnGraphView::GetNamesConfigPin() const
{
	for (auto Pin : Pins)
	{
		if (Pin->PinName == "NamesConstructConfig" && Pin->Direction == EGPD_Input)
			return Pin;
	}
	return nullptr;
}


UEdGraphPin* UK2Node_SpawnGraphView::GetReturnValuePin() const
{
	for (auto Pin: Pins)
	{
		if (Pin->PinName == "GraphView" && Pin->Direction == EGPD_Output)
			return Pin;
	}
	return nullptr;
}

UEdGraphPin* UK2Node_SpawnGraphView::GetSelfOwnerPin() const
{
	for (auto Pin : Pins)
	{
		if (Pin->PinName == "SelfOwner" && Pin->Direction == EGPD_Input)
			return Pin;
	}
	return nullptr;
}

UEdGraphPin* UK2Node_SpawnGraphView::GetViewNamePin() const
{
	for (auto Pin : Pins)
	{
		if (Pin->PinName == "GraphViewName" && Pin->Direction == EGPD_Input)
			return Pin;
	}
	return nullptr;
}

TArray<UK2Node_SpawnGraphNode*> UK2Node_SpawnGraphView::GetRealSpawnNodes()
{
	TArray<UK2Node_SpawnGraphNode*> rr;
	for (int32 ChildIndex : ChildNodeIndex)
	{
		auto& Nodes = AllGraphNodeArray::Get().GetNodes();
		rr.Emplace(Nodes[ChildIndex].Get());
	}
	return rr;
}

void UK2Node_SpawnGraphView::UpdateChildrenNodeIndex(int32 RemovedChildIndex)
{
	for (auto& OldIndex : ChildNodeIndex)
	{
		if (OldIndex > RemovedChildIndex)
			OldIndex -= 1;
	}
}


TArray<UEdGraphPin*> UK2Node_SpawnGraphView::BuildConfigPinsFirst()
{
	TArray<UEdGraphPin*> Result;
	int32 Index = Pins.Num();
	switch (IndexValueOfConstructMethod)
	{
	case 0:
		{
			IndexOfPinsFirst_Names.Empty();
			UEdGraphPin* NewPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct, FNamesConstructConfig::StaticStruct(),
				TEXT("NamesConstructConfig"));
			IndexOfPinsFirst_Names.Emplace(Index);
			Index++;
			Result.Emplace(NewPin);
			return Result;
		}
	case 1:
		break ;
	default:
		WAITING_MOD_LOG_UK2NODE();
	}
	return Result;
}

//拿外部蓝图生成的UClass
UClass* UK2Node_SpawnGraphView::GetBlueprintGenerateClass()
{
	UBlueprint* OuterBlueprint = GetBlueprint();
	if (!OuterBlueprint)
		return nullptr;
	return OuterBlueprint->GeneratedClass;
}

TArray<UEdGraphPin*> UK2Node_SpawnGraphView::GetDefaultPins(bool IncludeSubPins)
{
	TArray<UEdGraphPin*> rr;
	for (int32 Index = 0 ; Index < NumOfDefaultPins; ++Index)
	{
		rr.Emplace(Pins[Index]);
		if (IncludeSubPins)
		{
			for (auto SubPin : Pins[Index]->SubPins)
			{
				rr.Emplace(SubPin);
			}
		}
	}
	return rr;
}

TArray<UEdGraphPin*> UK2Node_SpawnGraphView::GetPinsFirst(bool IncludeSubPins)
{
	TArray<UEdGraphPin*> rr;
	switch (IndexValueOfConstructMethod)
	{
	case 0:
		{
			for (int32 Index : IndexOfPinsFirst_Names)
			{
				rr.Emplace(Pins[Index]);
				if (IncludeSubPins)
				{
					for (auto Pin : Pins[Index]->SubPins)
					{
						rr.Emplace(Pin);
					}
				}
			}
		}
		break ;
	case 1:
		{
			;
		}
		break ;
	default:
		WAITING_MOD_LOG_UK2NODE();
	}
	return rr;
}

void UK2Node_SpawnGraphView::UpdatePinsFirst()
{
	switch (IndexValueOfConstructMethod)
	{
	case 0:
		{
			PinsFirst_Names = GetPinsFirst();
		}
		break ;
	case 1:
		{
			;
		}
		break ;
	default:
		WAITING_MOD_LOG_UK2NODE();
	}
}


void UK2Node_SpawnGraphView::UpdateGraphViewNameForThisNode(bool NeedRandom)
{
	FString& Name = GetViewNamePin()->DefaultValue;
	if (Name.Len() == 0 && NeedRandom)
	{
		FString PreStr = GenerateRandomString(5);
		uint8 Find = 0;
		do
		{
			Find = 0;
			for (auto& View : AllGraphViewArray::Get().GetAllViews())
			{
				if (View->GraphViewName == PreStr)
				{
					Find = 1;
					break ;
				}
			}
		}while (Find == 1);
		//如果5位都还能全部用完那我也没办法了
		GraphViewName = PreStr;
		//GraphViewName = GetName();
	}
	else
	{
		GraphViewName = Name;
	}

	FindPinChecked(TEXT("HiddenViewName"))->DefaultValue = GraphViewName;
}


TArray<UK2Node_SpawnGraphNode*> UK2Node_SpawnGraphView::FindWaitNodeByCommonName()
{
	TArray<UK2Node_SpawnGraphNode*> rr;
	for (int32 ChildIndex = NodeWaitArray::Get().GetAllNodes().Num() - 1 ; ChildIndex >= 0; --ChildIndex)
	{
		auto Child = AllGraphNodeArray::Get().GetNodes()[NodeWaitArray::Get().GetAllNodes()[ChildIndex]].Get();
		if (Child->IndexValueOfGetViewWay != 0)//  != Name
			continue ;

		if (Child->GetViewNamePin()->DefaultValue == GraphViewName)
		{
			//UAllGraphViewArray::Get().GetAllViews()[Child->IndexOfSourceGraphView] = this;
			ChildNodeIndex.Emplace(Child->IndexInAllNodeArray);
			Child->IndexOfSourceGraphView = IndexInViewArray;
			NodeWaitArray::Get().GetAllNodes().RemoveAt(ChildIndex);
			rr.Emplace(Child);
		}
	}
	return rr;
}


TArray<UK2Node_SpawnGraphNode*> UK2Node_SpawnGraphView::FindWaitNodeByLink()
{
	TArray<UK2Node_SpawnGraphNode*> rr;
	
	for (int32 ChildIndex = NodeWaitArray::Get().GetAllNodes().Num() - 1 ; ChildIndex >= 0; --ChildIndex)
	{
		auto Child = AllGraphNodeArray::Get().GetNodes()[NodeWaitArray::Get().GetAllNodes()[ChildIndex]];
		if (UEdGraphPin* ViewFamilyPin = Child->GetViewFamilyPin(); ViewFamilyPin)
		{
			UBlueprint* ViewOuterBlueprint = Cast<UBlueprint>(ViewFamilyPin->DefaultObject);
			if (ViewOuterBlueprint && ViewOuterBlueprint == GetBlueprint())
			{
				rr.Emplace(Child.Get());
				Child->IndexOfSourceGraphView = IndexInViewArray;
				Child->FindPinChecked(TEXT("RealViewNameHidden"))->DefaultValue = GraphViewName;
				ChildNodeIndex.Emplace(Child->IndexInAllNodeArray);
				NodeWaitArray::Get().GetAllNodes().RemoveAt(ChildIndex);
			}
		}
	}
	return rr;
}


TArray<UK2Node_SpawnGraphNode*> UK2Node_SpawnGraphView::EmptyChildNode_Name()
{
	TArray<UK2Node_SpawnGraphNode*> rr;
	
	for (int32 ChildIndex = ChildNodeIndex.Num() - 1; ChildIndex >= 0; --ChildIndex)
	{
		auto Child = AllGraphNodeArray::Get().GetNodes()[ChildNodeIndex[ChildIndex]].Get();
		if (Child->IndexValueOfGetViewWay != 0)//  != Name
			continue ;
		//Name
		{
			{
				Child->Modify();
				Child->UpdateDefaultPins();
				Child->UpdateFirstPins();
				Child->UpdateSecondPinsBySourceView();
				Child->BreakLinkedToAllPins();
				Child->ErrorMsg.Reset();
				Child->Pins.Reset();
				Child->AllocateDefaultPins();
				Child->BuildFirstPins();
				Child->FixUpDefaultPins();
				Child->FixUpFirstPins();
				Child->IndexOfSourceGraphView = -1;
				Child->FindPinChecked(TEXT("RealViewNameHidden"))->DefaultValue.Empty();
				Child->GetGraph()->NotifyNodeChanged(Child);
			}
			NodeWaitArray::Get().AddWaitNode(Child);
			ChildNodeIndex.RemoveAt(ChildIndex);
			rr.Emplace(Child);
		}
	}
	return rr;
}

FEditorLinkerTask UK2Node_SpawnGraphView::DelayLinkChild_Link()
{
	double WaitTime = MaxWaitTime;
	if (WaitTime <= 0.2f)
		WaitTime = 0.2f;
	FPollAwaiter WaitForNodes(WaitTime, this);
	co_await WaitForNodes;
	// 延迟结束后，安全执行查找
	TArray<UK2Node_SpawnGraphNode*> NewChild = FindWaitNodeByLink();
	for (UK2Node_SpawnGraphNode* Child : NewChild)
	{
		//防止有神仙手速太快导致父子任意一方被删了导致崩溃
		if (IsValid(Child) && Child->IndexOfSourceGraphView != -1)
		{
			Child->BuildSecondPins();
			//Child->FixUpSecondPins();
			Child->GetGraph()->NotifyNodeChanged(Child);
		}
	}
	co_return;
}


void UK2Node_SpawnGraphView::AllocateDefaultPins()
{
	auto K2Schema = GetDefault<UEdGraphSchema_K2>();
	NumOfDefaultPins = 0;
	
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	NumOfDefaultPins++;
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
	NumOfDefaultPins++;
	
	UEnum* ConstructEnum = StaticEnum<NAConstructMethod::EConstructMethod>();
	UEdGraphPin* EnumPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Byte, ConstructEnum, TEXT("ConstructMethod"));
	NumOfDefaultPins++;
	EnumPin->DefaultValue = ConstructEnum->GetNameStringByIndex(IndexValueOfConstructMethod);
	EnumPin->bNotConnectable = true;
	
	UEdGraphPin* ViewNamePin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_String, TEXT("GraphViewName"));
	NumOfDefaultPins++;
	ViewNamePin->PinFriendlyName = NSLOCTEXT("PreGraphKN", "ViewNamePinFriendlyName", "GraphViewName");
	ViewNamePin->bNotConnectable = true;
	if (GraphViewName == GetName())
		ViewNamePin->DefaultValue = "";
	else
		ViewNamePin->DefaultValue = GraphViewName;
	
	UEdGraphPin* OwnerPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UObject::StaticClass(), TEXT("SelfOwner"));
	NumOfDefaultPins++;
	OwnerPin->bHidden = true;

	//创建一个隐藏的Pin来真正保存当前蓝图节点的GraphViewName
	UEdGraphPin* HiddenViewNamePin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_String, TEXT("HiddenViewName"));
	NumOfDefaultPins++;
	HiddenViewNamePin->DefaultValue = "";
	HiddenViewNamePin->bHidden = true;

	UEnum* DealSameNodeEnum = StaticEnum<NAWayToDealSameGraphNode::EWayToDealSameGraphNode>();
	UEdGraphPin* DealSameNodePin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Byte, DealSameNodeEnum, TEXT("WayToDealSameGraphNode"));
	DealSameNodePin->DefaultValue = DealSameNodeEnum->GetNameStringByIndex(IndexValueOfWayToDealSameNode);
	NumOfDefaultPins++;

	UEdGraphPin* AllocateSizePin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, TEXT("AllocateSize"));
	K2Schema->SetPinAutogeneratedDefaultValue(AllocateSizePin, TEXT("0"));
	NumOfDefaultPins++;
	
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Object, UGraphView::StaticClass(), TEXT("GraphView"));
	NumOfDefaultPins++;
	
	if (ENodeAdvancedPins::NoPins == AdvancedPinDisplay)
	{
		AdvancedPinDisplay = ENodeAdvancedPins::Hidden;
	}
}

void UK2Node_SpawnGraphView::PostReconstructNode()
{
	Super::PostReconstructNode();
}

void UK2Node_SpawnGraphView::PostPasteNode()
{
	Message_Error(TEXT("Creating a new 'SpawnGraphView' by copy-pasting is not allowed."));
	BeCopied = 1;
}

void UK2Node_SpawnGraphView::ReconstructNode()
{
	if (BeCopied == 1)
	{
		ChildNodeIndex.Empty();
		return ;
	}
	
	ChildNodeIndex.Empty();
	
	TArray<UEdGraphPin*> OldPins(Pins);
	Modify();
	ErrorMsg.Reset();
	
	Pins.Reset();
	AllocateDefaultPins();
	BuildConfigPinsFirst();
	RestoreSplitPins(OldPins);
	TMap<UEdGraphPin*, UEdGraphPin*> NewPinsToOldPins;
	RewireOldPinsToNewPins(OldPins, Pins, &NewPinsToOldPins);
	//UpdateGraphViewNameForThisNode();
	PostReconstructNode();
	if (HasAnyFlags(RF_Transactional))
		AllGraphViewArray::Get().AddView(this);
	GetGraph()->NotifyNodeChanged(this);
	
	//我真棒！💪✨ (๑•̀ㅂ•́)و✧ (◍•ᴗ•◍)❤
}

void UK2Node_SpawnGraphView::PostPlacedNewNode()
{
	Super::PostPlacedNewNode();
	BuildConfigPinsFirst();
	AllGraphViewArray::Get().AddView(this);
	UpdateGraphViewNameForThisNode(true);
	//此时已经有Blueprint的父亲关系了，只是Blueprint的Nodes还没有把新创建的节点添加进来
	DelayLinkChild_Link();
}


FText UK2Node_SpawnGraphView::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("PreGraphKN" ,"Spawn", "SpawnGraphView");
}

FSlateIcon UK2Node_SpawnGraphView::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon(FAppStyle::GetAppStyleSetName(), "GraphEditor.SpawnActor_16x");
	return Icon;
}

FText UK2Node_SpawnGraphView::GetTooltipText() const
{
	return NSLOCTEXT("PreGraphKN", "NodeToolTip",
		"Nodes specifically designed for generating GraphView.\n "
		"Please do not create a new 'SpawnGraphView' by copying and pasting from another 'SpawnGraphView'. \n "
		"If you assign a non-empty string to 'GraphViewName', ensure it is unique and does not match the 'GraphViewName' \n"
		" of any previously created 'SpawnGraphView'.\n "
		"Warning! Do NOT use 'Ctrl + Z' to undo operations for this blueprint node. Since this blueprint involves communication between different blueprints, \n "
		"using this undo method will most likely cause chaos in the entire plugin functionality and result in malfunctions.");
}

void UK2Node_SpawnGraphView::DestroyNode()
{
	if (BeCopied == 1 || NameSameAsOtherView == 1)
	{      
		Super::DestroyNode();
		return ;
	}
	AllGraphViewArray::Get().RemoveView(this);
	AllGraphViewArray::Get().UpdateAllViewIndex();
	AllGraphNodeArray::Get().UpdateParentIndexForNode(IndexInViewArray);
	for (auto Child : GetRealSpawnNodes())
	{
		Child->Modify();
		Child->UpdateDefaultPins();
		Child->UpdateFirstPins();
		Child->UpdateSecondPinsNotBySourceView();
		Child->BreakLinkedToAllPins();
		Child->ErrorMsg.Reset();
		Child->Pins.Reset();
		Child->AllocateDefaultPins();
		Child->FixUpDefaultPins();
		Child->BuildFirstPins();
		Child->FixUpFirstPins();
		Child->IndexOfSourceGraphView = -1;
		if (Child->IndexValueOfGetViewWay == 1)//Link
		{
			UBlueprint* Blueprint = GetBlueprint();
			TArray<UEdGraph*> AllGraphs;
			Blueprint->GetAllGraphs(AllGraphs);
			uint8 Find = 0;
			for (auto Graph : AllGraphs)
			{
				for (auto Node : Graph->Nodes)
				{
					UK2Node_SpawnGraphView* View = Cast<UK2Node_SpawnGraphView>(Node);
					if (View && View != this && View->BeCopied == 0)
					{
						Child->IndexOfSourceGraphView = View->IndexInViewArray;
						View->ChildNodeIndex.Emplace(Child->IndexInAllNodeArray);
						Child->FindPinChecked(TEXT("RealViewNameHidden"))->DefaultValue = View->GraphViewName;
						Child->GetBlueprint()->Status = BS_Dirty;
						Find = 1;
						break ;
					}
				}
				if (Find)
					break ;
			}
			if (Find)
			{
				Child->BuildSecondPins();
				Child->FixUpSecondPins();
			}
		}
		Child->GetGraph()->NotifyNodeChanged(Child);
		Child->PostReconstructNode();
	}
	Super::DestroyNode();
}

void UK2Node_SpawnGraphView::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	if (BeCopied == 1)
	{
		MessageLog.Error(TEXT("@@: Creating a new 'SpawnGraphView' by copy-pasting is not allowed."), this);
	}
	if (NameSameAsOtherView == 1)
	{
		FString FirstName;
		for (auto ElView : AllGraphViewArray::Get().GetAllViews())
		{
			if (ElView->IndexValueOfConstructMethod == 0 && ElView->GetViewNamePin()->DefaultValue == GetViewNamePin()->DefaultValue)
			{
				FirstName = ElView->GetName();
				break ;
			}
		}
		FString ErrorMessage = "@@: The non-empty 'GraphViewName' value provided to this 'SpawnGraphView' must be unique and cannot be \n"
						"the same as that of any other 'SpawnGraphView': " + FirstName;
		MessageLog.Error(*ErrorMessage, this);
	}
}

void UK2Node_SpawnGraphView::GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const
{
	const UEdGraphSchema_K2* SchemaHelper = GetDefault<UEdGraphSchema_K2>();
	if (UEdGraphPin* EnumPin = GetEnumPin())
	{
		SchemaHelper->ConstructBasicPinTooltip(*EnumPin,
			NSLOCTEXT("PreGraphKN", "EnumPinToolTip", "Choose the construction method of the graph"), EnumPin->PinToolTip);
	}
	if (UEdGraphPin* GraphViewNamePin = GetViewNamePin())
	{
		SchemaHelper->ConstructBasicPinTooltip(*GraphViewNamePin,
			NSLOCTEXT("PreGraphKN", "GraphViewPinToolTip", "The name you want to give this GraphView. If you don't specify a name, one will be generated automatically."),
			GraphViewNamePin->PinToolTip);
	}

	if (UEdGraphPin* NamingOfRulesPin = FindPin(TEXT("NamesConstructConfig_NamingOfRules"), EGPD_Input))
		SchemaHelper->ConstructBasicPinTooltip(*NamingOfRulesPin,
			NSLOCTEXT("PreGraphKN", "NamingOfRulesPinTooltip", "If you want to use the Names method for construction but do not wish to follow a structured \n"
													  "naming convention, set this option to false to reduce memory usage; otherwise, set it to true. "
													  "\nImproper configuration may significantly degrade construction performance. "
													  "\nNote: This option only takes effect when using the Names construction method."),
													  NamingOfRulesPin->PinToolTip);

	if (UEdGraphPin* PrecisionPin = FindPin(TEXT("NamesConstructConfig_Precision"), EGPD_Input))
		SchemaHelper->ConstructBasicPinTooltip(*PrecisionPin,
		NSLOCTEXT("PreGraphKN", "PrecisionPinTooltip", "How many characters at the beginning of a name define a clan (family).\n"
													"For example:\n"
													"If you have names like AA2, AA3, BB2, BB3, then the Precision value should be 2.\n"
													"If you have AA1 and AB2, the Precision can be either 1 (using just A) or 2 (using AA and AB).\n"
													"Note: This option only takes effect when the construction method is set to Names and NamingOfRules is true.\n"
													"If this value exceeds the length of a given name, the entire name will be used.\n"
													"If the value is less than 1, it defaults to 1."),
		PrecisionPin->PinToolTip);

	if (UEdGraphPin* AllocatePin = FindPinChecked(TEXT("AllocateSize")))
		SchemaHelper->ConstructBasicPinTooltip(*AllocatePin,
		NSLOCTEXT("PreGraphKN", "AllocatePinToolTip", "The approximate number of 'GraphNode's you intend to include in this 'GraphView'. \n "
												   "For example, if you plan to add 10 'GraphNode's (excluding the root node inherent to 'GraphView') to this 'GraphView', \n "
													"you can specify 10 or a larger capacity. This allows the graph construction process to be slightly faster."),
													AllocatePin->PinToolTip);

	if (UEdGraphPin* WayToDealSameNodePin = FindPinChecked(TEXT("WayToDealSameGraphNode")))
		SchemaHelper->ConstructBasicPinTooltip(*WayToDealSameNodePin,
			NSLOCTEXT("PreGraphKn", "WayToDealSameNodePinToolTip", "How do you want to be notified when a 'GraphNode' is added to the 'GraphView' multiple times?"),
													WayToDealSameNodePin->PinToolTip);
	
	Super::GetPinHoverText(Pin, HoverTextOut);
}




FText UK2Node_SpawnGraphView::GetMenuCategory() const
{
	return NSLOCTEXT("PreGraphKN", "MenuCategory", "GraphWeaver");
}

void UK2Node_SpawnGraphView::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(ActionKey);
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

bool UK2Node_SpawnGraphView::ShouldShowNodeProperties() const
{
	return true;
}


void UK2Node_SpawnGraphView::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	if (Pin->PinName == GetEnumPin()->PinName)
	{
		Modify();
		for (auto Child : GetRealSpawnNodes())
		{
			Child->Modify();
			Child->UpdateSecondPinsBySourceView();
			Child->BreakLinkedToSecondPins();
		}
		TArray<UEdGraphPin*> OldDefaultPins = GetDefaultPins();
		UpdatePinsFirst();
		for (auto SelfOldPin : Pins)
		{
			for (auto LinkedPin : SelfOldPin->LinkedTo)
			{
				LinkedPin->LinkedTo.RemoveSingle(SelfOldPin);
				GetGraph()->NotifyNodeChanged(LinkedPin->GetOwningNode());
			}
		}
		IndexValueOfConstructMethod = StaticEnum<NAConstructMethod::EConstructMethod>()->GetIndexByNameString(Pin->DefaultValue);
		ErrorMsg.Reset();
		Pins.Reset();
		AllocateDefaultPins();
		TMap<UEdGraphPin*, UEdGraphPin*> DefaultPldPinToNewPin;
		RewireOldPinsToNewPins(OldDefaultPins, Pins, &DefaultPldPinToNewPin);
		BuildConfigPinsFirst();
		switch (IndexValueOfConstructMethod)
		{
		case 0:
			{
				RestoreSplitPins(PinsFirst_Names);
				TMap<UEdGraphPin*, UEdGraphPin*> NewPinsToOldPins;
				TArray<UEdGraphPin*> NewFirstPins = GetPinsFirst();
				RewireOldPinsToNewPins(PinsFirst_Names, NewFirstPins, &NewPinsToOldPins);
			}
			break;
		case 1:
			{
				;
			}
			break ;
		default:
			WAITING_MOD_LOG_UK2NODE();
		}
		//UpdateGraphViewNameForThisNode();
		GetGraph()->NotifyNodeChanged(this);
		//上面完成自我重建
		
		for (auto Child : GetRealSpawnNodes())
		{
			Child->UpdateDefaultPins();
			Child->UpdateFirstPins();
			Child->ErrorMsg.Reset();
			Child->Pins.Reset();
			Child->AllocateDefaultPins();
			Child->BuildFirstPins();
			Child->FixUpDefaultPins();
			Child->FixUpFirstPins();
			Child->BuildSecondPins();
			Child->FixUpSecondPins();
			Child->GetBlueprint()->Status = BS_Dirty;
			Child->GetGraph()->NotifyNodeChanged(Child);
		}
		
		return ;
	}
	
	if (Pin->PinName == GetViewNamePin()->PinName)
	{
		for (auto& ElView : AllGraphViewArray::Get().GetAllViews())
		{
			if (ElView->IndexValueOfConstructMethod == 0 && ElView.Get() != this)
			{
				if (ElView->GetViewNamePin()->DefaultValue == Pin->DefaultValue && Pin->DefaultValue.Len() > 0)
				{
					NameSameAsOtherView = 1;
					return ;
				}
			}
		}
		NameSameAsOtherView = 0;
		
		UpdateGraphViewNameForThisNode(true);
		EmptyChildNode_Name();
		TArray<UK2Node_SpawnGraphNode*> NewChild = FindWaitNodeByCommonName();
		for (auto Child : NewChild)
		{
			Child->BuildSecondPins();
			Child->FixUpSecondPins();
			Child->GetGraph()->NotifyNodeChanged(Child);
		}
		for (auto Child : GetRealSpawnNodes())
		{
			//UE_LOG(LogTemp, Error, TEXT("SpawnView|PinValue|ChildSourceVViewNameReset, NewName: %s"), *GraphViewName);
			//EMPTY_LOG();
			Child->Modify();
			Child->FindPinChecked(TEXT("RealViewNameHidden"))->DefaultValue = GraphViewName;
			Child->GetBlueprint()->Status = EBlueprintStatus::BS_Dirty;
			Child->GetGraph()->NotifyNodeChanged(Child);
			//FKismetCompilerContext::Compile();
		}
		return ;
	}
	if (Pin == FindPinChecked(TEXT("WayToDealSameGraphNode")))
	{
		Modify();
		IndexValueOfWayToDealSameNode = StaticEnum<NAWayToDealSameGraphNode::EWayToDealSameGraphNode>()->GetIndexByNameString(Pin->DefaultValue);
		GetGraph()->NotifyNodeChanged(this);
	}
}



void UK2Node_SpawnGraphView::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);
	
	UK2Node_SpawnGraphView* SpawnNode = this;
	
	UK2Node_CallFunction* CreateView = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(SpawnNode, SourceGraph);
	CreateView->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFunctionTools_GraphWeaver, CreateGraphView_NotManuallyCall),UFunctionTools_GraphWeaver::StaticClass());
	CreateView->AllocateDefaultPins();
	
	UK2Node_Self* SelfOuter = CompilerContext.SpawnIntermediateNode<UK2Node_Self>(SpawnNode, SourceGraph);
	SelfOuter->AllocateDefaultPins();

	SelfOuter->FindPinChecked(TEXT("Self"))->MakeLinkTo(CreateView->FindPinChecked(TEXT("Outer")));

	//创建修改对象BaseAttribute的节点
	UK2Node_CallFunction* ModViewAttri = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(SpawnNode, SourceGraph);
	ModViewAttri->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFunctionTools_GraphWeaver, ModGraphViewBaseAttri_NotManuallyCall), UFunctionTools_GraphWeaver::StaticClass());
	ModViewAttri->AllocateDefaultPins();

	int32 ConstructMethodIndex = StaticEnum<NAConstructMethod::EConstructMethod>()->GetIndexByNameString(GetEnumPin()->DefaultValue);
	switch (ConstructMethodIndex)
	{
	case 0://NAConstructMethod::Names
		{
			UK2Node_CallFunction* ModViewNaCon = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
			ModViewNaCon->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFunctionTools_GraphWeaver, ModGraphViewNaCon_NotManuallyCall), UFunctionTools_GraphWeaver::StaticClass());
			ModViewNaCon->AllocateDefaultPins();

			UK2Node_CallFunction* ModViewFinal = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
			ModViewFinal->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFunctionTools_GraphWeaver, ModGraphViewFinalPhase_NotManuallyCall), UFunctionTools_GraphWeaver::StaticClass());
			ModViewFinal->AllocateDefaultPins();
			
			CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *CreateView->GetExecPin());
			CreateView->GetThenPin()->MakeLinkTo(ModViewAttri->GetExecPin());
			ModViewAttri->GetThenPin()->MakeLinkTo(ModViewNaCon->GetExecPin());
			ModViewNaCon->GetThenPin()->MakeLinkTo(ModViewFinal->GetExecPin());
			CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *ModViewFinal->GetThenPin());

			CreateView->GetReturnValuePin()->MakeLinkTo(ModViewAttri->FindPinChecked(TEXT("Target")));
			SelfOuter->FindPinChecked(TEXT("Self"))->MakeLinkTo(ModViewAttri->FindPinChecked(TEXT("SelfOwner")));
			CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("HiddenViewName")), *ModViewAttri->FindPinChecked(TEXT("ViewName")));
			CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("ConstructMethod")), *ModViewAttri->FindPinChecked(TEXT("EnumValue")));
			CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("WayToDealSameGraphNode")), *ModViewAttri->FindPinChecked(TEXT("DealSameNode")));
			ModViewAttri->GetReturnValuePin()->MakeLinkTo(ModViewNaCon->FindPinChecked(TEXT("Target")));
			CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("AllocateSize")), *ModViewFinal->FindPinChecked(TEXT("SizeAllocate")));
			ModViewNaCon->GetReturnValuePin()->MakeLinkTo(ModViewFinal->FindPinChecked(TEXT("Target")));

			UEdGraphPin* NamesConfigPin = GetNamesConfigPin();
			if (NamesConfigPin->SubPins.Num() > 0)
			{
				UK2Node_MakeStruct* NamesConfigStruct = CompilerContext.SpawnIntermediateNode<UK2Node_MakeStruct>(this, SourceGraph);
				NamesConfigStruct->StructType = FNamesConstructConfig::StaticStruct();
				NamesConfigStruct->bMadeAfterOverridePinRemoval = true;
				NamesConfigStruct->AllocateDefaultPins();

				CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("NamesConstructConfig_NamingOfRules")),
					*NamesConfigStruct->FindPinChecked(TEXT("NamingOfRules")));
				CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("NamesConstructConfig_Precision")),
					*NamesConfigStruct->FindPinChecked(TEXT("Precision")));

				NamesConfigStruct->FindPinChecked(TEXT("NamesConstructConfig"))->MakeLinkTo(ModViewNaCon->FindPinChecked(TEXT("NamesValue")));
			}
			else
				CompilerContext.MovePinLinksToIntermediate(*GetNamesConfigPin(), *ModViewNaCon->FindPinChecked(TEXT("NamesValue")));
			
			CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("GraphView")), *ModViewFinal->GetReturnValuePin());
		}
		break ;
	case 1://NAConstructMethod::LHCode_G
		{
			UK2Node_CallFunction* ModViewFinal = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
			ModViewFinal->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFunctionTools_GraphWeaver, ModGraphViewFinalPhase_NotManuallyCall), UFunctionTools_GraphWeaver::StaticClass());
			ModViewFinal->AllocateDefaultPins();
			
			CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *CreateView->GetExecPin());
			CreateView->GetThenPin()->MakeLinkTo(ModViewAttri->GetExecPin());
			ModViewAttri->GetThenPin()->MakeLinkTo(ModViewFinal->GetExecPin());
			ModViewAttri->GetReturnValuePin()->MakeLinkTo(ModViewFinal->FindPinChecked(TEXT("Target")));
			CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *ModViewFinal->GetThenPin());

			CreateView->GetReturnValuePin()->MakeLinkTo(ModViewAttri->FindPinChecked(TEXT("Target")));
			SelfOuter->FindPinChecked(TEXT("Self"))->MakeLinkTo(ModViewAttri->FindPinChecked(TEXT("SelfOwner")));
			CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("HiddenViewName")), *ModViewAttri->FindPinChecked(TEXT("ViewName")));
			CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("ConstructMethod")), *ModViewAttri->FindPinChecked(TEXT("EnumValue")));
			CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("WayToDealSameGraphNode")), *ModViewAttri->FindPinChecked(TEXT("DealSameNode")));
			CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("AllocateSize")), *ModViewFinal->FindPinChecked(TEXT("SizeAllocate")));

			CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("GraphView")), *ModViewFinal->GetReturnValuePin());
		}
		break ;
	default:
		WAITING_MOD_LOG();
	}

	SpawnNode->BreakAllNodeLinks();
}


class FNodeHandlingFunctor* UK2Node_SpawnGraphView::CreateNodeHandler(
	class FKismetCompilerContext& CompilerContext) const
{
	//先不进行正确的转换
	return new FKCHandler_SpawnGraphView(CompilerContext);
}

void UK2Node_SpawnGraphView::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);
}

UK2Node_SpawnGraphView::UK2Node_SpawnGraphView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	IndexValueOfConstructMethod = 0;//默认为Names
	IndexInViewArray = -1;
	MaxWaitTime = 2.0f;
	BeCopied = 0;
	NameSameAsOtherView = 0;
	IndexValueOfWayToDealSameNode = 0;
}

#undef LLOCTEXT_NAMESPACE









#define LLOCTEXT_NAMESPACE "GraphNodeKN"








UEdGraphPin* UK2Node_SpawnGraphNode::GetViewNamePin() const
{
	for (auto Pin : Pins)
	{
		if (Pin->PinName == TEXT("GraphViewName"))
			return Pin;
	}
	return nullptr;
}

UEdGraphPin* UK2Node_SpawnGraphNode::GetViewFamilyPin() const
{
	for (auto Pin : Pins)
	{
		if (Pin->PinName == TEXT("GraphViewFamily"))
			return Pin;
	}
	return nullptr;
}

UK2Node_SpawnGraphView* UK2Node_SpawnGraphNode::GetRealSpawnView()
{
	return AllGraphViewArray::Get().GetAllViews()[IndexOfSourceGraphView].Get();
}

TArray<UEdGraphPin*> UK2Node_SpawnGraphNode::GetDefaultPins()
{
	TArray<UEdGraphPin*> rr;
	for (auto Pin : Pins)
	{
		for (auto& Name : PinsDefault)
		{
			if (Pin->PinName.ToString() == Name)
			{
				rr.Emplace(Pin);
				break ;
			}
		}
	}
	return rr;
}

void UK2Node_SpawnGraphNode::UpdateDefaultPins()
{
	PinsDefault_Ptr = GetDefaultPins();
}

void UK2Node_SpawnGraphNode::FixUpDefaultPins()
{
	TMap<UEdGraphPin*, UEdGraphPin*> NewPinsToOldPins;
	RewireOldPinsToNewPins(PinsDefault_Ptr, Pins, &NewPinsToOldPins);
}

void UK2Node_SpawnGraphNode::BuildFirstPins()
{
	switch (IndexValueOfGetViewWay)
	{
	case 0://Name
		{
			PinsFirst_Name.Empty();
			UEdGraphPin* ViewNamePin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_String, TEXT("GraphViewName"));
			ViewNamePin->bNotConnectable = true;
			PinsFirst_Name.Emplace(ViewNamePin->PinName.ToString());
		}
		break ;
	case 1://Link
		{
			PinsFirst_Link.Empty();
			UEdGraphPin* ViewFamilyPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UBlueprint::StaticClass(), TEXT("GraphViewFamily"));
			ViewFamilyPin->bNotConnectable = true;
			PinsFirst_Link.Emplace(ViewFamilyPin->PinName.ToString());
		}
		break ;
	default:
		WAITING_MOD_LOG_UK2NODE();
	}
}

TArray<UEdGraphPin*> UK2Node_SpawnGraphNode::GetPinsFirst(bool IncludeSubPins)
{
	TArray<UEdGraphPin*> rr;

	auto f = [&](const TArray<FString>& Array)
	{
		for (auto& Name : Array)
		{
			for (auto Pin : Pins)
			{
				if (Pin->PinName.ToString() == Name)
				{
					rr.Emplace(Pin);
					break ;
				}
			}
		}
		if (IncludeSubPins)
		{
			int32 Num = Array.Num();
			for (int32 Index = 0 ; Index < Num ; ++Index)
			{
				for (auto SubPin : rr[Index]->SubPins)
					rr.Emplace(SubPin);
			}
		}
	};

	switch (IndexValueOfGetViewWay)
	{
	case 0://Name
		{
			f(PinsFirst_Name);
		}
		break ;
	case 1:
		{
			f(PinsFirst_Link);
		}
		break ;
	default:
		WAITING_MOD_LOG_UK2NODE();
	}
	
	return rr;
}

void UK2Node_SpawnGraphNode::UpdateFirstPins()
{
	
	switch (IndexValueOfGetViewWay)
	{
	case 0://Name
		{
			PinsFirst_Name_Ptr = GetPinsFirst();
		}
		break ;
	case 1://Link
		{
			PinsFirst_Link_Ptr = GetPinsFirst();
		}
		break ;
	default:
		WAITING_MOD_LOG_UK2NODE();
	}
}

void UK2Node_SpawnGraphNode::FixUpFirstPins()
{
	TMap<UEdGraphPin*, UEdGraphPin*> NewPinsToOldPins;
	switch (IndexValueOfGetViewWay)
	{
	case 0:
		{
			RestoreSplitPins(PinsFirst_Name_Ptr);
			RewireOldPinsToNewPins(PinsFirst_Name_Ptr, Pins, &NewPinsToOldPins);
			break ;
		}
	case 1:
		{
			RestoreSplitPins(PinsFirst_Link_Ptr);
			RewireOldPinsToNewPins(PinsFirst_Link_Ptr, Pins, &NewPinsToOldPins);
			break ;
		}
	default:
		WAITING_MOD_LOG_UK2NODE();
	}
}

void UK2Node_SpawnGraphNode::UpdateSourceView()
{
	if (IndexOfSourceGraphView != -1)
		AllGraphViewArray::Get().GetAllViews()[IndexOfSourceGraphView]->ChildNodeIndex.RemoveSingle(IndexInAllNodeArray);
	IndexOfSourceGraphView = -1;

	uint8 Find = 0;
	switch (IndexValueOfGetViewWay)
	{
	case 0://Name
		{
			for (auto View : AllGraphViewArray::Get().GetAllViews())
			{
				if (View->GraphViewName == GetViewNamePin()->DefaultValue)
				{
					IndexOfSourceGraphView = View->IndexInViewArray;
					View->ChildNodeIndex.Emplace(IndexInAllNodeArray);
					NodeWaitArray::Get().GetAllNodes().RemoveSingle(IndexInAllNodeArray);
					FindPinChecked(TEXT("RealViewNameHidden"))->DefaultValue = View->FindPinChecked(TEXT("HiddenViewName"))->DefaultValue;
					Find = 1;
					break ;
				}
			}
		}
		break ;
	case 1:
		{
			UBlueprint* Family = Cast<UBlueprint>(GetViewFamilyPin()->DefaultObject);
			if (Family)
			{
				TArray<UEdGraph*> AllGraphs;
				Family->GetAllGraphs(AllGraphs);
				for (auto Graph : AllGraphs)
				{
					for (auto Node : Graph->Nodes)
					{
						if (UK2Node_SpawnGraphView* TargetView = Cast<UK2Node_SpawnGraphView>(Node) ; TargetView)
						{
							if (TargetView->BeCopied == 1)
								continue ;
							TargetView->ChildNodeIndex.Emplace(IndexInAllNodeArray);
							IndexOfSourceGraphView = TargetView->IndexInViewArray;
							NodeWaitArray::Get().GetAllNodes().RemoveSingle(IndexInAllNodeArray);
							FindPinChecked(TEXT("RealViewNameHidden"))->DefaultValue = TargetView->FindPinChecked(TEXT("HiddenViewName"))->DefaultValue;
							Find = 1;
							break ;
						}
					}
					if (Find)
						break ;
				}
			}
		}
		break ;
	default:
		WAITING_MOD_LOG_UK2NODE();
	}
	if (!Find)
	{
		NodeWaitArray::Get().GetAllNodes().AddUnique(IndexInAllNodeArray);
		FindPinChecked(TEXT("RealViewNameHidden"))->DefaultValue.Reset();
	}
}

void UK2Node_SpawnGraphNode::BuildSecondPins(uint8 BuildAll)
{
	int32 Start;
	if (BuildAll)
		Start = 0;
	else
		Start = GetRealSpawnView()->IndexValueOfConstructMethod;
	switch (Start)
	{
	case 0://Names
		{
			PinsSecond_Names.Empty();
			UEdGraphPin* NamesInputPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct, FNamesInputNode::StaticStruct(), TEXT("NamesInput"));
			PinsSecond_Names.Emplace(NamesInputPin->PinName.ToString());
		}
		if (!BuildAll)
			break ;
	case 1:
		{
			PinsSecond_LHCode_G.Empty();
			UEdGraphPin* LHCodeInputPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct, FLHCode_G_Input::StaticStruct(), TEXT("LHCode_G_Input"));
			PinsSecond_LHCode_G.Emplace(LHCodeInputPin->PinName.ToString());
		}
		break ;
	default:
		WAITING_MOD_LOG_UK2NODE();
	}
}

TArray<UEdGraphPin*> UK2Node_SpawnGraphNode::GetPinsSecond(bool IncludeSubPins)
{
	TArray<UEdGraphPin*> rr;
	std::function<void(TArray<FString>&)> f = [&](TArray<FString>& Array)
	{
		for (auto& Name : Array)
		{
			for (auto Pin : Pins)
			{
				if (Pin->PinName.ToString() == Name)
				{
					rr.Emplace(Pin);
					break ;
				}
			}
		}
		if (IncludeSubPins)
		{
			int32 NumPins = rr.Num();
			for (int32 Index = 0 ; Index < NumPins ; ++Index)
			{
				for (auto SubPin : rr[Index]->SubPins)
					rr.Emplace(SubPin);
			}
		}
	};

	switch (GetRealSpawnView()->IndexValueOfConstructMethod)
	{
	case 0://Names
		{
			f(PinsSecond_Names);
		}
		break ;
	case 1:
		{
			f(PinsSecond_LHCode_G);
		}
		break ;
	default:
		WAITING_MOD_LOG_UK2NODE();
	}
	return rr;
}

void UK2Node_SpawnGraphNode::UpdateSecondPinsBySourceView()
{
	switch (GetRealSpawnView()->IndexValueOfConstructMethod)
	{
	case 0:
		{
			PinsSecond_Names_Ptr = GetPinsSecond();
		}
		break ;
	case 1:
		{
			PinsSecond_LHCode_G_Ptr = GetPinsSecond();
		}
		break ;
	default:
		WAITING_MOD_LOG_UK2NODE();
	}
}

void UK2Node_SpawnGraphNode::UpdateSecondPinsNotBySourceView(uint8 IncludeSubPins)
{
	FString TryName = PinsSecond_Names[0];
	std::function<TArray<UEdGraphPin*>(TArray<FString>&)> f = [&](TArray<FString>& Array)
	{
		TArray<UEdGraphPin*> rr;
		for (auto& Name : Array)
		{
			for (auto Pin : Pins)
			{
				if (Pin->PinName.ToString() == Name)
				{
					rr.Emplace(Pin);
					break ;
				}
			}
		}
		if (IncludeSubPins)
		{
			int32 NumPins = rr.Num();
			for (int32 Index = 0 ; Index < NumPins ; ++Index)
			{
				for (auto SubPin : rr[Index]->SubPins)
					rr.Emplace(SubPin);
			}
		}
		return rr;
	};
	
	if (FindPin(*TryName) != nullptr)
	{
		PinsSecond_Names_Ptr = f(PinsSecond_Names);
		return ;
	}
	TryName = PinsSecond_LHCode_G[0];
	if (FindPin(*TryName) != nullptr)
	{
		PinsSecond_LHCode_G_Ptr = f(PinsSecond_LHCode_G);
		return ;
	}

	WAITING_MOD_LOG_UK2NODE();
}

void UK2Node_SpawnGraphNode::FixUpSecondPins()
{
	TMap<UEdGraphPin*, UEdGraphPin*> NewPinsToOldPins;
	switch (GetRealSpawnView()->IndexValueOfConstructMethod)
	{
	case 0:
		{
			RestoreSplitPins(PinsSecond_Names_Ptr);
			RewireOldPinsToNewPins(PinsSecond_Names_Ptr, Pins, &NewPinsToOldPins);
		}
		break ;
	case 1:
		{
			RestoreSplitPins(PinsSecond_LHCode_G_Ptr);
			RewireOldPinsToNewPins(PinsSecond_LHCode_G_Ptr, Pins, &NewPinsToOldPins);
		}
		break ;
	default:
		WAITING_MOD_LOG_UK2NODE();
	}
}

void UK2Node_SpawnGraphNode::BreakLinkedToSecondPins()
{
	for (auto Pin : GetPinsSecond())
	{
		for (auto LinkedPin : Pin->LinkedTo)
		{
			LinkedPin->LinkedTo.RemoveSingle(Pin);
			LinkedPin->GetOwningNode()->GetGraph()->NotifyNodeChanged(LinkedPin->GetOwningNode());
		}
	}
}

void UK2Node_SpawnGraphNode::BreakLinkedToAllPins()
{
	for (auto Pin : Pins)
	{
		for (auto LinkedPin : Pin->LinkedTo)
		{
			LinkedPin->LinkedTo.RemoveSingle(Pin);
			LinkedPin->GetOwningNode()->GetGraph()->NotifyNodeChanged(LinkedPin->GetOwningNode());
		}
	}
}

void UK2Node_SpawnGraphNode::AllocateDefaultPins()
{
	PinsDefault.Empty();

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	
	UEdGraphPin* ExecPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	PinsDefault.Emplace(ExecPin->PinName.ToString());

	UEdGraphPin* ThenPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
	PinsDefault.Emplace(ThenPin->PinName.ToString());

	UEnum* GetViewWay = StaticEnum<NAGetGraphViewWay::EGetGraphViewWay>();
	UEdGraphPin* GetViewWayPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Byte, GetViewWay, TEXT("GetGraphViewWay"));
	PinsDefault.Emplace(GetViewWayPin->PinName.ToString());
	GetViewWayPin->DefaultValue = GetViewWay->GetNameStringByIndex(IndexValueOfGetViewWay);
	GetViewWayPin->bNotConnectable = true;

	UEdGraphPin* AutoBuildPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Boolean, TEXT("AutoBuild"));
	PinsDefault.Emplace(AutoBuildPin->PinName.ToString());
	AutoBuildPin->bHidden = true;
	Schema->SetPinAutogeneratedDefaultValue(AutoBuildPin, AutoBuild ? TEXT("true") : TEXT("false"));

	UEdGraphPin* GraphNodePin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Object, UGraphNode::StaticClass(), TEXT("GraphNode"));
	PinsDefault.Emplace(GraphNodePin->PinName.ToString());

	UEdGraphPin* RealViewNameHiddenPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_String, TEXT("RealViewNameHidden"));
	PinsDefault.Emplace(RealViewNameHiddenPin->PinName.ToString());
	RealViewNameHiddenPin->bHidden = true;
}

void UK2Node_SpawnGraphNode::PostPlacedNewNode()
{
	Super::PostPlacedNewNode();
	BuildFirstPins();
	AllGraphNodeArray::Get().AddNewNode(this);
}

void UK2Node_SpawnGraphNode::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	if (Pin == FindPinChecked(TEXT("GetGraphViewWay")))
	{
		Modify();
		UpdateDefaultPins();
		UpdateFirstPins();
		if (IndexOfSourceGraphView != -1)
			UpdateSecondPinsBySourceView();
		BreakLinkedToAllPins();
		IndexValueOfGetViewWay = StaticEnum<NAGetGraphViewWay::EGetGraphViewWay>()->GetIndexByNameString(Pin->DefaultValue);
		ErrorMsg.Reset();
		Pins.Reset();
		AllocateDefaultPins();
		FixUpDefaultPins();
		BuildFirstPins();
		FixUpFirstPins();
		UpdateSourceView();
		if (IndexOfSourceGraphView != -1)
		{
			BuildSecondPins();
			FixUpSecondPins();
		}
		GetGraph()->NotifyNodeChanged(this);
		return ;
	}
	if (Pin == GetViewNamePin() || Pin == GetViewFamilyPin())
	{
		Modify();
		UpdateDefaultPins();
		UpdateFirstPins();
		if (IndexOfSourceGraphView != -1)
		{
			UpdateSecondPinsBySourceView();
		}
		BreakLinkedToAllPins();
		ErrorMsg.Reset();
		Pins.Reset();
		AllocateDefaultPins();
		FixUpDefaultPins();
		BuildFirstPins();
		FixUpFirstPins();
		UpdateSourceView();
		if (IndexOfSourceGraphView != -1)
		{
			BuildSecondPins();
			FixUpSecondPins();
		}
		GetGraph()->NotifyNodeChanged(this);
		return ;
	}
}

void UK2Node_SpawnGraphNode::DestroyNode()
{
	NodeWaitArray::Get().GetAllNodes().RemoveSingle(IndexInAllNodeArray);
	AllGraphNodeArray::Get().RemoveNode(this);
	AllGraphNodeArray::Get().UpdateAllNodeIndex(IndexInAllNodeArray);
	if (IndexOfSourceGraphView != -1)
	{
		GetRealSpawnView()->ChildNodeIndex.RemoveSingle(IndexInAllNodeArray);
	}
	AllGraphViewArray::Get().UpdateChildIndexForParent(IndexInAllNodeArray);
	Super::DestroyNode();
}

void UK2Node_SpawnGraphNode::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	if (IndexOfSourceGraphView == -1)[[unlikely]]
	{
		UK2Node_CallFunction* NonFun = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		NonFun->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFunctionTools_GraphWeaver, NonFunction), UFunctionTools_GraphWeaver::StaticClass());
		NonFun->AllocateDefaultPins();

		CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *NonFun->GetExecPin());
		CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *NonFun->GetThenPin());
		BreakAllNodeLinks();
		return ;
	}
	
	UK2Node_CallFunction* CreateNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CreateNode->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFunctionTools_GraphWeaver, CreateGraphNode_NotManuallyCall), UFunctionTools_GraphWeaver::StaticClass());
	CreateNode->AllocateDefaultPins();

	UK2Node_CallFunction* SetNodeOuter = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	SetNodeOuter->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFunctionTools_GraphWeaver, SetNodeOuter_NotManuallyCall), UFunctionTools_GraphWeaver::StaticClass());
	SetNodeOuter->AllocateDefaultPins();

	UK2Node_Self* SelfNode = CompilerContext.SpawnIntermediateNode<UK2Node_Self>(this, SourceGraph);
	SelfNode->AllocateDefaultPins();

	UK2Node_CallFunction* SetRealView = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	SetRealView->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFunctionTools_GraphWeaver, SetRealSourceViewForNode_NotManuallyCall), UFunctionTools_GraphWeaver::StaticClass());
	SetRealView->AllocateDefaultPins();

	UK2Node_CallFunction* CallProcessInform = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallProcessInform->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFunctionTools_GraphWeaver, CallProcessInformOrNot_NotManuallyCall), UFunctionTools_GraphWeaver::StaticClass());
	CallProcessInform->AllocateDefaultPins();

	CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *CreateNode->GetExecPin());
	CreateNode->GetThenPin()->MakeLinkTo(SetNodeOuter->GetExecPin());
	CreateNode->GetReturnValuePin()->MakeLinkTo(SetNodeOuter->FindPinChecked(TEXT("Target")));
	SelfNode->FindPinChecked(TEXT("Self"))->MakeLinkTo(SetNodeOuter->FindPinChecked(TEXT("Outer")));
	SetNodeOuter->GetThenPin()->MakeLinkTo(SetRealView->GetExecPin());
	SetNodeOuter->GetReturnValuePin()->MakeLinkTo(SetRealView->FindPinChecked(TEXT("Node")));
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("RealViewNameHidden")), *SetRealView->FindPinChecked(TEXT("ViewName")));

	switch (GetRealSpawnView()->IndexValueOfConstructMethod)
	{
	case 0:
		{
			UK2Node_CallFunction* ModNamesInput = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
			ModNamesInput->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFunctionTools_GraphWeaver, ModNamesInput_NotManuallyCall), UFunctionTools_GraphWeaver::StaticClass());
			ModNamesInput->AllocateDefaultPins();

			SetRealView->GetThenPin()->MakeLinkTo(ModNamesInput->GetExecPin());
			SetRealView->GetReturnValuePin()->MakeLinkTo(ModNamesInput->FindPinChecked(TEXT("Target")));

			UEdGraphPin* NamesInputPin = FindPinChecked(TEXT("NamesInput"));
			if (NamesInputPin->SubPins.Num() > 0)
			{
				UK2Node_MakeStruct* NamesInputStruct = CompilerContext.SpawnIntermediateNode<UK2Node_MakeStruct>(this, SourceGraph);
				NamesInputStruct->StructType = FNamesInputNode::StaticStruct();
				NamesInputStruct->bMadeAfterOverridePinRemoval = true;
				NamesInputStruct->AllocateDefaultPins();

				CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("NamesInput_SelfName")),
					*NamesInputStruct->FindPinChecked(TEXT("SelfName")));
				CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("NamesInput_ParentNodeNames")),
					*NamesInputStruct->FindPinChecked(TEXT("ParentNodeNames")));
				CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("NamesInput_BroNames")),
					*NamesInputStruct->FindPinChecked(TEXT("BroNames")));

				NamesInputStruct->FindPinChecked(TEXT("NamesInputNode"))->MakeLinkTo(ModNamesInput->FindPinChecked(TEXT("Names")));
			}
			else
				CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("NamesInput")), *ModNamesInput->FindPinChecked(TEXT("Names")));

			ModNamesInput->GetThenPin()->MakeLinkTo(CallProcessInform->GetExecPin());
			ModNamesInput->GetReturnValuePin()->MakeLinkTo(CallProcessInform->FindPinChecked(TEXT("Target")));
		}
		break ;
	case 1:
		{
			UK2Node_CallFunction* ModLHCodeInput = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
			ModLHCodeInput->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFunctionTools_GraphWeaver, ModLHCodeInput_NotManuallyCall), UFunctionTools_GraphWeaver::StaticClass());
			ModLHCodeInput->AllocateDefaultPins();

			SetRealView->GetThenPin()->MakeLinkTo(ModLHCodeInput->GetExecPin());
			SetRealView->GetReturnValuePin()->MakeLinkTo(ModLHCodeInput->FindPinChecked(TEXT("Target")));

			UEdGraphPin* LHCodeInputPin = FindPinChecked(TEXT("LHCode_G_Input"));
			if (LHCodeInputPin->SubPins.Num() > 0)
			{
				UK2Node_MakeStruct* LHCodeStruct = CompilerContext.SpawnIntermediateNode<UK2Node_MakeStruct>(this, SourceGraph);
				LHCodeStruct->StructType = FLHCode_G_Input::StaticStruct();
				LHCodeStruct->bMadeAfterOverridePinRemoval = true;
				LHCodeStruct->AllocateDefaultPins();

				CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("LHCode_G_Input_SelfId")),
					 *LHCodeStruct->FindPinChecked(TEXT("SelfId")));
				CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("LHCode_G_Input_ParentCodes")),
					 *LHCodeStruct->FindPinChecked(TEXT("ParentCodes")));
				CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("LHCode_G_Input_BrotherCodes")),
					 *LHCodeStruct->FindPinChecked(TEXT("BrotherCode")));

				LHCodeStruct->FindPinChecked(TEXT("LHCode_G_Input"))->MakeLinkTo(ModLHCodeInput->FindPinChecked(TEXT("LHCode")));
			}
			else
				CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("LHCode_G_Input")), *ModLHCodeInput->FindPinChecked(TEXT("LHCode")));

			ModLHCodeInput->GetThenPin()->MakeLinkTo(CallProcessInform->GetExecPin());
			ModLHCodeInput->GetReturnValuePin()->MakeLinkTo(CallProcessInform->FindPinChecked(TEXT("Target")));
		}
		break ;
		
	default:
		WAITING_MOD_LOG();
	}
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("AutoBuild")), *CallProcessInform->FindPinChecked(TEXT("AutoBuild")));
	CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *CallProcessInform->GetThenPin());
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("GraphNode")), *CallProcessInform->GetReturnValuePin());

 	BreakAllNodeLinks();
}

void UK2Node_SpawnGraphNode::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UK2Node_SpawnGraphNode, AutoBuild))
	{
		// 查找 AutoBuild 引脚并更新其默认值
		if (UEdGraphPin* AutoBuildPin = FindPin(TEXT("AutoBuild")))
		{
			AutoBuildPin->DefaultValue = AutoBuild ? TEXT("true") : TEXT("false");
		}
	}
}

void UK2Node_SpawnGraphNode::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	if (IndexOfSourceGraphView == -1)
	{
		// 抛出编译错误，@@会被替换为节点引用，可在编辑器中高亮
		MessageLog.Error(
			*NSLOCTEXT("SourceGraphViewMissing", "SourceGraphViewError","@@: 'SourceGraphView' cannot be null. Please specify a valid GraphView object.").ToString(),
			this  // 传入节点指针用于错误定位
		);
	}
}

void UK2Node_SpawnGraphNode::GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (UEdGraphPin* NamesPin = FindPin(TEXT("NamesInput")) ; NamesPin)
	{
		Schema->ConstructBasicPinTooltip(*NamesPin,
		NSLOCTEXT("NodePinText", "NamesInput", "For nodes that are siblings of each other, you only need to declare the sibling relationship in one direction—not both.\n"
										 "For example, if 'A1' and 'B1' are siblings, it is sufficient for either 'A1' to declare 'B1' as a sibling or for 'B1' to declare 'A1' as a sibling.\n"
										 "Do not have both 'A1' and 'B1' declare each other as siblings simultaneously, as this will cause relationship ambiguity or conflicts."),
			NamesPin->PinToolTip);
	}
	
	if (UEdGraphPin* NamesBroNamesPin = FindPin(TEXT("NamesInput_BroNames")); NamesBroNamesPin)
	{
		Schema->ConstructBasicPinTooltip(*NamesBroNamesPin,
		NSLOCTEXT("NodePinText", "NamesInput", "For nodes that are siblings of each other, you only need to declare the sibling relationship in one direction—not both.\n"
										 "For example, if 'A1' and 'B1' are siblings, it is sufficient for either 'A1' to declare 'B1' as a sibling or for 'B1' to declare 'A1' as a sibling.\n"
										 "Do not have both 'A1' and 'B1' declare each other as siblings simultaneously, as this will cause relationship ambiguity or conflicts."),
			NamesBroNamesPin->PinToolTip);
	}

	if (UEdGraphPin* GetViewWayPin = FindPinChecked(TEXT("GetGraphViewWay")))
	{
		Schema->ConstructBasicPinTooltip(*GetViewWayPin,
		NSLOCTEXT("NodePinText", "GetViewWay", "The method used to locate the 'SpawnGraphView'.\n "
															"Note: If the value is set to 'Link', it will only find the first 'SpawnGraphView' in the corresponding Blueprint. \n"
															"Therefore, if your Blueprint contains multiple 'SpawnGraphView' nodes, please switch to using the 'Name' method for identification instead."),
			GetViewWayPin->PinToolTip);
	}
	
	Super::GetPinHoverText(Pin, HoverTextOut);
}

void UK2Node_SpawnGraphNode::PostPasteNode()
{
	BeCopied = 1;
}


void UK2Node_SpawnGraphNode::ReconstructNode()
{
	if (BeCopied == 0)
	{
		TArray<UEdGraphPin*> OldPins(Pins);
		Modify();
		ErrorMsg.Reset();
		Pins.Reset();
		AllocateDefaultPins();
		BuildFirstPins();
		BuildSecondPins(true);
		RestoreSplitPins(OldPins);
		TMap<UEdGraphPin*, UEdGraphPin*> NewPinsToOldPins;
		RewireOldPinsToNewPins(OldPins, Pins, &NewPinsToOldPins);
		GetGraph()->NotifyNodeChanged(this);
		PostReconstructNode();

		//实际上只有本体在打开引擎或者复制粘贴一个新的节点的时候才会执行这个函数
		if (HasAnyFlags(RF_Transactional))
			AllGraphNodeArray::Get().AddNewNode(this);
		return ;
	}
	if (BeCopied == 1)
	{
		AllGraphNodeArray::Get().AddNewNode(this);
		GetRealSpawnView()->ChildNodeIndex.Emplace(IndexInAllNodeArray);
		PostReconstructNode();
	}
}

void UK2Node_SpawnGraphNode::PostLoad()
{
	Super::PostLoad();
	if (HasAnyFlags(RF_Transient))
		return ;
	UpdateDefaultPins();
	UpdateSourceView();
	if (IndexOfSourceGraphView != -1)
	{
		UpdateFirstPins();
		UpdateSecondPinsBySourceView();
		Pins.Reset();
		AllocateDefaultPins();
		FixUpDefaultPins();
		BuildFirstPins();
		FixUpFirstPins();
		BuildSecondPins();
		FixUpSecondPins();
		GetGraph()->NotifyNodeChanged(this);
	}
	else
	{
		UpdateFirstPins();
		Pins.Reset();
		AllocateDefaultPins();
		FixUpDefaultPins();
		BuildFirstPins();
		FixUpFirstPins();
		GetGraph()->NotifyNodeChanged(this);
	}
}

void UK2Node_SpawnGraphNode::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(ActionKey);
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FText UK2Node_SpawnGraphNode::GetMenuCategory() const
{
	return NSLOCTEXT("PreGraphKN", "MenuCategory", "GraphWeaver");
}

bool UK2Node_SpawnGraphNode::ShouldShowNodeProperties() const
{
	return true;
}

FText UK2Node_SpawnGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("PreGraphNodeKN", "NodeTitleKey", "SpawnGraphNode");
}


FSlateIcon UK2Node_SpawnGraphNode::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon(FAppStyle::GetAppStyleSetName(), "GraphEditor.SpawnActor_16x");
	return Icon;
}

FText UK2Node_SpawnGraphNode::GetTooltipText() const
{
	return NSLOCTEXT("PreGraphNodeKN", "GetTooltipText", "Nodes specifically designed for generating GraphView\n "
	"Warning! Do NOT use 'Ctrl + Z' to undo operations for this blueprint node. Since this blueprint involves communication between different blueprints, \n"
	"using this undo method will most likely cause chaos in the entire plugin functionality and result in malfunctions.");
}

UK2Node_SpawnGraphNode::UK2Node_SpawnGraphNode(const FObjectInitializer&)
{
	IndexOfSourceGraphView = -1;
	IndexValueOfGetViewWay = 0;
	AutoBuild = true;
	IndexInAllNodeArray = -1;
	BeCopied = 0;
}





#undef LLOCTEXT_NAMESPACE


void NodeWaitArray::AddWaitNode(UK2Node_SpawnGraphNode* Target)
{
	if (!Target)
		return ;
	FScopeLock Lock(&ArrayMutex);
	NodeArray_Wait.AddUnique(Target->IndexInAllNodeArray);
}

void NodeWaitArray::RemoveWaitNode(UK2Node_SpawnGraphNode* Target)
{
	if (!Target)
		return;
	FScopeLock Lock(&ArrayMutex);
	NodeArray_Wait.RemoveSingleSwap(Target->IndexInAllNodeArray);
}

auto NodeWaitArray::GetAllNodes() -> TArray<int32>&
{
	return NodeArray_Wait;
}

NodeWaitArray::NodeWaitArray()
{
	NodeArray_Wait.Reserve(10);
}

NodeWaitArray::~NodeWaitArray()
{
	NodeArray_Wait.Empty();
}

int32 AllGraphNodeArray::AddNewNode(UK2Node_SpawnGraphNode* NewNode)
{
	FScopeLock Lock(&ArrayMutex);
	
	int32 Index = Nodes.Emplace(NewNode);
	NewNode->IndexInAllNodeArray = Index;
	return Index;
}

void AllGraphNodeArray::RemoveNode(int32 Index)
{
	if (!(Index >= 0 && Index < Get().GetNodes().Num()))[[unlikely]]
	{
		WAITING_MOD_LOG_UK2NODE();
		check(0);//阻止后续运行
		return ;
	}
	FScopeLock Lock(&ArrayMutex);
	Nodes.RemoveAt(Index);
}

void AllGraphNodeArray::RemoveNode(UK2Node_SpawnGraphNode* Target)
{
	if (!IsValid(Target))[[unlikely]]
		return ;

	FScopeLock Lock(&ArrayMutex);
	for (int32 Index = Nodes.Num() - 1; Index >= 0; --Index)
	{
		if (Nodes[Index].Get() == Target)
		{
			Nodes.RemoveAt(Index);
			return;
		}
	}
}

void AllGraphNodeArray::UpdateAllNodeIndex(int32 StartIndex)
{
	int32 Index = -1;
	auto& Array = Get().GetNodes();
	if (StartIndex == -1)
	{
		for (auto Node : Array)
		{
			Index++;
			Node.Get()->IndexInAllNodeArray = Index;
		}
		return ;
	}

	for ( Index = StartIndex; Index < Array.Num(); ++Index)
	{
		Array[Index].Get()->IndexInAllNodeArray = Index;
	}
}

void AllGraphNodeArray::UpdateParentIndexForNode(int32 RemovedParentIndex)
{
	for (auto Node : Nodes)
	{
		auto RealNode = Node.Get();
		if (RealNode->IndexOfSourceGraphView > RemovedParentIndex)
			RealNode->IndexOfSourceGraphView -= 1;
	}
}


AllGraphNodeArray::AllGraphNodeArray()
{
	Nodes.Empty();
}




//////////////////////////////////////////////////////////////////////////////////////////
#define LOCTEXT_NAMESPACE "FunctionsForGraphWeaver"


class FKCHandler_ObtainDesRefFromNode_Inner : public FNodeHandlingFunctor
{
public:
	FKCHandler_ObtainDesRefFromNode_Inner(FKismetCompilerContext& Context)
		:FNodeHandlingFunctor(Context)
	{
		
	}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		FNodeHandlingFunctor::RegisterNets(Context, Node);//注册Index_h

		FBPTerminal* ResultTer = new FBPTerminal();
		UEdGraphPin* ResultPin = Node->Pins[2];
		ResultTer->CopyFromPin(ResultPin, Context.NetNameMap->MakeValidName(ResultPin));
		Context.NetMap.Add(ResultPin, ResultTer);
		Context.InlineGeneratedValues.Add(ResultTer);
	}

	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		FBlueprintCompiledStatement* GetRefStatement = new FBlueprintCompiledStatement();

		GetRefStatement->Type = KCST_ArrayGetByRef;

		UEdGraphPin* ArrayNet = FEdGraphUtilities::GetNetFromPin(Node->Pins[0]);
		UEdGraphPin* IndexNet = FEdGraphUtilities::GetNetFromPin(Node->Pins[1]);
		UEdGraphPin* ResultNet = FEdGraphUtilities::GetNetFromPin(Node->Pins[2]);

		FBPTerminal** ArrayTerm = Context.NetMap.Find(ArrayNet);
		FBPTerminal** IndexTerm = Context.NetMap.Find(IndexNet);
		FBPTerminal** ResultTerm = Context.NetMap.Find(ResultNet);

		GetRefStatement->RHS.Add(*ArrayTerm);
		GetRefStatement->RHS.Add(*IndexTerm);

		(*ResultTerm)->InlineGeneratedParameter = GetRefStatement;
	}
};



void UK2Node_ObtainDesRefFromNode_Inner::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* SchemaHelper = GetDefault<UEdGraphSchema_K2>();
	
	FCreatePinParams PinParams;
	PinParams.ContainerType = EPinContainerType::Array;
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct,FGraphNodeDescription::StaticStruct(), TEXT("DesArray_h"), PinParams);

	UEdGraphPin* IndexPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, TEXT("Index_h"));
	SchemaHelper->SetPinAutogeneratedDefaultValue(IndexPin, TEXT("-1"));

	UEdGraphPin* ReturnPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Struct, FGraphNodeDescription::StaticStruct(), TEXT("DesRef_h"));
	ReturnPin->PinType.bIsReference = true;
}

class FNodeHandlingFunctor* UK2Node_ObtainDesRefFromNode_Inner::CreateNodeHandler(
	class FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_ObtainDesRefFromNode_Inner(CompilerContext);
}

FText UK2Node_ObtainDesRefFromNode_Inner::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("UK2NodeName", "ObtainDesRefFromNodeName", "GetDesRef_h");
}

FText UK2Node_ObtainDesRefFromNode_Inner::GetMenuCategory() const
{
	return NSLOCTEXT("PreGraphKN", "MenuCategory", "GraphWeaver");
}

void UK2Node_GetDesRef::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UGraphNode::StaticClass(), TEXT("Source"));

	UEdGraphPin* ResultPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Struct, FGraphNodeDescription::StaticStruct(), TEXT("Des"));
	ResultPin->PinType.bIsReference = true;
}

void UK2Node_GetDesRef::GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const
{
	const UEdGraphSchema_K2* SchemaHelper = GetDefault<UEdGraphSchema_K2>();
	if (UEdGraphPin* SourcePin = Pins[0])
	{
		SchemaHelper->ConstructBasicPinTooltip(*SourcePin,
			NSLOCTEXT("GraphWeaverPinTooltip", "GetDesRefPin", "A 'GraphNode' object is passed here, and we need to get its  \n "
													  "reference to the 'GraphNodeDescription' in 'SourceGraphView'."),
			SourcePin->PinToolTip);
	}
	
	Super::GetPinHoverText(Pin, HoverTextOut);
}

FSlateIcon UK2Node_GetDesRef::GetIconAndTint(FLinearColor& OutColor) const
{
	static const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.PureFunction_16x");
	return Icon;
}

void UK2Node_GetDesRef::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	UK2Node_CallFunction* GetViewAndIndex = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this);
	GetViewAndIndex->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFunctionTools_GraphWeaver, GetViewAndIndexFromNode), UFunctionTools_GraphWeaver::StaticClass());
	GetViewAndIndex->AllocateDefaultPins();

	UK2Node_VariableGet* GetRealNodes = CompilerContext.SpawnIntermediateNode<UK2Node_VariableGet>(this);
	TUniquePtr<FMemberReference> MemberReference_h = MakeUnique<FMemberReference>();
	MemberReference_h->SetExternalMember(TEXT("RealNodes"), UGraphView::StaticClass());
	GetRealNodes->VariableReference = *MemberReference_h.Get();
	GetRealNodes->AllocateDefaultPins();
	CompilerContext.MessageLog.NotifyIntermediateObjectCreation(GetRealNodes, this);

	UK2Node_ObtainDesRefFromNode_Inner* GetDes = CompilerContext.SpawnIntermediateNode<UK2Node_ObtainDesRefFromNode_Inner>(this);
	GetDes->AllocateDefaultPins();
	
	CompilerContext.CopyPinLinksToIntermediate(*FindPinChecked(TEXT("Source")), *GetViewAndIndex->FindPinChecked(TEXT("Target")));
	GetViewAndIndex->GetReturnValuePin()->MakeLinkTo(GetRealNodes->FindPinChecked(UEdGraphSchema_K2::PN_Self));//GetSourceView
	GetViewAndIndex->FindPinChecked(TEXT("Index"))->MakeLinkTo(GetDes->FindPinChecked(TEXT("Index_h")));
	GetRealNodes->FindPinChecked(TEXT("RealNodes"))->MakeLinkTo(GetDes->FindPinChecked(TEXT("DesArray_h")));
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("Des")), *GetDes->FindPinChecked(TEXT("DesRef_h")));

	BreakAllNodeLinks();
}

void UK2Node_GetDesRef::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	if (SourcePinLinked == 0)
	{
		MessageLog.Error(
			*NSLOCTEXT("GraphWeaverErrorMessage", "GetDesRefError", "@@: 'Source' must be connected to a Blueprint node and cannot be empty.").ToString(),
			this);
	}
}

void UK2Node_GetDesRef::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);

	if (Pin == Pins[0])
	{
		SourcePinLinked = Pins[0]->LinkedTo.Num();
	}
}

FText UK2Node_GetDesRef::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("UK2NodeName", "ObtainDesRefFromNodeName", "GetDesRef");
}

FText UK2Node_GetDesRef::GetMenuCategory() const
{
	return NSLOCTEXT("PreGraphKN", "MenuCategory", "GraphWeaver");
}

void UK2Node_GetDesRef::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(ActionKey);
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

UK2Node_GetDesRef::UK2Node_GetDesRef(const FObjectInitializer&)
{
	SourcePinLinked = 0;
}





void UK2Node_ValidateRankingConsistency::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UGraphView::StaticClass(), TEXT("GraphView"));
	
	FCreatePinParams PinParams;
	PinParams.ContainerType = EPinContainerType::Array;
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, TEXT("IgnoredNodeIndices"), PinParams);

	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Boolean, TEXT("Consistency"));
}

void UK2Node_ValidateRankingConsistency::ExpandNode(class FKismetCompilerContext& CompilerContext,
	UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	UK2Node_CallFunction* CallCheckFunc = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallCheckFunc->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UGraphView, ValidateRankingConsistency_Inner), UGraphView::StaticClass());
	CallCheckFunc->AllocateDefaultPins();

	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("GraphView")), *CallCheckFunc->FindPinChecked(TEXT("GraphView")));
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("Consistency")), *CallCheckFunc->GetReturnValuePin());

	if (FindPinChecked(TEXT("IgnoredNodeIndices"))->LinkedTo.Num() > 0)
		CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("IgnoredNodeIndices")),
			*CallCheckFunc->FindPinChecked(TEXT("IgnoredNodeIndices")));
	else
	{
		UK2Node_CallFunction* GetEmptyIntArray = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		GetEmptyIntArray->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFunctionTools_GraphWeaver, GetEmptyIntArray),
			UFunctionTools_GraphWeaver::StaticClass());
		GetEmptyIntArray->AllocateDefaultPins();

		GetEmptyIntArray->GetReturnValuePin()->MakeLinkTo(CallCheckFunc->FindPinChecked(TEXT("IgnoredNodeIndices")));
	}

	BreakAllNodeLinks();
}

FText UK2Node_ValidateRankingConsistency::GetTooltipText() const
{
	return NSLOCTEXT("PreGraphNodeKN", "GetTooltipText_CHeckRanking", "Check whether the 'Ranking' of each element in 'RealNodes' is correct within the family \n "
	"relationships. Manually deleting certain elements from 'RealNodes' (not recommended) or calling \n "
	"the 'RemoveNodes' function may cause incorrect 'Ranking'. See the 'RemoveNodes' function for details.");
}

FText UK2Node_ValidateRankingConsistency::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("UK2NodeName", "CheckIndexIsRightName", "ValidateRankingConsistency");
}

FText UK2Node_ValidateRankingConsistency::GetMenuCategory() const
{
	return NSLOCTEXT("PreGraphKN", "MenuCategory", "GraphWeaver");
}

void UK2Node_ValidateRankingConsistency::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(ActionKey);
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}



void UK2Node_ValidateRankingConsistencyLight::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UGraphView::StaticClass(), TEXT("GraphView"));
	
	FCreatePinParams PinParams;
	PinParams.ContainerType = EPinContainerType::Array;
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, TEXT("IgnoredNodeIndices"), PinParams);

	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Boolean, TEXT("Consistency"));
}

void UK2Node_ValidateRankingConsistencyLight::ExpandNode(class FKismetCompilerContext& CompilerContext,
	UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	UK2Node_CallFunction* CallCheckFunc = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallCheckFunc->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UGraphView, ValidateRankingConsistencyLight_Inner), UGraphView::StaticClass());
	CallCheckFunc->AllocateDefaultPins();

	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("GraphView")), *CallCheckFunc->FindPinChecked(TEXT("GraphView")));
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("Consistency")), *CallCheckFunc->GetReturnValuePin());

	if (FindPinChecked(TEXT("IgnoredNodeIndices"))->LinkedTo.Num() > 0)
		CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("IgnoredNodeIndices")),
			*CallCheckFunc->FindPinChecked(TEXT("IgnoredNodeIndices")));
	else
	{
		UK2Node_CallFunction* GetEmptyIntArray = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		GetEmptyIntArray->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UFunctionTools_GraphWeaver, GetEmptyIntArray),
			UFunctionTools_GraphWeaver::StaticClass());
		GetEmptyIntArray->AllocateDefaultPins();

		GetEmptyIntArray->GetReturnValuePin()->MakeLinkTo(CallCheckFunc->FindPinChecked(TEXT("IgnoredNodeIndices")));
	}

	BreakAllNodeLinks();
}

FText UK2Node_ValidateRankingConsistencyLight::GetTooltipText() const
{
	return NSLOCTEXT("PreGraphNodeKN", "GetTooltipText_CHeckRanking", "Check whether the 'Ranking' of each element in 'RealNodes' is correct within the family \n "
	"relationships. Manually deleting certain elements from 'RealNodes' (not recommended) or calling \n "
	"the 'RemoveNodes' function may cause incorrect 'Ranking'. See the 'RemoveNodes' function for details.\n "
	"This function does not print logs. If you only want to check whether the 'Ranking' is entirely correct, this function is more suitable than \n "
	"'ValidateRankingConsistency'.");
}

FText UK2Node_ValidateRankingConsistencyLight::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("UK2NodeName", "CheckIndexIsRightLightName", "ValidateRankingConsistencyLight");
}

FText UK2Node_ValidateRankingConsistencyLight::GetMenuCategory() const
{
	return NSLOCTEXT("PreGraphKN", "MenuCategory", "GraphWeaver");
}

void UK2Node_ValidateRankingConsistencyLight::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(ActionKey);
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}


#undef LOCTEXT_NAMESPACE



/*
* BlueprintExceptionInfo ExceptionInfo(EBlueprintExceptionType::AbortExecution,
			INVTEXT("execMakeCustomDataEffectData 必须使用Struct类型进行链接")
		);

		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
ContainerPtrToValuePtr
*/

//以下是写这款插件的时候借鉴学习的源代码
//UK2Node_GetDataTableRow
//UK2Node_SpawnActorFromClass
//UK2Node_IfThenElse
//PostReconstructNode
//UK2Node_ConstructObjectFromClass
//SpawnActor()
//FCriticalSection
//UK2Node::ReallocatePinsDuringReconstruction
//UEdGraphNode::PostLoadSubobjects()
//UK2Node_Select
//UK2Node_MakeStruct
//UObject::PostEditChangeProperty
//UEdGraphNode::ValidateNodeDuringCompilation

//UEdGraphNode::PostPasteNode
//UK2Node::DoPinsMatchForReconstruction
//UK2Node::GetNodeRefreshPriority
//UEdGraphNode::PinTypeChanged
//UK2Node::ReconstructNode()
//UEdGraphSchema_K2::SplitPin()
//UK2Node_Variable
//struct FMemberReference
//CompileClassLayout
//UK2Node_GetArrayItem