// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * GraphShot capture engine.
 *
 * Captures a COMPLETE screenshot (all nodes, including ones scrolled off-screen) of the graph
 * the user is currently viewing, and copies it to the clipboard.
 *
 * Works for any UEdGraph-based editor (Blueprint event graph, AnimBlueprint event graph,
 * ControlRig ForwardSolve, Material, ...) because node widgets are created via
 * UEdGraphNode::CreateVisualWidget(), which dispatches per node type.
 */
struct FGraphShotCapture
{
	/** Capture the current graph to the clipboard. Returns true on success. */
	static bool CaptureToClipboard();

	/** True if a graph panel is currently focused/visible and could be captured. */
	static bool CanCapture();
};
