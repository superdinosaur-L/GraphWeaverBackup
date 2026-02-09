// Copyright 2026 RainButterfly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <coroutine>
#include "K2Node.h"
#include "Engine/TimerHandle.h"
#include <atomic>
#include "UObject/StrongObjectPtrTemplates.h"
#include "Windows/WindowsCriticalSection.h"
#include "K2NodeForGraph.generated.h"

class UK2Node_CallFunction;
class UK2Node_SpawnGraphView;
class UK2Node_SpawnGraphNode;
struct FNamesInputNode;
struct FLHCode_G_Input;
class UEdGraphPin;


#define EMPTY_LOG_UK2NODE() UE_LOG(LogTemp, Error, TEXT("        "))
#define WAITING_MOD_LOG_UK2NODE() \
    do { \
        UE_LOG(LogTemp, Error, TEXT("The code here is incomplete and needs to be fixed immediately.")); \
        UE_LOG(LogTemp, Error, TEXT("File: %s, Func: %s, Line: %d"), TEXT(__FILE__), TEXT(__FUNCTION__), __LINE__); \
        EMPTY_LOG_UK2NODE() \
    } while(0)

class FEditorLinkerTask
{
public:
	struct promise_type
	{
		std::suspend_never initial_suspend() noexcept;
		std::suspend_never final_suspend() noexcept;
		FEditorLinkerTask get_return_object() noexcept;
		void return_void() noexcept;
		void unhandled_exception() noexcept;
	};
};

class FPollAwaiter
{
public:
	// 构造函数：超时秒数 + 监控对象（用于防止UObject被GC）
	FPollAwaiter(float InTimeout, TWeakObjectPtr<UK2Node_SpawnGraphView> InOwner)
		: TimeoutSeconds(InTimeout), WeakOwner(InOwner) {}
    
	// 禁止拷贝（避免生命周期混乱）
	FPollAwaiter(const FPollAwaiter&) = delete;
	FPollAwaiter& operator=(const FPollAwaiter&) = delete;
	~FPollAwaiter() { bIsActive = false; }

public:
	double TimeoutSeconds;
	TWeakObjectPtr<UK2Node_SpawnGraphView> WeakOwner;
	std::atomic<bool> bIsActive{true};

	// 所有轮询状态数据（通过shared_ptr共享）
	struct PollState
	{
		std::coroutine_handle<> Handle;
		float TimeoutSeconds;
		TWeakObjectPtr<UK2Node_SpawnGraphView> WeakOwner;
		std::atomic<bool> bIsActive{true};
		float ElapsedTime = 0.0f;
		FTimerHandle TimerHandle;
	};

	static bool IsConditionMet(std::shared_ptr<PollState> State);
	// 静态轮询函数（不捕获this，避免悬挂指针）
	static void StartPolling(std::shared_ptr<PollState> State, float ElapsedTime);

	bool await_ready() noexcept;
	void await_suspend(std::coroutine_handle<> handle) noexcept;
	void await_resume() noexcept;
};


//存放所有的UK2Node_SpawnGraphView
class  AllGraphViewArray
{
public:
	static AllGraphViewArray& Get()
	{
		static AllGraphViewArray Instance;  // C++11保证线程安全
		return Instance;
	}
	
	void AddView(UK2Node_SpawnGraphView* View);

	//不涉及其余View重排序，需要手动调用
	void RemoveView(UK2Node_SpawnGraphView* View);

	//在Views里面通过GraphViewName去查找对应的SpawnGraphView
	UK2Node_SpawnGraphView* FindViewByCommonName(const FString& Name) const;
	
	TArray<TStrongObjectPtr<UK2Node_SpawnGraphView>>& GetAllViews();

	//当删除View的时候，需要更新每个View在数组里面的下标
	void UpdateAllViewIndex();

	//当删除一个Node的时候需要更新所有的View的Nodes下标,对应的是AllF=GraphNodeArray里面的UpdateParentIndexForChild
	void UpdateChildIndexForParent(int32 RemovedIndex);

private:
	AllGraphViewArray(); 

	//阻止GC

	////////////////////////////////////////////////////////////////
	TArray<TStrongObjectPtr<UK2Node_SpawnGraphView>> Views;

	// 线程安全锁
	mutable FCriticalSection ArrayMutex;
};



//在ReconstructNode,PostPlaceNewNode,PostPasteNode里面加入到SpawnGraphViewArray里面
UCLASS(meta = (Keywords = "Spawn Create Graph"))
class UK2NODETOOLS_API UK2Node_SpawnGraphView : public UK2Node
{
	GENERATED_BODY()

public:
	////当前节点是否是复制粘贴出来的。不能使用UPROPERTY进行修饰(给拷贝体使用)
	UPROPERTY()
	uint8 BeCopied;

	UPROPERTY()
	uint8 NameSameAsOtherView;
	
	UPROPERTY()
	FString GraphViewName;

	UPROPERTY()
	int32 NumOfDefaultPins;

	//0代表Names,1代表LHCode_G
	UPROPERTY()
	int32 IndexValueOfConstructMethod;

	UPROPERTY()
	int32 IndexValueOfWayToDealSameNode;

	UPROPERTY()
	int32 IndexInViewArray;
	
	UPROPERTY()
	TArray<int32> ChildNodeIndex;

	UPROPERTY()
	TArray<int32> IndexOfPinsFirst_Names;
	//包含SubPins。只包含第一阶段
	TArray<UEdGraphPin*> PinsFirst_Names;

	UPROPERTY(EditAnywhere, Category = "GraphWeaver|UK2Node" ,
		meta = (ToolTip = "When you create a 'SpawnGraphNode' and set 'GetGraphViewWay' to 'Link',  and then create a 'SpawnGraphView', the system must wait for the 'SpawnGraphView' to be fully loaded into the Blueprint before it can connect to the corresponding 'SpawnGraphNode'.The 'MaxWaitTime' specifies how long (in seconds) the system will wait for the View to load into the Blueprint.If this value is too large, it may cause stuttering or lag.If your computer has sufficient performance, you can reduce this value.The minimum allowed value is '0.2' seconds."))
	double MaxWaitTime;

public:
	UEdGraphPin* GetEnumPin() const;
	UEdGraphPin* GetNamesConfigPin() const;
	UEdGraphPin* GetReturnValuePin() const;
	UEdGraphPin* GetSelfOwnerPin() const;
	UEdGraphPin* GetViewNamePin() const;
	
	TArray<UK2Node_SpawnGraphNode*> GetRealSpawnNodes();
	//当删除某个ChildNode的时候会导致原本的ChildNodeIndex混乱(被删除的那个ChildNode在UAllSpawnNode里面消失，下标前移),需要根据旧的Children来更新ChildNodeIndex
	void UpdateChildrenNodeIndex(int32 RemovedChildIndex);
	
	TArray<UEdGraphPin*> BuildConfigPinsFirst();

	[[nodiscard]]UClass* GetBlueprintGenerateClass();
	TArray<UEdGraphPin*> GetDefaultPins(bool IncludeSubPins = true);
	//获取第一阶段生成的动态Pins
	TArray<UEdGraphPin*> GetPinsFirst(bool IncludeSubPins = true);
	//更新第一阶段动态生成的Pins
	void UpdatePinsFirst();
	void UpdateGraphViewNameForThisNode(bool NeedRandom = false);

	//下面的两个FindWait会自动处理子节点和父节点的互相指认的逻辑，并且把找到的孩子从Wait数组里面移除
	
	/////////////////////////////////////////////////////////////////////////////////////
	TArray<UK2Node_SpawnGraphNode*> FindWaitNodeByCommonName();
	TArray<UK2Node_SpawnGraphNode*> FindWaitNodeByLink();
	//删除孩子节点里面GetGraphViewWay的方式为Name的节点。需要借助父子关系
	TArray<UK2Node_SpawnGraphNode*> EmptyChildNode_Name();

	/////////////////////////////////////////////////////////
	FEditorLinkerTask DelayLinkChild_Link();

	FString GenerateRandomString(int32 Length);
	
public:
	virtual void AllocateDefaultPins() override;
	virtual void PostReconstructNode() override;
	//复制粘贴新节点的时候新节点会调用这个函数然后调用ReconstructNode。离谱的是，不知道虚幻是怎么设置的，新的节点的数量后缀总是会替换被复制的节点的数量后缀.
	//例如原本有K2Node_SpawnGraphView_2，那么粘贴只会新的节点变成了UK2Node_SpawnGraphView_2，而旧节点却变成了K2Node_SpawnGraphView_3
	virtual void PostPasteNode() override;
	virtual void ReconstructNode() override;
	virtual void PostPlacedNewNode() override;
	//virtual void PostLoad() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual FText GetTooltipText() const override;
	virtual void DestroyNode() override;
	//该函数由拷贝体执行而不是本体执行。不能理解
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	
	virtual void GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const override;
	
	virtual FText GetMenuCategory() const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual bool ShouldShowNodeProperties() const override;
	
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	
	UK2Node_SpawnGraphView(const FObjectInitializer&);
};


//因为这里要找的实际上是UK2Node_SpawnGraphView，而不是真正要找GraphView，而UK2Node只能存在于蓝图里面，所以要找的GraphView目标
//必须由UK2Node_SpawnGraphView生成
UENUM()
namespace NAGetGraphViewWay
{
	enum EGetGraphViewWay : uint8
	{
		Name,
		Link,
	};
}


///////////////////////////////////////////////////////////////////////////////////////////
//因为虚幻底层的代码限制的问题，该类不能在PinDefaultValueChanged里面动态的清空Pin，删除Pin，


///////////////////////////////////////////////////////////////////////////////
UCLASS()
class UK2NODETOOLS_API UK2Node_SpawnGraphNode : public UK2Node
{
	GENERATED_BODY()
public:
	//当前节点是否是复制粘贴出来的。不能使用UPROPERTY进行修饰
	uint8 BeCopied;
	
	//下面的几个int32类型加一个UPROPERTY只是为了让Modify可以回溯

	/////////////////////////////////////////////
	UPROPERTY()
	int32 IndexOfSourceGraphView;

	//在UAllGraphNodeArray里面的Index
	UPROPERTY()
	int32 IndexInAllNodeArray;

	UPROPERTY()
	int32 IndexValueOfGetViewWay;

	//所有的FString类型数组只记录父亲节点,不记录子节点
	
	/////////////////////////////////////////////////////////////////
	UPROPERTY()
	TArray<FString> PinsFirst_Name;
	TArray<UEdGraphPin*> PinsFirst_Name_Ptr;
	UPROPERTY()
	TArray<FString> PinsFirst_Link;
	TArray<UEdGraphPin*> PinsFirst_Link_Ptr;
	
	UPROPERTY()
	TArray<FString> PinsSecond_Names;
	TArray<UEdGraphPin*> PinsSecond_Names_Ptr;
	UPROPERTY()
	TArray<FString> PinsSecond_LHCode_G;
	TArray<UEdGraphPin*> PinsSecond_LHCode_G_Ptr;

	UPROPERTY()
	TArray<FString> PinsDefault;
	TArray<UEdGraphPin*> PinsDefault_Ptr;

	UPROPERTY(EditAnywhere, Category = "GraphWeaver|UK2Node", meta = (ToolTip = "Whether this 'GraphNode' should automatically participate in the construction of the 'GraphView'."))
	bool AutoBuild;
public:
	UEdGraphPin* GetViewNamePin() const;
	UEdGraphPin* GetViewFamilyPin() const;

	UK2Node_SpawnGraphView* GetRealSpawnView();

	TArray<UEdGraphPin*> GetDefaultPins();
	void UpdateDefaultPins();
	void FixUpDefaultPins();
	
	//根据GetGraphViewWay来构建第一阶段的动态Pin
	void BuildFirstPins();
	TArray<UEdGraphPin*> GetPinsFirst(bool IncludeSubPins = true);
	void UpdateFirstPins();
	void FixUpFirstPins();
	
	void UpdateSourceView();

	//以下任何涉及到第二阶段的Pins的函数都必须要求IndexOfSourceGraphView是有效的

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//根据SourceGraphView的ConstructMethodIndexValue来构建第二阶段的Pins。当BuildAll选择true的时候表明需要在不依靠SourceGraphViw的前提下创建所有的有可能的第二阶段的Pin
	void BuildSecondPins(uint8 BuildAll = false);
	TArray<UEdGraphPin*> GetPinsSecond(bool IncludeSubPins = true);
	void UpdateSecondPinsBySourceView();

	//不需要根据SourceGraphView,而是根据当前自身的Pins结合PinsSecond_Names和PinsSecond_LHCode_G来获取当前的第二阶段的Pin.该函数运行速度不如BySourceView
	void UpdateSecondPinsNotBySourceView(uint8 IncludeSubPins = 1);
	void FixUpSecondPins();

	//清除和第二阶段的Pin连接的Pin(第一阶段不允许有任何连接，所以不需要再写一个类似的函数)
	void BreakLinkedToSecondPins();
	void BreakLinkedToAllPins();
	
public:

	virtual void AllocateDefaultPins() override;
	virtual void PostPlacedNewNode() override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void DestroyNode() override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	virtual void GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const override;

	
	virtual void PostPasteNode() override;
	virtual void ReconstructNode() override;
	virtual void PostLoad() override;
	

	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual bool ShouldShowNodeProperties() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual FText GetTooltipText() const override;
	
	UK2Node_SpawnGraphNode(const FObjectInitializer&);
};


//仅用作等待UK2Node_SpawnGraphView后到的情况
class NodeWaitArray
{
public:
	NodeWaitArray(const NodeWaitArray& o) = delete;
	NodeWaitArray(NodeWaitArray&& o) = delete;
	NodeWaitArray& operator=(const NodeWaitArray& o) = delete;

	
	static NodeWaitArray& Get()
	{
		static NodeWaitArray Obj;
		return Obj;
	}

	void AddWaitNode(UK2Node_SpawnGraphNode*);
	void RemoveWaitNode(UK2Node_SpawnGraphNode*);
	auto GetAllNodes()->TArray<int32>&;
private:
	NodeWaitArray();
	~NodeWaitArray();

	TArray<int32> NodeArray_Wait;

	mutable FCriticalSection ArrayMutex;
};


//用来记录所有的UK2Node_SpawnGraphNode蓝图节点
class AllGraphNodeArray
{
public:
	static AllGraphNodeArray& Get()
	{
		static AllGraphNodeArray Obj;
		return Obj;
	}

	TArray<TStrongObjectPtr<UK2Node_SpawnGraphNode>>& GetNodes()
	{
		return Nodes;
	}
	
	int32 AddNewNode(UK2Node_SpawnGraphNode* NewNode);
	//RemoveNode不包含给后面的元素重排序
	void RemoveNode(int32 Index);
	//RemoveNode不包含给后面的元素重排序
	void RemoveNode(UK2Node_SpawnGraphNode* Target);
	//StartIndex表示从当前Index开始重新对IndexInAllNodeArray排序
	void UpdateAllNodeIndex(int32 StartIndex = -1);
	//当删除一个SpawnGraphView的时候需要重新定位在x之后所有的
	void UpdateParentIndexForNode(int32 RemovedParentIndex);
public:
	//阻止GC
	TArray<TStrongObjectPtr<UK2Node_SpawnGraphNode>> Nodes;

	mutable FCriticalSection ArrayMutex;

private:
	AllGraphNodeArray();
};


//下面是一些辅助使用插件的节点

/////////////////////////////////////////////

//真正获取Description引用的节点,封装在内层不给用户展示
UCLASS()
class UK2Node_ObtainDesRefFromNode_Inner : public UK2Node
{
	GENERATED_BODY()
	
public:

	virtual void AllocateDefaultPins() override;
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;

	virtual bool IsNodePure() const override{return true; }
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	//virtual bool ShouldDrawCompact() const override{return true; }
	virtual FText GetMenuCategory() const override;
};


UCLASS()
class UK2Node_GetDesRef : public UK2Node
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 SourcePinLinked;
public:
	virtual void AllocateDefaultPins() override;
	virtual void GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	//这个函数由复制体执行
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	
	virtual bool IsNodePure() const override{return true;}
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetMenuCategory() const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;

	UK2Node_GetDesRef(const FObjectInitializer&);
};





/*
*/




























