// Fill out your copyright notice in the Description page of Project Settings.

#include "GraphShotCapture.h"
#include "GraphShotClipboard.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "ConnectionDrawingPolicy.h"
#include "NodeFactory.h"
#include "SGraphPin.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/IConsoleManager.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/ChildrenBase.h"
#include "Layout/Geometry.h"
#include "Misc/App.h"
#include "Rendering/SlateLayoutTransform.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "SGraphPanel.h"
#include "SGraphNode.h"
#include "SNodePanel.h"
#include "Slate/WidgetRenderer.h"
#include "Styling/AppStyle.h"
#include "TextureResource.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "GraphShotCapture"

// SGraphPanel's registered Slate type name (SWidget::GetType).
static const FName GraphPanelTypeName(TEXT("SGraphPanel"));

// ---------------------------------------------------------------------------
// Color-matching knobs (live; no recompile needed).
//
// Slate packs vertex/solid colors as sRGB bytes but samples textures as linear,
// so no single FWidgetRenderer gamma flag makes both match the on-screen editor
// exactly (false => textures dark, true => solid colors bright). These two cvars
// let you dial the capture to your editor:
//
//   GraphShot.UseGamma 0|1  RT sRGB base: 0 = linear RT (darker), 1 = sRGB RT (brighter). Default 1.
//   GraphShot.Gamma <float>  Post-readback pow(rgb, Gamma): >1 darkens, <1 brightens, 1.0 = off. Default 2.0
//                            (2.0 + UseGamma 1 matches the in-editor appearance for this project).
//
// If the capture is too bright, try:  GraphShot.Gamma 2.2   (slightly darker)
// If it is too dark, try:              GraphShot.Gamma 1.8
// Or switch the base:                  GraphShot.UseGamma 0  then adjust GraphShot.Gamma
// ---------------------------------------------------------------------------
static TAutoConsoleVariable<int32> CVarGraphShotUseGamma(
	TEXT("GraphShot.UseGamma"), 1,
	TEXT("RT sRGB base for graph capture: 0 = linear RT (darker), 1 = sRGB RT (brighter)."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarGraphShotGamma(
	TEXT("GraphShot.Gamma"), 2.0f,
	TEXT("Post-readback gamma applied to the capture: out = pow(rgb, Gamma). >1 darkens, <1 brightens, 1.0 = off. "
		 "2.0 matches the in-editor appearance when UseGamma=1 on this project."),
	ECVF_Default);

static void Notify(const FText& Message, bool bWarning)
{
	FNotificationInfo Info(Message);
	Info.bFireAndForget = true;
	Info.FadeInDuration = 0.1f;
	Info.FadeOutDuration = 0.5f;
	Info.ExpireDuration = bWarning ? 6.0f : 3.0f;
	if (bWarning)
	{
		Info.Image = FAppStyle::GetBrush(TEXT("NotificationBlend.Warning"));
	}
	if (FSlateApplication::IsInitialized())
	{
		FSlateNotificationManager::Get().AddNotification(Info);
	}
	if (bWarning)
	{
		UE_LOG(LogTemp, Warning, TEXT("GraphShot: %s"), *Message.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("GraphShot: %s"), *Message.ToString());
	}
}

/**
 * Find the SGraphPanel the user is currently viewing.
 * 1) Walk up from the keyboard-focused widget (precise for hotkey use).
 * 2) Fallback: BFS the active top-level window and pick the largest visible SGraphPanel
 *    (handles toolbar/menu triggers where focus isn't on the graph).
 */
static TSharedPtr<SGraphPanel> FindActiveGraphPanel()
{
	if (!FSlateApplication::IsInitialized())
	{
		return nullptr;
	}

	auto TryAsPanel = [](const TSharedPtr<SWidget>& Widget) -> TSharedPtr<SGraphPanel> {
		if (Widget.IsValid() && Widget->GetType() == GraphPanelTypeName)
		{
			return StaticCastSharedPtr<SGraphPanel>(Widget);
		}
		return nullptr;
	};

	// 1) Keyboard-focused widget and its ancestors.
	TSharedPtr<SWidget> Focus = FSlateApplication::Get().GetKeyboardFocusedWidget();
	while (Focus.IsValid())
	{
		if (TSharedPtr<SGraphPanel> Panel = TryAsPanel(Focus))
		{
			return Panel;
		}
		Focus = Focus->GetParentWidget();
	}

	// 2) BFS the active window for the largest visible graph panel.
	TSharedPtr<SWindow> Window = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (!Window.IsValid())
	{
		return nullptr;
	}

	TSharedPtr<SGraphPanel> BestPanel;
	float BestArea = 0.0f;

	TArray<TSharedPtr<SWidget>> Queue;
	Queue.Add(Window);
	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		TSharedPtr<SWidget> Cur = Queue[Head];
		if (!Cur.IsValid())
		{
			continue;
		}

		if (TSharedPtr<SGraphPanel> Panel = TryAsPanel(Cur))
		{
			if (Panel->GetVisibility() == EVisibility::Visible)
			{
				const FVector2f Size = Panel->GetCachedGeometry().GetLocalSize();
				const float Area = Size.X * Size.Y;
				if (Area >= BestArea)
				{
					BestArea = Area;
					BestPanel = Panel;
				}
			}
		}

		if (FChildren* Kids = Cur->GetChildren())
		{
			const int32 NumKids = Kids->Num();
			for (int32 i = 0; i < NumKids; ++i)
			{
				Queue.Add(Kids->GetChildAt(i));
			}
		}
	}

	return BestPanel;
}

// BFS a widget subtree for a set of pin widgets using the public SWidget::ArrangeChildren. SWidget::
// FindChildGeometries is protected, so it cannot be called on a different widget type (e.g. an SGraphNode
// from this SGraphPanel subclass); this reproduces its algorithm: arrange the subtree at each level and
// collect the requested pin widgets' real arranged geometry. The result is the same pin geometry the engine
// uses for visible nodes, so GetSplineEndPoints (which reads the geometry's AbsolutePosition AND GetDrawSize
// = size*scale) computes identical wire endpoints -- output pin at its right edge, input pin at its left
// edge, both vertically centered -- instead of the zero-size cull-path synthesis that misplaces them.
static void FindPinsInSubtree(
	const TSharedRef<SWidget>& Root,
	const FGeometry& RootGeometry,
	const TSet<TSharedRef<SWidget>>& PinsToFind,
	TMap<TSharedRef<SWidget>, FArrangedWidget>& OutPinGeometries)
{
	TArray<TPair<TSharedRef<SWidget>, FGeometry>> Queue;
	Queue.Emplace(Root, RootGeometry);
	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		// Copy out before Emplace may reallocate the queue below.
		const TSharedRef<SWidget> CurWidget = Queue[Head].Key;
		const FGeometry CurGeometry = Queue[Head].Value;

		FArrangedChildren Arranged(EVisibility::Visible);
		CurWidget->ArrangeChildren(CurGeometry, Arranged);

		for (int32 i = 0; i < Arranged.Num(); ++i)
		{
			const FArrangedWidget& Child = Arranged[i];
			if (PinsToFind.Contains(Child.Widget))
			{
				OutPinGeometries.Add(Child.Widget, Child);
			}
			Queue.Emplace(Child.Widget, Child.Geometry);
		}
	}
}

/**
 * SGraphPanel variant used only for GraphShot's detached capture panel.
 *
 * The base SGraphPanel only draws a wire to a node when it can locate that node's pin geometry:
 *  - visible, in-view nodes  -> real geometry via FindChildGeometries
 *  - culled, off-screen nodes -> synthesized geometry (the "cull path", which also draws their wires)
 * GraphShot hides every UNSELECTED node (EVisibility::Hidden) so its body/shadow do not leak into the
 * capture. A hidden node is absent from VisibleChildren, so FindChildGeometries cannot find its pins and
 * the base panel draws NO wires to in-view unselected nodes (it still draws wires to culled unselected
 * nodes via the cull path). The result the user sees: connections from a selected node to a nearby
 * unselected node vanish.
 *
 * This OnPaint override adds exactly the missing wires. After the base panel paints (background, selected
 * node bodies/shadows/comments/overlays, and the wires it already knows), it synthesizes pin geometry for
 * the hidden, in-view unselected nodes using the base panel's own cull-path formula and draws the wires
 * that connect a SELECTED node to one of those nodes, via the graph's real connection policy so the
 * spline/arrow/style match the base panel exactly. Culled unselected nodes are left to the base panel
 * (already handled), and wires between two unselected nodes are never drawn.
 */
class SGraphShotPanel : public SGraphPanel
{
public:
	/** Build a detached, read-only capture panel over the given graph (same config the old BuildTempPanel used). */
	static TSharedRef<SGraphShotPanel> Create(UEdGraph* InGraph)
	{
		SGraphPanel::FArguments Args;
		Args.GraphObj(InGraph)
			.IsEditable(false)
			.DisplayAsReadOnly(false)
			.ShowGraphStateOverlay(false)
			.AllowConnectionSlicing(false)
			.ShouldDrawBackground(true)
			.AllowZoom(true)
			.AllowPanning(false);

		TSharedRef<SGraphShotPanel> Panel = MakeShared<SGraphShotPanel>();
		Panel->Construct(Args);
		return Panel;
	}

	void Construct(const SGraphPanel::FArguments& InArgs)
	{
		SGraphPanel::Construct(InArgs);
	}

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;
};

int32 SGraphShotPanel::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	// Let the base panel paint the background, the selected node bodies (shadows/comments/overlays/popups),
	// and the wires it already knows how to draw: selected<->selected, and selected<->culled-unselected
	// (the cull path synthesizes geometry for off-screen nodes, hidden or not, and draws their wires).
	const int32 SuperMaxLayerId = SGraphPanel::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (!GraphObj)
	{
		return SuperMaxLayerId;
	}

	// The base SGraphPanel::OnPaint is also const and iterates the protected `Children` member directly; we
	// do the same here (GetManagedChildren() is non-const and so cannot be called from this const override).
	const int32 NumChildren = Children.Num();
	if (NumChildren == 0)
	{
		return SuperMaxLayerId;
	}

	const UEdGraphSchema* Schema = GraphObj->GetSchema();
	if (!Schema)
	{
		return SuperMaxLayerId;
	}

	const float ZoomFactor = AllottedGeometry.Scale * GetZoomAmount();

	// The base SGraphPanel only draws a wire to a node when it can locate that node's pin geometry: real for
	// in-view nodes, synthesized (cull path) for off-screen nodes. GraphShot hides every UNSELECTED node
	// (EVisibility::Hidden) so it doesn't leak into the shot, which leaves no geometry for the hidden node's
	// pins and therefore no wires touching it. This OnPaint adds back the wires that touch a hidden, in-view
	// unselected node.
	//
	// IMPORTANT: this detached temp panel carries its OWN empty SelectionManager (selection lives on the live
	// panel), so "selected" is NOT read from SelectionManager here. It is exactly the inverse of the
	// Hidden/Visible state the capture flow already set: nodes the user selected stay Visible, unselected
	// nodes are Hidden. So we drive everything off that visibility.
	TSet<UEdGraphNode*> HiddenNodes;
	for (int32 i = 0; i < NumChildren; ++i)
	{
		const TSharedRef<SGraphNode> GraphNodeWidget = StaticCastSharedRef<SGraphNode>(Children[i]);
		if (UEdGraphNode* NodeObj = GraphNodeWidget->GetNodeObj())
		{
			if (GraphNodeWidget->GetVisibility() != EVisibility::Visible)
			{
				HiddenNodes.Add(NodeObj);
			}
		}
	}

	// No hidden nodes -> this is a whole-graph capture and the base panel already drew every wire.
	if (HiddenNodes.Num() == 0)
	{
		return SuperMaxLayerId;
	}

	TMap<TSharedRef<SWidget>, FArrangedWidget> PinGeometries;

	// Use REAL arranged pin geometry for EVERY node (including hidden/unselected ones) so the wire endpoints
	// match the engine exactly. The engine's spline endpoints come from GetSplineEndPoints, which reads the pin
	// geometry's AbsolutePosition AND GetDrawSize() (size*scale):
	//   output pin start = (AbsPos.X + DrawSize.X, AbsPos.Y + DrawSize.Y/2)  -- right edge, vertically centered
	//   input  pin end   = (AbsPos.X,         AbsPos.Y + DrawSize.Y/2)        -- left edge,  vertically centered
	// The base panel's cull-path synthesizes a ZERO-size geometry for off-screen nodes; that collapses the
	// output-pin endpoint to the left edge and drops the vertical centering. That is fine for wires running
	// off-screen, but for in-view hidden nodes it puts the wire's hidden end in the wrong place ("乱").
	// So instead we arrange each node -- even hidden ones -- and use FindChildGeometries to obtain the true
	// arranged pin geometry, identical to what the engine uses for visible nodes.
	for (int32 i = 0; i < NumChildren; ++i)
	{
		const TSharedRef<SGraphNode> GraphNodeWidget = StaticCastSharedRef<SGraphNode>(Children[i]);

		// Culled (off-screen) hidden nodes are already handled by the base panel's cull path; skip them.
		if (GraphNodeWidget->GetVisibility() != EVisibility::Visible && IsNodeCulled(GraphNodeWidget, AllottedGeometry))
		{
			continue;
		}

		// Real node geometry, the same formula SNodePanel::ArrangeChildNodes uses.
		const FArrangedWidget NodeArranged = AllottedGeometry.MakeChild(
			StaticCastSharedRef<SWidget>(GraphNodeWidget),
			GraphNodeWidget->GetPosition2f() - GetViewOffset(),
			GraphNodeWidget->GetDesiredSize(),
			GetZoomAmount());

		TSet<TSharedRef<SWidget>> NodePins;
		GraphNodeWidget->GetPins(NodePins);

		TMap<TSharedRef<SWidget>, FArrangedWidget> NodePinGeoms;
		FindPinsInSubtree(StaticCastSharedRef<SWidget>(GraphNodeWidget), NodeArranged.Geometry, NodePins, NodePinGeoms);
		PinGeometries.Append(MoveTemp(NodePinGeoms));
	}

	if (PinGeometries.Num() == 0)
	{
		return SuperMaxLayerId;
	}

	// Use the graph's real connection policy (via the static FNodeFactory, since this temp panel has no
	// custom NodeFactory set) so spline/arrow/wire-style match the base panel exactly. Draw the spline below
	// the nodes (WireLayerId = LayerId + 3, matching the base panel's fixed layer allocation in OnPaint) and
	// the arrow above everything (SuperMaxLayerId).
	const int32 WireLayerId = LayerId + 3;
	TUniquePtr<FConnectionDrawingPolicy> Policy(FNodeFactory::CreateConnectionPolicy(
		Schema, WireLayerId, SuperMaxLayerId, ZoomFactor, MyCullingRect, OutDrawElements, GraphObj));
	if (!Policy)
	{
		// The schema factory should normally return a policy; fall back to the base policy rather than
		// silently dropping the wires.
		Policy = TUniquePtr<FConnectionDrawingPolicy>(new FConnectionDrawingPolicy(WireLayerId, SuperMaxLayerId, ZoomFactor, MyCullingRect, OutDrawElements));
	}

	// Map UEdGraphPin* -> pin widget (mirrors FConnectionDrawingPolicy::BuildPinToPinWidgetMap) so each
	// link's target end can be resolved and its fade state read.
	TMap<UEdGraphPin*, TSharedRef<SGraphPin>> PinToWidget;
	for (const TPair<TSharedRef<SWidget>, FArrangedWidget>& Pair : PinGeometries)
	{
		const TSharedRef<SWidget>& PinWidget = Pair.Key;
		SGraphPin& Pin = static_cast<SGraphPin&>(PinWidget.Get());
		if (UEdGraphPin* PinObj = Pin.GetPinObj())
		{
			PinToWidget.Add(PinObj, StaticCastSharedRef<SGraphPin>(PinWidget));
		}
	}

	// Draw only the wires that connect a KEPT (selected) node to a HIDDEN (unselected) node -- the ones the
	// base panel missed (the base panel draws kept<->kept, and kept<->culled-hidden via the cull path). Wires
	// between two hidden nodes must not appear (only the kept nodes' connections are wanted).
	for (const TPair<TSharedRef<SWidget>, FArrangedWidget>& Pair : PinGeometries)
	{
		const TSharedRef<SWidget> PinWidget = Pair.Key;
		SGraphPin& Pin = static_cast<SGraphPin&>(PinWidget.Get());
		UEdGraphPin* ThePin = Pin.GetPinObj();
		if (!ThePin || ThePin->Direction != EGPD_Output)
		{
			continue;
		}

		const bool bStartHidden = HiddenNodes.Contains(ThePin->GetOwningNode());

		for (UEdGraphPin* TargetPin : ThePin->LinkedTo)
		{
			const TSharedRef<SGraphPin>* TargetWidgetPtr = PinToWidget.Find(TargetPin);
			if (!TargetWidgetPtr)
			{
				continue; // target not in our geometry set (culled, or a collapsed pin) -> base handled or N/A
			}

			const bool bEndHidden = HiddenNodes.Contains(TargetPin->GetOwningNode());
			if (bStartHidden == bEndHidden)
			{
				continue; // both kept (base drew it) or both hidden (must not draw)
			}

			const FArrangedWidget* StartGeom = PinGeometries.Find(PinWidget);
			const FArrangedWidget* EndGeom = PinGeometries.Find(*TargetWidgetPtr);
			if (!StartGeom || !EndGeom)
			{
				continue;
			}

			FConnectionParams Params;
			Policy->DetermineWiringStyle(ThePin, TargetPin, Params);

			// Match the base panel's fade rule: if both ends are faded, dim the wire.
			if (Pin.AreConnectionsFaded() && (*TargetWidgetPtr)->AreConnectionsFaded())
			{
				Params.WireColor.A = 0.2f;
			}

			Policy->DrawSplineWithArrow(StartGeom->Geometry, EndGeom->Geometry, Params);
		}
	}

	return SuperMaxLayerId;
}

/** Build a detached, read-only temp panel over the given graph. It only reads Graph->Nodes and owns its own widgets. */
static TSharedPtr<SGraphPanel> BuildTempPanel(UEdGraph* Graph)
{
	return SGraphShotPanel::Create(Graph);
}

/** Union of every node widget's position + desired size (graph space). */
static bool ComputeAllNodeBounds(SGraphPanel& Panel, FSlateRect& OutBounds)
{
	FVector2f Min(MAX_FLT, MAX_FLT);
	FVector2f Max(-MAX_FLT, -MAX_FLT);
	bool bAny = false;

	FChildren* Kids = Panel.GetManagedChildren();
	if (!Kids)
	{
		return false;
	}

	for (int32 i = 0; i < Kids->Num(); ++i)
	{
		// Every managed child of an SGraphPanel is an SGraphNode, which derives from SNodePanel::SNode.
		TSharedPtr<SWidget> ChildWidget = Kids->GetChildAt(i);
		const TSharedPtr<SNodePanel::SNode> NodeWidget = StaticCastSharedPtr<SNodePanel::SNode>(ChildWidget);
		const FVector2f Pos = NodeWidget->GetPosition2f();
		const FVector2f Size = NodeWidget->GetDesiredSize();

		Min.X = FMath::Min(Min.X, Pos.X);
		Min.Y = FMath::Min(Min.Y, Pos.Y);
		Max.X = FMath::Max(Max.X, Pos.X + Size.X);
		Max.Y = FMath::Max(Max.Y, Pos.Y + Size.Y);
		bAny = true;
	}

	if (!bAny)
	{
		return false;
	}

	OutBounds = FSlateRect(Min.X, Min.Y, Max.X, Max.Y);
	return true;
}

/** Union of node widgets whose underlying graph node is currently selected (graph space).
 *  Comments participate too: SGraphNodeComment derives from SGraphNode, so a comment
 *  selected in the live panel is captured exactly like a node.
 *  Bounds are computed only from widgets present on the temp panel; selected nodes with no
 *  widget are simply ignored. */
static bool ComputeSelectedNodeBounds(SNodePanel& Panel, const FGraphSelectionManager& Selection, FSlateRect& OutBounds)
{
	FVector2f Min(MAX_FLT, MAX_FLT);
	FVector2f Max(-MAX_FLT, -MAX_FLT);
	bool bAny = false;

	FChildren* Kids = Panel.GetManagedChildren();
	if (!Kids)
	{
		return false;
	}

	for (int32 i = 0; i < Kids->Num(); ++i)
	{
		TSharedPtr<SWidget> ChildWidget = Kids->GetChildAt(i);
		if (!ChildWidget.IsValid())
		{
			continue;
		}

		// It may be a comment, but every managed child on a graph panel is an SGraphNode
		// (SGraphNodeComment isa SGraphNode), so cast down to the base node widget type.
		const TSharedPtr<SGraphNode> GraphNodeWidget = StaticCastSharedPtr<SGraphNode>(ChildWidget);
		if (UEdGraphNode* GraphNode = GraphNodeWidget->GetNodeObj())
		{
			if (!Selection.IsNodeSelected(GraphNode))
			{
				continue;
			}

			const FVector2f Pos = GraphNodeWidget->GetPosition2f();
			const FVector2f Size = GraphNodeWidget->GetDesiredSize();

			Min.X = FMath::Min(Min.X, Pos.X);
			Min.Y = FMath::Min(Min.Y, Pos.Y);
			Max.X = FMath::Max(Max.X, Pos.X + Size.X);
			Max.Y = FMath::Max(Max.Y, Pos.Y + Size.Y);
			bAny = true;
		}
	}

	if (!bAny)
	{
		return false;
	}

	OutBounds = FSlateRect(Min.X, Min.Y, Max.X, Max.Y);
	return true;
}

/** Hide every node widget that is NOT currently selected (graph space).
 *  Comments participate too: FGraphNodeComment derives from FGraphNode, so a comment
 *  selected in the live graph is captured exactly like a node. Hidden widgets keep
 *  unselected nodes/comments from appearing above/below the captured region. */
static void HideUnselectedNodeWidgets(SGraphPanel& Panel, const FGraphSelectionManager& Selection)
{
	FChildren* Kids = Panel.GetManagedChildren();
	if (!Kids)
	{
		return;
	}

	for (int32 i = 0; i < Kids->Num(); ++i)
	{
		TSharedPtr<SWidget> ChildWidget = Kids->GetChildAt(i);
		if (!ChildWidget.IsValid())
		{
			continue;
		}

		// May be a comment, but every managed child on a graph panel is an SGraphNode
		// (SGraphNodeComment isa SGraphNode), so cast down to the base node widget type.
		const TSharedPtr<SGraphNode> GraphNodeWidget = StaticCastSharedPtr<SGraphNode>(ChildWidget);
		if (UEdGraphNode* GraphNode = GraphNodeWidget->GetNodeObj())
		{
			if (!Selection.IsNodeSelected(GraphNode))
			{
				GraphNodeWidget->SetVisibility(EVisibility::Hidden);
			}
		}
	}
}

bool FGraphShotCapture::CanCapture()
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}
	TSharedPtr<SGraphPanel> Panel = FindActiveGraphPanel();
	return Panel.IsValid() && Panel->GetGraphObj() != nullptr && Panel->GetGraphObj()->Nodes.Num() > 0;
}

bool FGraphShotCapture::CaptureToClipboard()
{
	if (!FSlateApplication::IsInitialized())
	{
		Notify(LOCTEXT("NoSlate", "Slate is not initialized."), true);
		return false;
	}

	TSharedPtr<SGraphPanel> LivePanel = FindActiveGraphPanel();
	if (!LivePanel.IsValid())
	{
		Notify(LOCTEXT("NoGraph", "No graph editor is currently focused or open."), true);
		return false;
	}

	UEdGraph* Graph = LivePanel->GetGraphObj();
	if (!Graph || Graph->Nodes.Num() == 0)
	{
		Notify(LOCTEXT("EmptyGraph", "The current graph is empty."), true);
		return false;
	}

	// Build a detached temp panel over the SAME UEdGraph so the live editor is never touched.
	TSharedPtr<SGraphPanel> Panel = BuildTempPanel(Graph);
	if (!Panel.IsValid())
	{
		Notify(LOCTEXT("BuildFail", "Failed to build the capture panel."), true);
		return false;
	}

	// Create + prepass a widget for every node (valid GetDesiredSize).
	Panel->Update();

	// If the user has nodes/comments selected in the live graph, capture ONLY that region;
	// otherwise fall back to the whole graph (original behavior).
	FSlateRect Bounds;
	const FGraphSelectionManager& Selection = LivePanel->SelectionManager;
	const bool bHasSelection = Selection.AreAnyNodesSelected() && ComputeSelectedNodeBounds(*Panel, Selection, Bounds);

	if (!bHasSelection && !ComputeAllNodeBounds(*Panel, Bounds))
	{
		Notify(LOCTEXT("NoBounds", "Could not compute graph bounds (no node widgets)."), true);
		return false;
	}

	const FVector2f GraphSize = Bounds.GetSize2f();
	const float Pad = 64.0f;
	const float MaxTex = FMath::Min((float)GetMax2DTextureDimension(), 8192.f);

	// Desired scale to fit the whole graph within the render-target limit (never upscale beyond 1:1).
	const float DesiredScale = FMath::Clamp(
		FMath::Min(MaxTex / (GraphSize.X + 2.f * Pad), MaxTex / (GraphSize.Y + 2.f * Pad)),
		0.01f, 1.0f);

	// Zoom is discrete: RestoreViewSettings snaps to the nearest zoom level. Pick the largest level
	// whose amount is <= DesiredScale so the rendered extent never exceeds DrawSize (no clipping)
	// and stays within MaxTex. Level 0 is the most zoomed-out (smallest amount).
	const TSharedPtr<FZoomLevelsContainer>& ZoomLevels = Panel->GetZoomLevels();
	float BestAmount = ZoomLevels->GetZoomAmount(0);
	const int32 NumLevels = ZoomLevels->GetNumZoomLevels();
	for (int32 L = 0; L < NumLevels; ++L)
	{
		const float Amount = ZoomLevels->GetZoomAmount(L);
		if (Amount <= DesiredScale + KINDA_SMALL_NUMBER && Amount >= BestAmount)
		{
			BestAmount = Amount;
		}
	}

	// Apply the view (positive amount => direct zoom, no deferred zoom-to-fit) and read back the
	// real zoom so DrawSize matches it exactly.
	Panel->RestoreViewSettings(FVector2f(Bounds.Left - Pad, Bounds.Top - Pad), BestAmount, FGuid());
	const float ActualZoom = Panel->GetZoomAmount();

	int32 Width = FMath::Clamp(FMath::RoundToInt((GraphSize.X + 2.f * Pad) * ActualZoom), 1, (int32)MaxTex);
	int32 Height = FMath::Clamp(FMath::RoundToInt((GraphSize.Y + 2.f * Pad) * ActualZoom), 1, (int32)MaxTex);

	// Tick the detached panel with a geometry covering the draw size so PopulateVisibleChildren
	// keeps EVERY node (IsNodeCulled returns false within the allotted area). FWidgetRenderer only
	// prepasses; it never ticks, so VisibleChildren must be populated here first.
	const FGeometry TickGeo = FGeometry::MakeRoot(FVector2f((float)Width, (float)Height), FSlateLayoutTransform(1.0f));
	Panel->Tick(TickGeo, FApp::GetCurrentTime(), 0.0f);

	// Hide unselected nodes AFTER the tick. SGraphPin::Tick caches each pin's offset within its node
	// (CachedNodeOffset), and SGraphShotPanel::OnPaint synthesizes the hidden end of each wire from that
	// cache using the same formula as the engine's cull path. A Hidden node is never ticked, so hiding
	// before the tick would leave those offsets stale and the wires to hidden nodes would land in the
	// wrong place ("乱"). Ticking first -- with every node visible, at the render view/zoom -- populates
	// every in-view pin's offset, THEN we hide so only the selected node bodies are kept off-screen.
	if (bHasSelection)
	{
		HideUnselectedNodeWidgets(*Panel, Selection);
	}

	// Render the panel into a render target at the chosen size.
	// The RT sRGB base is selectable via GraphShot.UseGamma (1 = sRGB RT, brighter; 0 = linear RT, darker).
	// See the cvar comments above for why neither is a perfect match and how to tune GraphShot.Gamma.
	const bool bUseGamma = CVarGraphShotUseGamma.GetValueOnGameThread() != 0;
	FWidgetRenderer Renderer(/*bUseGammaCorrection=*/bUseGamma, /*bInClearTarget=*/true);
	UTextureRenderTarget2D* RenderTarget = Renderer.DrawWidget(Panel.ToSharedRef(), FVector2D((double)Width, (double)Height));
	if (!RenderTarget)
	{
		Notify(LOCTEXT("RenderFail", "Failed to render the graph."), true);
		return false;
	}

	FlushRenderingCommands();

	TArray<FColor> Pixels;
	FRenderTarget* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
	if (!RTResource || !RTResource->ReadPixels(Pixels, FReadSurfaceDataFlags(RCM_UNorm)))
	{
		Notify(LOCTEXT("ReadFail", "Failed to read the rendered pixels."), true);
		RenderTarget->ConditionalBeginDestroy();
		return false;
	}

	// Optional post-readback gamma (GraphShot.Gamma): out = pow(rgb, Gamma). Default 1.0 = off.
	const float GammaPow = CVarGraphShotGamma.GetValueOnGameThread();
	if (!FMath::IsNearlyEqual(GammaPow, 1.0f, KINDA_SMALL_NUMBER))
	{
		for (FColor& Pixel : Pixels)
		{
			auto Apply = [GammaPow](uint8 C) -> uint8
			{
				const float f = FMath::Pow(C / 255.0f, GammaPow);
				return (uint8)FMath::Clamp(FMath::RoundToInt(f * 255.0f), 0, 255);
			};
			Pixel.R = Apply(Pixel.R);
			Pixel.G = Apply(Pixel.G);
			Pixel.B = Apply(Pixel.B);
		}
	}

	const bool bCopied = GraphShotCopyPixelsToClipboard(Pixels, Width, Height);
	RenderTarget->ConditionalBeginDestroy();

	if (bCopied)
	{
		Notify(FText::Format(LOCTEXT("Copied", "Graph screenshot ({0}x{1}) copied to clipboard."), FText::AsNumber(Width), FText::AsNumber(Height)), false);
	}
	else
	{
		Notify(LOCTEXT("CopyFail", "Failed to copy the screenshot to the clipboard."), true);
	}
	return bCopied;
}

#undef LOCTEXT_NAMESPACE
