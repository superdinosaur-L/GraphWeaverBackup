// Copyright 2026 RainButterfly. All Rights Reserved.

#include "GraphNode.h"
#include "FunctionTools.h"
#include "GraphView.h"
#include "Engine/Engine.h"

UGraphNode::UGraphNode()
{
	SelfOuter = nullptr;
	IndexInRealNodes = -1;
	ExplicitName = "None";
	SourceGraphView = nullptr;
}

UGraphNode::~UGraphNode()
{
}





bool UGraphNode::ProcessInformAuto(UGraphView* InGraph)
{
	if (InGraph)
	{
		InGraph->AddNewNodeIntelligent(this);
	}
	else
	{
		//SpawnGraphNode::ValidateNodeDuringCompilation
		//实际上下面的报错大概率不会发生,主要由上面的函数和GetBlueprint()->Status = BS_Dirty来确保TargetView正确
		UE_LOG(LogTemp, Error, TEXT("UGraphNode::ProcessInformAuto: InGraph == nullptr"));
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("UGraphNode::ProcessInformAuto: InGraph == nullptr"));
	}
	return true;
}

FGraphNodeDescription& UGraphNode::ObtainSelfDescription()
{
	return SourceGraphView->RealNodes[IndexInRealNodes];
}

TArray<int32> UGraphNode::ObtainAllActivatedChildDescription()
{
	return UFunctionTools_GraphWeaver::ObtainAllActivatedChildDescription(SourceGraphView, ObtainSelfDescription());
}

TArray<int32> UGraphNode::ObtainAllActivatedBroDescription()
{
	return UFunctionTools_GraphWeaver::ObtainAllActivatedBroDescription(SourceGraphView, ObtainSelfDescription());
}

TArray<int32> UGraphNode::ObtainDirectActivatedChildAndBroDescription()
{
	return UFunctionTools_GraphWeaver::ObtainDirectActivatedChildAndBroDes(SourceGraphView, ObtainSelfDescription());
}

TArray<int32> UGraphNode::ObtainAllChildDescription()
{
	return UFunctionTools_GraphWeaver::ObtainAllChildDes(SourceGraphView, ObtainSelfDescription());
}














