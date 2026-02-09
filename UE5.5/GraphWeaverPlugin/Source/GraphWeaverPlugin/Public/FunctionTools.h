// Copyright 2026 RainButterfly. All Rights Reserved.
#pragma once


#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FunctionTools.generated.h"

namespace NAConstructMethod
{
	enum EConstructMethod : uint8;
}

namespace NAWayToDealSameGraphNode
{
	enum EWayToDealSameGraphNode : uint8;
}

class UGraphNode;
class UGraphView;
struct FLHCode_G_Input;
struct FGraphNodeDescription;
struct FNamesConstructConfig;
struct FNamesInputNode;

#define EMPTY_LOG() UE_LOG(LogTemp, Error, TEXT("        "))
#define WAITING_MOD_LOG() \
    do { \
        UE_LOG(LogTemp, Error, TEXT("The code here is incomplete and needs to be fixed immediately.")); \
        UE_LOG(LogTemp, Error, TEXT("File: %s, Func: %s, Line: %d"), TEXT(__FILE__), TEXT(__FUNCTION__), __LINE__); \
        EMPTY_LOG() \
    } while(0)


UCLASS()
class GRAPHWEAVERPLUGIN_API UFunctionTools_GraphWeaver : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFunctionTools_GraphWeaver();
	virtual ~UFunctionTools_GraphWeaver() override;

	static FString FlipString(FString& SourceString);

	//Gets the complete Id of the 'GraphNode', regardless of whether the Root node can be reached by tracing upwards.
	//Note that this function only takes effect for 'GraphNode' constructed in the LHCode_G manner. The Id of itself is included.
	UFUNCTION(BlueprintPure, Category = "GraphWeaver|FunctionTools")
	static TArray<FString> GetNodePath(UPARAM(ref)FGraphNodeDescription& SourceDesc);

	//Gets all child Descriptions of the specified Description with Activated set to true, and returns their Indexes in RealNodes.Do not include self.
	//Warning: If you use a structure like "A is B's parent, B is C's parent, C is A's parent" that causes the parent chain to form a loop,
	//do NOT use this method! Otherwise, it will cause a stack overflow and lead to program crash.
	UFUNCTION(BlueprintPure, Category = "GraphWeaver|FunctionTools")
	static TArray<int32> ObtainAllActivatedChildDescription(UGraphView* SourceGraphView, UPARAM(ref)FGraphNodeDescription& Des);

	//Gets all sibling Descriptions of the specified Description with Activated set to true.Do not include self.
	//Unlike the function ObtainAllActivatedChildDescription, this function allows loops in sibling relationships.
	//For example, if you record that A is B's sibling, B is C's sibling, and C is A's sibling, and both B and C have Activated set to true,
	//then inputting A will return the Indexes of B and C in RealNodes.
	UFUNCTION(BlueprintPure, Category = "GraphWeaver|FunctionTools")
	static TArray<int32> ObtainAllActivatedBroDescription(UGraphView* SourceGraphView, UPARAM(ref)FGraphNodeDescription& Des);

	//Only retrieves nodes that have a direct sibling or parent-child relationship with the currently specified node.
	//For example, if A is passed in, B is A's sibling node, C is A's child, and D is B's child. Assuming B, C, and D all have Activated set to true,
	//this function will only return B and C; D will not be returned as it has no direct relationship with A.
	UFUNCTION(BlueprintPure, Category = "GraphWeaver|FunctionTools")
	static TArray<int32> ObtainDirectActivatedChildAndBroDes(UGraphView* SourceGraphView, UPARAM(ref)FGraphNodeDescription& Des);

	//Retrieve all Child entries of a given 'Description'
	UFUNCTION(BlueprintPure, Category = "GraphWeaver|FunctionTools")
	static TArray<int32> ObtainAllChildDes(UGraphView* SourceGraphView, UPARAM(ref)FGraphNodeDescription& Des);
	
	//下面开始是为了辅助K2Node使用的函数，不应该由用户手动调用
	
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//用于辅助在UK2Node_SpawnGraphView的ExpandNode函数里面创建一个GraphView变量，该函数不应该由用户手动调用
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|FunctionToolsHidden", meta = (BlueprintInternalUseOnly = "true"))
	static UGraphView* CreateGraphView_NotManuallyCall(UObject* Outer);

	//这里或许可以改为使用CustomThunk来解决，避免头文件臃肿的问题
	//用于辅助在UK2Node_SpawnGraphView的ExpandNode函数里面修改一个GraphView变量，该函数不应该由用户手动调用
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|FunctionToolsHidden", meta = (BlueprintInternalUseOnly = "true"))
	static UGraphView* ModGraphViewBaseAttri_NotManuallyCall(UGraphView* Target, TEnumAsByte<NAConstructMethod::EConstructMethod> EnumValue, UObject* SelfOwner,
		const FString& ViewName, TEnumAsByte<NAWayToDealSameGraphNode::EWayToDealSameGraphNode> DealSameNode);

	//用于辅助在UK2Node_SpawnGraphView的ExpandNode函数里面修改一个GraphView变量，该函数不应该由用户手动调用
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|FunctionToolsHidden", meta = (BlueprintInternalUseOnly = "true"))
	static UGraphView* ModGraphViewNaCon_NotManuallyCall(UGraphView* Target, UPARAM(ref)FNamesConstructConfig& NamesValue);

	//最后阶段的各种准备工作
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|FunctionToolsHidden", meta = (BlueprintInternalUseOnly = "true"))
	static UGraphView* ModGraphViewFinalPhase_NotManuallyCall(UGraphView* Target, int32 SizeAllocate);

	//以下NotManuallyCall函数是提供给UK2Node_SpawnGraphNode的
	
	//////////////////////////////////////////////////////////////////////////////////////////////
	//不应该由用户手动调用
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|FunctionToolsHidden", meta = (BlueprintInternalUseOnly = "true"))
	static UGraphNode* CreateGraphNode_NotManuallyCall();
	
	//不应该由用户手动调用
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|FunctionToolsHidden", meta = (BlueprintInternalUseOnly = "true"))
	static UGraphNode* ModNamesInput_NotManuallyCall(UGraphNode* Target, UPARAM(ref)FNamesInputNode& Names);

	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|FunctionToolsHidden", meta = (BlueprintInternalUseOnly = "true"))
	static UGraphNode* ModLHCodeInput_NotManuallyCall(UGraphNode* Target, UPARAM(ref)FLHCode_G_Input& LHCode);

	//不应该由用户手动调用
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|FunctionToolsHidden", meta = (BlueprintInternalUseOnly = "true"))
	static UGraphNode* CallProcessInformOrNot_NotManuallyCall(UGraphNode* Target, bool AutoBuild);

	//通过名字来给GraphNode设置真正的GraphView
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|FunctionToolsHidden", meta = (BlueprintInternalUseOnly = "true"))
	static UGraphNode* SetRealSourceViewForNode_NotManuallyCall(UGraphNode* Node, FString ViewName);

	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|FunctionToolsHidden", meta = (BlueprintInternalUseOnly = "true"))
	static UGraphNode* SetNodeOuter_NotManuallyCall(UGraphNode* Target, UObject* Outer);
	
	//仅作占位符
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|FunctionToolsHidden", meta = (BlueprintInternalUseOnly = "true"))
	static void NonFunction();

	UFUNCTION(BlueprintPure, Category = "GraphWeaver|FunctionToolsHidden", meta = (BlueprintInternalUseOnly = "true"))
	static UGraphView* GetViewAndIndexFromNode(UGraphNode* Target, int32& Index);

	//static bool CanBePlacedInLevel(const UClass* Class);
	
	//以下函数不提供给用户使用
public:
	//打印变量类型
	template<typename T>
	static FString GetTypeName(const T& Var)
	{
		return UTF8_TO_TCHAR(typeid(T).name());
	}

	template<typename T>
	static TArray<T> ArrayLeftSplit(TArray<T>& SourceArray, int32 LeftArrayNum)
	{
		TArray<T> Result;
		int32 Num = 0;
		for (auto &i : SourceArray)
		{
			Num++;
			if (Num <= LeftArrayNum)
				Result.Emplace(i);
		}
		return Result;
	}

	//获取某个数组的最后几个元素
	template<typename T>
	static TArray<T> ArrayLastSeveral(TArray<T>& SourceArray, int32 LeftNum)
	{
		TArray<T> Result;
		if (SourceArray.Num() <= LeftNum)
			return Result;
		for( ; LeftNum < SourceArray.Num() ; LeftNum++)
		{
			Result.Emplace(SourceArray[LeftNum]);
		}
		return Result;
	}

	template<typename T>
	static TArray<T> ArrayNotInclude(TArray<T>& SourceArray, TArray<T>& IncludeArray)
	{
		TArray<T> rr;
		uint8 bIsOk = 1;
		for (auto& i : SourceArray)
		{
			bIsOk = 1;
			for (auto& j : IncludeArray)
			{
				if (i == j)
				{
					bIsOk = 0;
					break ;
				}
			}
			if (bIsOk)
				rr.Emplace(i);
		}
		return rr;
	}
};


//用于存放运行时真正的GraphView
class RealViewArray
{
public:
	static RealViewArray& Get()
	{
		static RealViewArray Instance;
		return Instance;
	}

	TArray<UGraphView*>& GetRealViews();
	
	RealViewArray(const RealViewArray&) = delete;
	RealViewArray& operator=(const RealViewArray&) = delete;

private:
	RealViewArray() = default;

	TArray<UGraphView*> GraphViews;
};



