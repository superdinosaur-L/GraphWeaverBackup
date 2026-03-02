// Copyright 2026 RainButterfly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "GraphView.generated.h"

class USaveGame;
class UGraphView;
class UGraphNode;
struct FNamesInputNode;
struct FLHCode_G_Input;

UENUM(Blueprintable)
namespace NAGraphConstructErrorCode
{
	enum EErrorCodeForConstructView : uint8
	{
		None,
		NameSameAsRoot,
		BroNameSameAsRoot,
	};

}

//仅作为镜像来保存数据，仅起辅助作用。

///////////////////////////////////////////
USTRUCT(BlueprintType)
struct FNamesInputNodeMirror
{
	GENERATED_BODY()

	//序列化保留,用于后续重构时校验

	///////////////////////
	//The name defined by the user for this node. It can be retrieved via FunctionTools::GetSelfNameFromDescription().
	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|NamesInputNodeMirror")
	FString SelfName = "None";

	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|NamesInputNodeMirror")
	TArray<FString> ParentNodeNames;

	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|NamesInputNodeMirror")
	TArray<FString> BroNames;

	void operator=(const FNamesInputNode& Source);
};


USTRUCT(BlueprintType)
struct FLHCode_G_InputMirror
{
	GENERATED_BODY()

	//序列化保留,用于后续重构时校验

	///////////////////////
	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|LHCodeNodeMirror")
	FString SelfId = "None";

	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|LHCodeNodeMirror")
	TArray<FString> ParentCodes;

	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|LHCodeNodeMirror")
	TArray<FString> BroCodes;

	void operator=(const FLHCode_G_Input& Source);
};

//不建议再对名字进行修改，否则需要修改UK2Node_SpawnGraph的代码

/////////////////////////////////////////////////////////////////////
UENUM(BlueprintType)
namespace NAConstructMethod
{
	enum  EConstructMethod : uint8
	{
		//Prefer using the Names method for construction.If you choose the Names method,
		//it is recommended that the first Precision characters of each node name follow a consistent naming pattern—this can significantly improve construction performance.
		//For example: A1, A2; B1, B2.
		Names,
		//Not recommended. It’s not only prone to input errors that can prevent the graph from being generated correctly,
		//but its runtime performance may even be worse than using 'Names'.
		LHCode_G,
	};
}

//当有相同的GraphNode参与构建的时候，应该采取的策略.
//对于会使用c++代码的人而言,下面的每个选项都包含一个额外的动作:更新指针UGraphNode* SourceGraphNode.这不影响正常使用
UENUM(BlueprintType)
namespace NAWayToDealSameGraphNode
{
	enum  EWayToDealSameGraphNode : uint8
	{
		NothingToDo,
		//Reminds you on the screen and in the TempLog that duplicate nodes have been added to the build.
		//Recommended for reviewing whether you have accidentally added the same node multiple times.
		OnlyWarningSameNode,
	};
}


//不建议再对名字进行修改，否则需要修改UK2Node_SpawnGraph的代码

/////////////////////////////////////////////////////////////////////
USTRUCT(BlueprintType)
struct FNamesConstructConfig
{
	GENERATED_BODY()
	
	// Set this option to false if you want to use the 'Names' construction method but do not want to use rule-based naming, to reduce memory usage;
	// otherwise, set it to true. True will speed up construction but increase memory usage, and the difference will be more noticeable
	// when many 'GraphNode's are involved in building the same 'GraphView'.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GraphWeaver|InputConfiguration", meta = (ExposeOnSpawn = "NamingOfRules"))
	bool NamingOfRules = true;

	//The number of characters at the beginning of a name that defines a family.
	//For example, if you have AA2, AA3; BB2, BB3, the Precision value is 2.
	//If you have AA1 and AB2, the Precision value can be 1 (family character A) or 2 (family characters AA and AB).
	//Note that this option only takes effect when 'ConstructMethod' is set to 'Names' and 'NamingOfRules' is true.
	//If this value exceeds the length of a name, the entire name is copied. If the value is less than 1, it defaults to 1.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GraphWeaver|InputConfiguration", meta = (ExposeOnSpawn = "Precision"))
	int32 Precision = 1;
	
	void operator=(const FNamesConstructConfig& other);
};



//不使用标准的TMap，因为不能对TMap<CHAR,TArray<int32>>使用UPROPERTY修饰，后果:当角色(容器)死亡的时候，变量FirstWordOfName会消亡，导致下一次构建图的时候(采用LHCode_G的方式)构建速度会很慢

////////////////////////////////////////////////////////////////////
USTRUCT(BlueprintType)
struct FGraphViewSimpleMap
{
	GENERATED_BODY()

	//Stores the clan name
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|GraphViewSimpleMap")
	FString ClanName;

	//The Index of each member in the clan within 'RealNodes'
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|GraphViewSimpleMap")
	TArray<int32> Indexs;

	FGraphViewSimpleMap();
	FGraphViewSimpleMap(FString& TargetKey, int32 Index, UGraphView* TargetView);
	~FGraphViewSimpleMap();
};

USTRUCT(BlueprintType)
struct FGraphViewSimplePair
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|GraphViewSimplePair")
	int32 IndexInRealNodes;

	//'Ranking' should be treated as an Index. The minimum value is 0, not 1.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|GraphViewSimplePair")
	int32 Ranking;

	FGraphViewSimplePair();
	FGraphViewSimplePair(int32 Index, int32 _Ranking);
	bool operator==(int32 OtherIndex) const;
};

USTRUCT(BlueprintType)
struct FGraphNodeDescriptionPtr
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|DescriptionPtr")
	TArray<FGraphViewSimplePair> Parents;
	
	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|DescriptionPtr")
	TArray<FGraphViewSimplePair> Children;
	
	//Only one party needs to point to the other. For example, if A1's Bro specifies A2, there is no need for A2 to reversely specify A1 in A2's Bro.
	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|DescriptionPtr")
	TArray<FGraphViewSimplePair> Brothers;
};


//真正用于存放信息的载体.GraphNode只是把信息传递给这个类
USTRUCT(BlueprintType)
struct FGraphNodeDescription
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|Description")
	FNamesInputNodeMirror NamesInputNodeMirror;

	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|Description")
	FLHCode_G_InputMirror LHCode_G_InputMirror;

	//Only relationships that have been confirmed and connected are stored here; otherwise, they remain in the two 'Mirror' containers above.
	//For example, given three nodes 'A1', 'A2', and 'A3', where 'A1' is the parent of 'A2' and 'A2' is the parent of 'A3',
	//but only 'A2' and 'A3' have participated in the construction so far, then only 'A3's 'Family' will contain 'A2', while 'A2's 'Family' will be empty.
	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|Description")
	FGraphNodeDescriptionPtr Family;
	
	//Debug name for 'Description', format: 01Player_C_0_GraphNode_Script
	UPROPERTY(BlueprintReadWrite, Category = "GraphWeaver|Description")
	FString ExplicitName;

	//The tags, markers, etc., that you want to add to this 'GraphNodeDescription'.
	UPROPERTY(BlueprintReadWrite, Category = "GraphWeaver|Description")
	FString Tag;

	//The additional data you want to add. You can also manually add other data by modifying the source code yourself,
	//and I highly recommend this approach (C++ version required).
	UPROPERTY(BlueprintReadWrite, Category = "GraphWeaver|Description")
	FInstancedStruct ExtraData;

	//The source 'GraphNode' passed in for construction. This target will be automatically updated each time the graph is rebuilt.
	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|Description", Transient)
	UGraphNode* SourceGraphNode;

	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|Description", DisplayName = "IndexInRealNodes")
	int32 IndexInRecorded; 
	
	FGraphNodeDescription();
	//对传进来的GraphNode进行镜像复制，避免GraphNode被GC回收而导致无法正确构建树
	FGraphNodeDescription(UGraphNode* Source, UGraphView* TargetView);
	~FGraphNodeDescription();

	UPROPERTY(BlueprintReadWrite, Category = "GraphWeaver|Description")
	bool Activated;
};



//好像并不能通过自定义序列化的方式来直接保存UGraphView的指针(我也不会)
//创建一个结构体来作为存档存储GraphView里面的信息.目前还不会怎么针对USTRUCT类进行使用自定义内存池,先使用官方的(其实好像也不应该使用内存池?避免没有进行存档的时候别的暂时不需要实例化的浪费空间)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//A carrier used to save and load information during archiving. This carrier is designed because
//users cannot directly store or load 'GraphView' class objects from archives.
USTRUCT(BlueprintType)
struct GRAPHWEAVERPLUGIN_API FGraphViewDataCarrier
{
	GENERATED_BODY()

	//以下某些参数通过SpawnGraphView节点传入,不需要该结构体存储该信息以减小内存占用
	//SelfOwner, GraphViewName, NamesConstructConfig, ConstructMethod, 

	//////////////////////////////////////////////////
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|Save")
	TArray<FGraphViewSimpleMap> Clans;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|Save")
	TArray<FGraphNodeDescription> RealNodes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|Save")
	TArray<int32> WillVerticalAwakeNode;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|Save")
	TArray<int32> WillHorizontalAwakeNode;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|Save")
	TEnumAsByte<NAGraphConstructErrorCode::EErrorCodeForConstructView> ErrorCodeForConstructView{0};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|Save")
	TEnumAsByte<NAWayToDealSameGraphNode::EWayToDealSameGraphNode> WayToDealSameNode{0};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|Save")
	TArray<FString> NodeRemoved;
};


//注意，因为在运行时可能改变GraphView的值，因此不要尝试在代码里面通过UGraphView::StaticClass()来获取CDO，这样会导致配置数据不一致。

////////////////////////////////////////////////////////
UCLASS(BlueprintType)
class GRAPHWEAVERPLUGIN_API UGraphView : public UObject
{
	GENERATED_BODY()
public:
	UGraphView(const FObjectInitializer& ObjectInitializer);
	virtual ~UGraphView() override;

public:
	//不建议再对名字进行修改，否则需要修改UK2Node_SpawnGraph的代码
	//所有的ExposeOnSpawn并不是真的要生成引脚，只是为了用来作为UK2Node_SpawnGraph的引脚
	
	////////////////////////////////////
	//External Owner. Automatically set by the function 'UFunctionTools::ModGraphViewBaseAttri_NotManuallyCall'.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|GraphView", meta = (ExposeOnSpawn = "SelfOwner"))
	UObject* SelfOwner;
	
	//The name of this 'GraphView'. Can only be changed within the 'GraphViewName' of the 'SpawnGraphView' Blueprint node.
	//Please check the naming rules by hovering over 'GraphViewName'.
	//Users must not attempt to change it manually!
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|GraphView", meta = (ExposeOnSpawn = "GraphViewName"))
	FString GraphViewName;

	//在UFunctionTools_GraphWeaver::ModGraphViewNaCon_NotManuallyCall里面设置,当Precision <= 1 的时候会自动设置为 1
	
	//////////////////////////////////////////////////////////////////////////////////
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GraphWeaver|GraphView", meta = (ExposeOnSpawn = "NamesConstructConfig"))
	FNamesConstructConfig NamesConstructConfig;

	//只有在NamesConstructConfig.NamingOfRules == true 的时候Clans才会被使用
	
	///////////////////////////////////////////////////////////////////////////////
	//This variable assists in constructing the 'GraphView' when 'ConstructMethod' is 'Names' and 'NamingOfRules' is true.
	//Users are only recommended to use it for debugging (although you cannot modify this value directly).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|GraphView")
	TArray<FGraphViewSimpleMap> Clans;

	//目前不打算针对LHCode_G_Input设置一个Config结构体

	//The actual storage for graph data. 'GraphNode' is only responsible for passing data to this array.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|GraphView")
	TArray<FGraphNodeDescription> RealNodes;

	//节点连接只有两种情况：A1指向A2,但是进入顺序有:1.A1先到然后A2到。2.A2先到然后A1到
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|GraphView")
	TArray<int32> WillVerticalAwakeNode;//父亲节点没有被链接完全的GraphNode节点在AlreadyRecorded里面的下标
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|GraphView")
	TArray<int32> WillHorizontalAwakeNode;//兄弟节点没有被链接完全的GraphNode节点在AlreadyRecorded里面的下标
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GraphWeaver|GraphView", meta = (ExposeOnSpawn = "ConstructMethod"))
	TEnumAsByte<NAConstructMethod::EConstructMethod> ConstructMethod;

	UPROPERTY(BlueprintReadOnly, Category = "GraphWeaver|GraphView")
	TEnumAsByte<NAGraphConstructErrorCode::EErrorCodeForConstructView> ErrorCodeForConstructView;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "GraphWeaver|GraphView")
	TEnumAsByte<NAWayToDealSameGraphNode::EWayToDealSameGraphNode> WayToDealSameNode;

	//The 'SelfName' or 'SelfId' of nodes removed via the 'RemoveNodes' function will be recorded here.
	//Recorded nodes will no longer actively participate in GraphView construction unless you explicitly clear this array along with 'RealNodes'.
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "GraphWeaver|GraphView")
	TArray<FString> NodeRemoved;
public:
	
	bool CheckSameNode_Names(UGraphNode* Node);
	bool CheckSameNode_LHCode(UGraphNode* Node);
	bool CheckIsNodeRemoved(UGraphNode* Node);
	
	void DealingWithParent_ChildRelationships(int32 ParentIndex, int32 ChildIndex);
	void DealingWithBrothersRelationships(int32 BroIndex, int32 SelfIndex);

	void NamesConstructWay(UGraphNode* TargetNode);

	//第一个int32是Code最深的成功导航深度，第二个int32是最终导航的节点在AlreadyRecorded里面的Index
	std::tuple<int32, int32> TryGuidByLHCode(FString& Code);
	void LHCode_G_ConstructWay(UGraphNode* TargetNode);

	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|GraphView", meta = (ToolTip = "Set the initial capacity in advance to speed up the construction process. Automatically called by the 'SpawnGraphView' blueprint node."))
	void AllocateGraphViewSize(int32 Size);

	//Add a new 'GraphNode' to the 'GraphView'. Generally, you should not call this function manually;
	//instead, it should be automatically invoked by setting the 'AutoBuild' parameter to true in 'SpawnGraphNode'.
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|GraphView", meta = (ToolTip = "Automatically adds new nodes to the graph. Manual invocation is not recommended."))
	bool AddNewNodeIntelligent(UGraphNode* NewNode);
	
	//Set a new value for 'WayToDealSameNode'.
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|GraphView")
	void SetWayToDealSameNode(TEnumAsByte<NAWayToDealSameGraphNode::EWayToDealSameGraphNode> Way);

	//Obtain a corresponding 'DataCarrier' from the current 'GraphView' for saving data into an archive.
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|GraphView")
	FGraphViewDataCarrier GetViewDataCarrier();

	//Reset the current 'GraphView' based on a 'GraphViewDataCarrier'. Used to restore data from an archive.
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|GraphView")
	void ResetViewFromDataCarrier(UPARAM(ref)FGraphViewDataCarrier& DataCarrier);

	//检查Ranking是否合理
	UFUNCTION(BlueprintPure, Category = "GraphWeaver|GraphView", meta = (BlueprintInternalUseOnly = "true"))
	static bool ValidateRankingConsistency_Inner(UGraphView* GraphView, UPARAM(ref)TArray<int32>& IgnoredNodeIndices);
	//检查Ranking是否合理
	bool ValidateRankingConsistency(TArray<int32>&& IgnoredNodeIndices);
	bool ValidateRankingConsistency(TArray<int32>& IgnoredNodeIndices);
	
	//只检查Ranking是否存在一个错误,不关心谁错,也不进行日志打印
	UFUNCTION(BlueprintPure, Category = "GraphWeaver|GraphView", meta = (BlueprintInternalUseOnly = "true"))
	static bool ValidateRankingConsistencyLight_Inner(UGraphView* GraphView, UPARAM(ref)TArray<int32>& IgnoredNodeIndices);
	bool ValidateRankingConsistencyLight(TArray<int32>&& IgnoredNodeIndices);
	bool ValidateRankingConsistencyLight(TArray<int32>& IgnoredNodeIndices);

	void AddRemovedNodeName(TArray<FGraphNodeDescription>& Removed);
	void AddRemovedNodeName(FGraphNodeDescription& Removed);
};















