//
// FILE: treewin.cc -- Implementation of TreeWindow class
//
// $Id$
//

#include "wx/wxprec.h"
#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif  // WX_PRECOMP
#include "wx/dcps.h"
#include "wx/dragimag.h"
#include "guishare/wxmisc.h"

#include "base/gmisc.h"
#include "math/math.h"
#include "efg.h"
#include "treewin.h"
#include "efgshow.h"
#include "treedrag.h"

#include "efgutils.h"
#include "base/glist.imp"


//----------------------------------------------------------------------
//                      TreeWindow: Member functions
//----------------------------------------------------------------------

BEGIN_EVENT_TABLE(TreeWindow, wxScrolledWindow)
  EVT_SIZE(TreeWindow::OnSize)
  EVT_MOTION(TreeWindow::OnMouseMotion)
  EVT_LEFT_DOWN(TreeWindow::OnLeftClick)
  EVT_LEFT_DCLICK(TreeWindow::OnLeftDoubleClick)
  EVT_LEFT_UP(TreeWindow::OnMouseMotion)
  EVT_RIGHT_DOWN(TreeWindow::OnRightClick)
  EVT_CHAR(TreeWindow::OnKeyEvent)
END_EVENT_TABLE()

//----------------------------------------------------------------------
//                  TreeWindow: Constructor and destructor
//----------------------------------------------------------------------

TreeWindow::TreeWindow(EfgShow *p_efgShow, wxWindow *p_parent)
  : wxScrolledWindow(p_parent),
    m_efg(*p_efgShow->Game()), m_parent(p_efgShow), m_layout(m_efg, this),
    m_zoom(1.0), m_markNode(0), m_cursor(m_efg.RootNode()),
    m_dragImage(0), m_dragSource(0),
    m_infosetDragger(new IsetDragger(this, m_efg)),
    m_branchDragger(new BranchDragger(this, m_efg))
{
  // Make sure that Chance player has a name
  m_efg.GetChance()->SetName("Chance");

  SetBackgroundColour(*wxWHITE);
  MakeMenus();
}

TreeWindow::~TreeWindow()
{
  delete m_infosetDragger;
  delete m_branchDragger;
  Show(false);
}

void TreeWindow::MakeMenus(void)
{
  m_nodeMenu = new wxMenu;

  m_nodeMenu->Append(efgmenuEDIT_NODE_ADD, "Add Move", "Add a move");
  m_nodeMenu->Append(efgmenuEDIT_NODE_INSERT, "Insert Move", 
		     "Insert a move before this node");
  m_nodeMenu->AppendSeparator();
  m_nodeMenu->Append(efgmenuEDIT_PROPERTIES, "Properties",
		     "View and change node properties");

  m_gameMenu = new wxMenu;
  m_gameMenu->Append(efgmenuEDIT_GAME, "Properties",
		     "View and change game properties");
}

//---------------------------------------------------------------------
//                  TreeWindow: Event-hook members
//---------------------------------------------------------------------

static Node *PriorSameIset(const Node *n)
{
    Infoset *iset = n->GetInfoset();
    if (!iset) return 0;
    for (int i = 1; i <= iset->NumMembers(); i++)
        if (iset->Members()[i] == n)
            if (i > 1)
                return iset->Members()[i-1];
            else 
                return 0;
    return 0;
}

static Node *NextSameIset(const Node *n)
{
    Infoset *iset = n->GetInfoset();
    if (!iset) return 0;
    for (int i = 1; i <= iset->NumMembers(); i++)
        if (iset->Members()[i] == n)
            if (i < iset->NumMembers()) 
                return iset->Members()[i+1]; 
            else
                return 0;
    return 0;
}

//
// OnKeyEvent -- handle keypress events
// Currently we support the following keys:
//     left arrow:   go to parent of current node
//     right arrow:  go to first child of current node
//     up arrow:     go to previous sibling of current node
//     down arrow:   go to next sibling of current node
// Since the addition of collapsible subgames, a node's parent may not
// be visible in the current display.  Thus, find the first predecessor
// that is visible (ROOT is always visible)
//
void TreeWindow::OnKeyEvent(wxKeyEvent &p_event)
{
  if (Cursor() && !p_event.ShiftDown()) {
    bool c = false;   // set to true if cursor position has changed
    switch (p_event.KeyCode()) {
    case WXK_LEFT:
      if (Cursor()->GetParent()) {
	SetCursorPosition(m_layout.GetValidParent(Cursor())->GetNode());
	c = true;
      }
      break;
    case WXK_RIGHT:
      if (m_layout.GetValidChild(Cursor())) {
	SetCursorPosition(m_layout.GetValidChild(Cursor())->GetNode());
	c = true;
      }
      break;
    case WXK_UP: {
      Node *prior = ((!p_event.ControlDown()) ? 
		     m_layout.PriorSameLevel(Cursor()) :
		     PriorSameIset(Cursor()));
      if (prior) {
	SetCursorPosition(prior);
	c = true;
      }
      break;
    }
    case WXK_DOWN: {
      Node *next = ((!p_event.ControlDown()) ?
		    m_layout.NextSameLevel(Cursor()) :
		    NextSameIset(Cursor()));
      if (next) {
	SetCursorPosition(next);
	c = true;
      }
      break;
    }
    case WXK_SPACE:
      // Force a scroll to be sure selected node is visible
      c = true;
      break;
    }
        
    if (c) { 
      ProcessCursor(); // cursor moved
    }
  }
  else {
    p_event.Skip();
  }
}

//---------------------------------------------------------------------
//                   TreeWindow: Drawing functions
//---------------------------------------------------------------------

void TreeWindow::RefreshTree(void)
{
  m_layout.BuildNodeList(*m_parent->GetSupport());
  m_layout.Layout(*m_parent->GetSupport());
  AdjustScrollbarSteps();
}

void TreeWindow::RefreshLayout(void)
{
  m_layout.Layout(*m_parent->GetSupport());
  AdjustScrollbarSteps();
}

void TreeWindow::RefreshLabels(void)
{
  m_layout.GenerateLabels();
  Refresh();
}

void TreeWindow::AdjustScrollbarSteps(void)
{
  int width, height;
  GetClientSize(&width, &height);

  int scrollX, scrollY;
  GetViewStart(&scrollX, &scrollY);

  const int OUTCOME_LENGTH = 60;

  SetScrollbars(50, 50,
		(int) ((m_layout.MaxX() + m_drawSettings.NodeSize() + 
			OUTCOME_LENGTH) * m_zoom / 50 + 1),
		(int) (m_layout.MaxY() * m_zoom / 50 + 1),
		scrollX, scrollY);
}

void TreeWindow::FitZoom(void)
{
  int width, height;
  GetClientSize(&width, &height);

  double zoomx = (double) width / (double) m_layout.MaxX();
  double zoomy = (double) height / (double) m_layout.MaxY();

  zoomx = gmin(zoomx, 1.0); 
  zoomy = gmin(zoomy, 1.0);  // never zoom in (only out)
  m_zoom = gmin(zoomx, zoomy) * .9;
}

void TreeWindow::SetZoom(float p_zoom)
{
  m_zoom = p_zoom;
  AdjustScrollbarSteps();
  EnsureCursorVisible();
  Refresh();
}

void TreeWindow::OnDraw(wxDC &dc)
{
  dc.SetUserScale(m_zoom, m_zoom);
    
  if (Cursor()) {
    if (!m_layout.GetNodeEntry(Cursor())) {
      SetCursorPosition(m_efg.RootNode());
    }
    
    UpdateCursor();
  }
    
  dc.BeginDrawing();
  dc.Clear();
  m_layout.Render(dc);
  //  flasher->Flash(dc);
  dc.EndDrawing();
}

void TreeWindow::EnsureCursorVisible(void)
{
  NodeEntry *entry = m_layout.GetNodeEntry(Cursor()); 
  int xScroll, yScroll;
  GetViewStart(&xScroll, &yScroll);
  int width, height;
  GetClientSize(&width, &height);

  int xx, yy;
  CalcScrolledPosition((int) (entry->X() * m_zoom - 20),
		       (int) (entry->Y() * m_zoom), &xx, &yy);
  if (xx < 0) {
    xScroll -= -xx / 50 + 1;
  }
  const int OUTCOME_LENGTH = 60;
  CalcScrolledPosition((int) (entry->X() * m_zoom + m_drawSettings.NodeSize() +
			      OUTCOME_LENGTH),
		       (int) (entry->Y() * m_zoom), &xx, &yy);
  if (xx > width) {
    xScroll += (xx - width) / 50 + 1;
  }
  if (xScroll < 0) {
    xScroll = 0;
  }
  else if (xScroll > GetScrollRange(wxHORIZONTAL)) {
    xScroll = GetScrollRange(wxHORIZONTAL);
  }

  CalcScrolledPosition((int) (entry->X() * m_zoom),
		       (int) (entry->Y() * m_zoom - 20), &xx, &yy);
  if (yy < 0) {
    yScroll -= -yy / 50 + 1;
  }
  CalcScrolledPosition((int) (entry->X() * m_zoom),
		       (int) (entry->Y() * m_zoom + 20), &xx, &yy);
  if (yy > height) {
    yScroll += (yy - height) / 50 + 1;
  }
  if (yScroll < 0) {
    yScroll = 0;
  }
  else if (yScroll > GetScrollRange(wxVERTICAL)) {
    yScroll = GetScrollRange(wxVERTICAL);
  } 

  Scroll(xScroll, yScroll);
}

void TreeWindow::ProcessCursor(void)
{
  if (Cursor()) {
    NodeEntry *entry = m_layout.GetNodeEntry(Cursor()); 
    if (!entry) {
      SetCursorPosition(m_efg.RootNode());
      entry = m_layout.GetNodeEntry(Cursor());
    }
    
    UpdateCursor();
    EnsureCursorVisible();
  }
  Refresh();
  m_parent->OnSelectedMoved(Cursor());
}

void TreeWindow::UpdateCursor(void)
{
  NodeEntry *entry = m_layout.GetNodeEntry(Cursor());

  if (!entry) {
    return;
  }

  entry->SetCursor(true);
  //  Refresh();

#ifdef UNUSED
  if (entry->n->GetSubgameRoot() == entry->n && !entry->expanded) {
    flasher->SetFlashNode(entry->x + DrawSettings().NodeLength() +
			  entry->nums*INFOSET_SPACING - SUBGAME_LARGE_ICON_SIZE,
			  entry->y,
			  entry->x + DrawSettings().NodeLength() +
			  entry->nums*INFOSET_SPACING,
			  entry->y, subgameCursor);
  }
  else {
    flasher->SetFlashNode(entry->x + entry->nums*INFOSET_SPACING, entry->y,
			  entry->x + DrawSettings().NodeLength() + 
			  entry->nums*INFOSET_SPACING - 8,
			  entry->y, nodeCursor);
  }

  wxClientDC dc(this);
  PrepareDC(dc);
  dc.SetUserScale(m_zoom, m_zoom);
  flasher->Flash(dc);
#endif  // UNUSED
}

gText TreeWindow::OutcomeAsString(const Node *n) const
{
  Efg::Outcome outcome = n->Game()->GetOutcome(n);
  if (!outcome.IsNull()) {
    const gArray<gNumber> &v = n->Game()->Payoff(outcome);
    gText tmp = "(";

    for (int i = v.First(); i <= v.Last(); i++) {
      if (i != 1) 
	tmp += ",";
      /*      
      if (DrawSettings().ColorCodedOutcomes())
	tmp += ("\\C{"+ToText(DrawSettings().GetPlayerColor(i))+"}");
      */
      tmp += ToText(v[i], NumDecimals());
    }
    /*
    if (DrawSettings().ColorCodedOutcomes()) 
      tmp += ("\\C{"+ToText(WX_COLOR_LIST_LENGTH-1)+"}");
    */
    tmp += ")";
        
    return tmp;
  }
  else
    return "";
}

void TreeWindow::OnMouseMotion(wxMouseEvent &p_event)
{
  if (p_event.LeftIsDown() && p_event.Dragging()) {
    if (!m_dragImage) {
      int x, y;
      CalcUnscrolledPosition(p_event.GetX(), p_event.GetY(), &x, &y);
      x = (int) ((float) x / m_zoom);
      y = (int) ((float) y / m_zoom);

      Node *node = m_layout.NodeHitTest(x, y);
    
      if (node && m_efg.NumChildren(node) > 0) {
	wxPoint hotSpot(p_event.GetPosition().x, p_event.GetPosition().y);
#ifdef __WXMSW__
	if (p_event.ControlDown()) {
	  m_dragImage = new wxDragImage(wxBitmap("COPY_BITMAP"),
					wxCursor(wxCURSOR_HAND),
					hotSpot);
	  m_dragMode = dragCOPY;
	}
	else {
	  m_dragImage = new wxDragImage(wxBitmap("MOVE_BITMAP"),
					wxCursor(wxCURSOR_HAND),
					hotSpot);
	  m_dragMode = dragMOVE;
	}
#else
#include "bitmaps/copy.xpm"
#include "bitmaps/move.xpm"
	if (p_event.ControlDown()) {
	  m_dragImage = new wxDragImage(wxBitmap(copy_xpm),
					wxCursor(wxCURSOR_HAND), hotSpot);
	  m_dragMode = dragCOPY;
	}
	else {
	  m_dragImage = new wxDragImage(wxBitmap(move_xpm),
					wxCursor(wxCURSOR_HAND), hotSpot);
	  m_dragMode = dragMOVE;
	}
#endif  // __WXMSW__
	m_dragImage->BeginDrag(wxPoint(0, 0), this);
	m_dragImage->Move(p_event.GetPosition());
	m_dragImage->Show();
	m_dragSource = node;
	return;
      }
    }
    else {
      m_dragImage->Move(p_event.GetPosition());
    }
  }
  else if (!p_event.LeftIsDown() && m_dragImage) {
    m_dragImage->Hide();
    m_dragImage->EndDrag();
    delete m_dragImage;
    m_dragImage = 0;

    int x, y;
    CalcUnscrolledPosition(p_event.GetX(), p_event.GetY(), &x, &y);
    x = (int) ((float) x / m_zoom);
    y = (int) ((float) y / m_zoom);

    Node *node = m_layout.NodeHitTest(x, y);
    if (node) {
      try {
	if (m_dragMode == dragCOPY) {
	  m_efg.CopyTree(m_dragSource, node);
	}
	else if (m_dragMode == dragMOVE) {
	  m_efg.MoveTree(m_dragSource, node);
	}
	else if (m_dragMode == dragOUTCOME) { 
	  node->Game()->SetOutcome(node,
				   m_dragSource->Game()->GetOutcome(m_dragSource));
	}
      }
      catch (gException &ex) {
	guiExceptionDialog(ex.Description(), this);
      }
      Refresh();
    }
  }

  // Check all the draggers.  Note that they are mutually exclusive
  if (!m_branchDragger->Dragging()) {
    if (m_infosetDragger->OnEvent(p_event) != DRAG_NONE) return;
  }

  if (!m_infosetDragger->Dragging()) {
    if (m_branchDragger->OnEvent(p_event) != DRAG_NONE) return;
  }
}    

void TreeWindow::OnLeftClick(wxMouseEvent &p_event)
{
  if (ProcessShift(p_event))  {
    return;
  }   
 
  // Check all the draggers.  Note that they are mutually exclusive
  if (!m_branchDragger->Dragging()) {
    if (m_infosetDragger->OnEvent(p_event) != DRAG_NONE) return;
  }

  if (!m_infosetDragger->Dragging()) {
    if (m_branchDragger->OnEvent(p_event) != DRAG_NONE) return;
  }
    
  int x, y;
  CalcUnscrolledPosition(p_event.GetX(), p_event.GetY(), &x, &y);
  x = (int) ((float) x / m_zoom);
  y = (int) ((float) y / m_zoom);

  Node *node = m_layout.NodeHitTest(x, y);
  SetCursorPosition(node);
  Refresh();
  ProcessCursor();
}

void TreeWindow::OnLeftDoubleClick(wxMouseEvent &p_event)
{
  int x, y;
  CalcUnscrolledPosition(p_event.GetX(), p_event.GetY(), &x, &y);
  x = (int) ((float) x / m_zoom);
  y = (int) ((float) y / m_zoom);

  Node *node = m_layout.NodeHitTest(x, y);
  if (node) {
    SetCursorPosition(node);
    Refresh();
    return;
  }
}

void TreeWindow::OnRightClick(wxMouseEvent &p_event)
{
  int x, y;
  CalcUnscrolledPosition(p_event.GetX(), p_event.GetY(), &x, &y);
  x = (int) ((float) x / m_zoom);
  y = (int) ((float) y / m_zoom);

  wxClientDC dc(this);
  PrepareDC(dc);
  dc.SetUserScale(m_zoom, m_zoom);

  Node *node = m_layout.NodeHitTest(x, y);
  if (node) {
    SetCursorPosition(node);
    Refresh();
    PopupMenu(m_nodeMenu, p_event.GetX(), p_event.GetY());
  }
  else {
    // If right-click doesn't hit anything, display generic game menu
    SetCursorPosition(0);
    Refresh();
    PopupMenu(m_gameMenu, p_event.GetX(), p_event.GetY());
  }
}

bool TreeWindow::ProcessShift(wxMouseEvent &ev)
{
  if (!ev.ShiftDown()) {
    return false;
  }

  long x, y;
  ev.GetPosition(&x, &y);

  Node *node = m_layout.NodeHitTest(x, y);
  if (node) {
    m_efg.DeleteTree(node);
    m_efg.DeleteEmptyInfosets();
    Refresh();
    return true;
  }

  node = m_layout.BranchHitTest(x, y);
  if (node) {
    m_efg.DeleteAction(node->GetParent()->GetInfoset(),
		       LastAction(m_efg, node));
    Refresh();
    return true;
  }

  node = m_layout.InfosetHitTest(x, y);
  if (node) {
    Infoset *iset = m_efg.SplitInfoset(node);
    iset->SetName("Infoset" + ToText(iset->GetPlayer()->NumInfosets()));
    Refresh();
    return true;
  }

  return false;
}

void TreeWindow::SupportChanged(void)
{
  if (!m_layout.GetNodeEntry(Cursor())) {
    SetCursorPosition(0);
  }
  RefreshLayout();
  Refresh();
}

Node *TreeWindow::GotObject(long &x, long &y, int what)
{
#ifdef NOT_PORTED_YET
  for (int i = 1; i <= m_layout.Length(); i++) {
    NodeEntry *entry = m_layout[i];
        
    if (what == DRAG_NODE_START) // check if clicked a non terminal node
      if (entry->n->Game()->NumChildren(entry->n) != 0)
	if(x > entry->x+entry->nums*INFOSET_SPACING &&
	   x < entry->x+draw_settings.NodeLength()+
	   entry->nums*INFOSET_SPACING-10 &&
	   y > entry->y-DELTA && y < entry->y+DELTA) 
	  return (Node *)entry->n;
        
    if (what == DRAG_NODE_END) // check if clicked on a terminal node
      if (entry->n->Game()->NumChildren(entry->n) == 0)
	if(x > entry->x+entry->nums*INFOSET_SPACING &&
	   x < entry->x+draw_settings.NodeLength()+entry->nums*INFOSET_SPACING &&
	   y > entry->y-DELTA && y < entry->y+DELTA)
	  return (Node *)entry->n;
    
    if (what == DRAG_OUTCOME_START) // check if clicked on a terminal node
      if (entry->n->Game()->NumChildren(entry->n) == 0 && entry->n->GetOutcome())
	if(x > entry->x+entry->nums*INFOSET_SPACING+draw_settings.NodeLength() &&
	   x < entry->x+draw_settings.NodeLength()+
	   entry->nums*INFOSET_SPACING + draw_settings.OutcomeLength()&&
	   y > entry->y-DELTA && y < entry->y+DELTA)
	  return (Node *)entry->n;
    
    if (what == DRAG_OUTCOME_END) // check if clicked on any valid node
      if(x > entry->x+entry->nums*INFOSET_SPACING && 
	 x < entry->x+draw_settings.NodeLength()+entry->nums*INFOSET_SPACING &&
	 y > entry->y-DELTA && y < entry->y+DELTA)
	return (Node *)entry->n;
    
    if (what == DRAG_ISET_START || what == DRAG_ISET_END)
      // check if clicked on a non terminal node
      if (entry->n->Game()->NumChildren(entry->n) != 0)
	if(x > entry->x+entry->num*INFOSET_SPACING-4 && 
	   x < entry->x+entry->num*INFOSET_SPACING+4 &&
	   y > entry->y-4 && y < entry->y+4) {
	  x = entry->x+entry->num*INFOSET_SPACING;
	  y = entry->y;
	  return (Node *)entry->n;
	}
    
    if (what == DRAG_BRANCH_START)
      // check if clicked on the very end of a node
      if (x > entry->x+draw_settings.NodeLength()+
	  entry->nums*INFOSET_SPACING-4 &&
	  x < entry->x+draw_settings.NodeLength()+
	  entry->nums*INFOSET_SPACING+4 &&
	  y > entry->y-4 && y < entry->y+4) {
	x = entry->x+draw_settings.NodeLength()+entry->nums*INFOSET_SPACING;
	y = entry->y;
	return (Node *)entry->n;
      }
        
    if (what == DRAG_BRANCH_END) {
      // check if released in a valid position
      NodeEntry *start_entry = m_layout.GetNodeEntry(m_branchDragger->StartNode());
      int xs = start_entry->x+draw_settings.NodeLength()+
	draw_settings.ForkLength()+start_entry->nums*INFOSET_SPACING;
      if (x > xs && x < xs+draw_settings.BranchLength() &&
	  y < start_entry->y+(start_entry->n->Game()->NumChildren(start_entry->n)+1)*draw_settings.YSpacing() &&
	  y > start_entry->y-(start_entry->n->Game()->NumChildren(start_entry->n)+1)*draw_settings.YSpacing()) {
	// figure out at what branch # the mouse was released
	int br = 1;
	NodeEntry *child_entry, *child_entry1;
	for (int ii = 1; ii <= start_entry->n->Game()->NumChildren(start_entry->n)-1; ii++) {
	  child_entry = m_layout.GetNodeEntry(start_entry->n->GetChild(ii));
	  if (ii == 1) 
	    if (y < child_entry->y) {
	      br = 1;
	      break;
	    }
	  child_entry1 = m_layout.GetNodeEntry(start_entry->n->GetChild(ii+1));
	  if (y > child_entry->y && y < child_entry1->y) {
	    br = ii+1;
	    break;
	  }
	  if (ii == start_entry->n->Game()->NumChildren(start_entry->n)-1 && y > child_entry1->y) {
	    br = start_entry->n->Game()->NumChildren(start_entry->n)+1;
	    break;
	  }
	}
	x = br;
	return (Node *)start_entry->n;
      }
      else
	return 0;
    }
  }
#endif // NOT_PORTED_YET
  return 0;
}

void TreeWindow::SetCursorPosition(Node *p_cursor)
{
  if (m_cursor) {
    m_layout.GetNodeEntry(m_cursor)->SetCursor(false);
    m_layout.GetNodeEntry(m_cursor)->SetSelected(false);
  }
  m_cursor = p_cursor;
  m_parent->UpdateMenus();
}

void TreeWindow::UpdateMenus(void)
{
  m_nodeMenu->Enable(efgmenuEDIT_NODE_ADD,
		     (!m_cursor || m_efg.NumChildren(m_cursor) > 0) ? false : true);
}

//-----------------------------------------------------------------------
//                    NODE MENU HANDLER FUNCTIONS
//-----------------------------------------------------------------------

//------------------------
// Edit->Node->Set Mark
//------------------------

void TreeWindow::node_set_mark(void)
{
  if (m_markNode != Cursor())
    m_markNode = Cursor();
  else
    m_markNode = 0;
  m_parent->UpdateMenus();
}

//-------------------------
// Edit->Node->Goto Mark
//-------------------------

void TreeWindow::node_goto_mark(void)
{
  if (m_markNode) {
    SetCursorPosition(m_markNode);
    ProcessCursor();
  }
}

//-----------------------------------------------------------------------
//                     SUBGAME MENU HANDLER FUNCTIONS
//-----------------------------------------------------------------------

//----------------------
// Subgames->Mark All
//----------------------

void TreeWindow::SubgameMarkAll(void)
{
  gList<Node *> subgame_roots;
  LegalSubgameRoots(m_efg, subgame_roots);
  m_efg.MarkSubgames(subgame_roots);
  RefreshLayout();
}

void TreeWindow::SubgameMark(void)
{
  if (Cursor()->GetSubgameRoot() == Cursor()) {
    // ignore silently
    return;
  }

  if (!m_efg.IsLegalSubgame(Cursor())) {
    wxMessageBox("This node is not a root of a valid subgame"); 
    return;
  }

  m_efg.MarkSubgame(Cursor());
  RefreshLayout();
}

void TreeWindow::SubgameUnmark(void)
{
  if (Cursor()->GetSubgameRoot() != Cursor() ||
      Cursor()->GetSubgameRoot() == m_efg.RootNode())
    return;
    
  m_efg.UnmarkSubgame(Cursor());
  RefreshLayout();
}

void TreeWindow::SubgameUnmarkAll(void)
{
  m_efg.UnmarkSubgames(m_efg.RootNode());
  RefreshLayout();
}

//-----------------------------------------------------------------------
//                     DISPLAY MENU HANDLER FUNCTIONS
//-----------------------------------------------------------------------

void TreeWindow::OnSize(wxSizeEvent &p_event)
{
  if (m_layout.MaxX() == 0 || m_layout.MaxY() == 0) {
    m_layout.Layout(*m_parent->GetSupport());
  }

  // This extra check because wxMSW seems to generate OnSize events
  // rather liberally (e.g., a size of (0,0) for minimizing the window)
  if (p_event.GetSize().GetWidth() == 0 ||
      p_event.GetSize().GetHeight() == 0) {
    return;
  }

  /*
  double zoomx = ((double) p_event.GetSize().GetWidth() /
		  (double) m_layout.MaxX());
  double zoomy = ((double) p_event.GetSize().GetHeight() /
		  (double) m_layout.MaxY());
    
  zoomx = gmin(zoomx, 1.0);
  zoomy = gmin(zoomy, 1.0);
  m_zoom = gmin(zoomx, zoomy); 
  */
  AdjustScrollbarSteps();

  Refresh();
}

template class gList<NodeEntry *>;

