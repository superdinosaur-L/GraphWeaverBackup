// Copyright 2026 RainButterfly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GraphNode.generated.h"

/**
 * 
 */

class UGraphView;
struct FGraphNodeDescription;

USTRUCT(BlueprintType)
struct FNamesInputNode
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GraphWeaver|InputConfiguration", meta = (ExposeOnSpawn = "SelfName"))
	FString SelfName = "None";
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GraphWeaver|InputConfiguration", meta = (ExposeOnSpawn = "ParentNodeNames"))
	TArray<FString> ParentNodeNames;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GraphWeaver|InputConfiguration", meta = (ExposeOnSpawn = "BroNames"))
	TArray<FString> BroNames;
};

USTRUCT(BlueprintType)
struct FLHCode_G_Input
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GraphWeaver|InputConfiguration", meta = (ExposeOnSpawn = "SelfId"))
	FString SelfId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GraphWeaver|InputConfiguration", meta = (ExposeOnSpawn = "ParentCodes"))
	TArray<FString> ParentCodes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GraphWeaver|InputConfiguration", meta = (ExposeOnSpawn = "BrotherCodes"))
	TArray<FString> BrotherCodes;
};

//UGraphNode只是作为用户操作的一种载体，因为自身无法进行序列化和反序列化，所以在GraphView里面都是以镜像的方式来存储对应的GraphNode

/////////////////////////////////////////////////////////////////////////////////////////////////////////
UCLASS(BlueprintType)
class GRAPHWEAVERPLUGIN_API UGraphNode : public UObject
{
	GENERATED_BODY()
public:
	UGraphNode();
	virtual ~UGraphNode() override;

public:
	//The owner of this node. Automatically set in the function 'UK2Node_SpawnGraphNode->UFunctionTools::SetNodeOuter_NotManuallyCall'
	//(called automatically by the blueprint node 'SpawnGraphNode').
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GraphWeaver|GraphNode", meta = (ExposeOnSpawn = "SelfOuter"))
	UObject* SelfOuter;

	//Which 'GraphView' does this 'GraphNode' and its corresponding 'Description' belong to?
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|GraphNode", meta = (ExposeOnSpawn = "SourceGraph"))
	UGraphView* SourceGraphView;

	//在UFunctionTools::ModNamesInput_NotManuallyCall里面自动设置

	//////////////////////////////////////////////////////////////////////////////
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GraphWeaver|GraphNode", meta = (ExposeOnSpawn = "NamesInput"))
	FNamesInputNode NamesInput;

	//在UFunctionTools::ModLHCodeInput_NotManuallyCall中自动设置

	/////////////////////////////////////////////////////////////////////////////////////
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GraphWeaver|GraphNode", meta = (ExposeOnSpawn = "LHCode_G_Input"))
	FLHCode_G_Input LHCode_G_Input;

	//A human-readable name for debugging purposes that reflects internal and external relationships.
	//Automatically set in UK2Node_SpawnGraphNode->UFunctionTools::SetNodeOuter_NotManuallyCall.
	//Format: 01Player_C_0_GraphNode
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "GraphWeaver|GraphNode")
	FString ExplicitName;

	//The mirrored Index of 'Description' within 'RealNodes' of 'SourceGraphView'
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GraphWeaver|GraphNode")
	int32 IndexInRealNodes = -1;
	
	//bool Recorded;
public:
	UFUNCTION(BlueprintCallable, Category = "GraphWeaver|GraphNode")
	bool ProcessInformAuto(UGraphView* InGraph);

	//仅供c++内部使用,不要对外开放接口,因为这种简单的引用在蓝图里面会变成复制拷贝.如果要在蓝图里面使用
	//对应功能的函数,应该使用GetDesRef.详情查看'UK2Node_GetDesRef'
	//For C++ internal use only. Do not expose this interface externally, as such simple references will become copy operations in Blueprints.  
	//If you need to use the corresponding functionality in Blueprints, use GetDesRef instead. See 'UK2Node_GetDesRef' for details.
	FGraphNodeDescription& ObtainSelfDescription();

	//Gets all child Descriptions of the specified Description with Activated set to true, and returns their Indexes in RealNodes.Do not include self.
	//Warning: If you use a structure like "A is B's parent, B is C's parent, C is A's parent" that causes the parent chain to form a loop,
	//do NOT use this method! Otherwise, it will cause a stack overflow and lead to program crash.
	UFUNCTION(BlueprintPure, Category = "GraphWeaver|GraphNode")
	TArray<int32> ObtainAllActivatedChildDescription();
	
	//Gets all sibling Descriptions of the specified Description with Activated set to true.Do not include self.
	//Unlike the function ObtainAllActivatedChildDescription, this function allows loops in sibling relationships.
	//For example, if you record that A is B's sibling, B is C's sibling, and C is A's sibling, and both B and C have Activated set to true,
	//then inputting A will return the Indexes of B and C in RealNodes.
	UFUNCTION(BlueprintPure, Category = "GraphWeaver|GraphNode")
	TArray<int32> ObtainAllActivatedBroDescription();

	//Only retrieves nodes that have a direct sibling or parent-child relationship with the currently specified node.
	//For example, if A is passed in, B is A's sibling node, C is A's child, and D is B's child. Assuming B, C, and D all have Activated set to true,
	//this function will only return B and C; D will not be returned as it has no direct relationship with A.
	UFUNCTION(BlueprintPure, Category = "GraphWeaver|GraphNode")
	TArray<int32> ObtainDirectActivatedChildAndBroDescription();

	//Retrieve all Child entries of a given 'Description'
	UFUNCTION(BlueprintPure, Category = "GraphWeaver|GraphNode")
	TArray<int32> ObtainAllChildDescription();
};




