using System;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;
using System.Collections.Generic;
using System.Windows.Forms.VisualStyles;

using Abstractspoon.Tdl.PluginHelpers;
using Abstractspoon.Tdl.PluginHelpers.ColorUtil;

namespace MindMapUIExtension
{
    public delegate bool EditTaskLabelEventHandler(object sender, UInt32 taskId);
    public delegate bool EditTaskIconEventHandler(object sender, UInt32 taskId);
    public delegate bool EditTaskCompletionEventHandler(object sender, UInt32 taskId, bool completed);

	class MindMapTaskItem
	{
		// Data
		private String m_Title;
		private UInt32 m_TaskID;
        private UInt32 m_ParentID;
		private UInt32 m_ReferenceId;
		private List<UInt32> m_LocalDepends;
		private Color m_TextColor;
		private bool m_HasIcon;
		private bool m_IsFlagged;
		private bool m_IsParent;
        private bool m_IsDone;
        private bool m_IsGoodAsDone;
        private bool m_SomeSubtasksDone;
		private bool m_IsLocked;

		// -----------------------------------------------------------------

		public MindMapTaskItem(String label)
		{
			m_Title = label;
			m_TaskID = 0;
            m_ParentID = 0;
			m_TextColor = new Color();
			m_HasIcon = false;
			m_IsFlagged = false;
			m_IsParent = false;
			m_IsDone = false;
            m_IsGoodAsDone = false;
            m_SomeSubtasksDone = false;
			m_IsLocked = false;
			m_ReferenceId = 0;
			m_LocalDepends = null;
		}

		public MindMapTaskItem(Task task)
		{
			m_Title = task.GetTitle();
			m_TaskID = task.GetID();
			m_ParentID = task.GetParentID();
			m_TextColor = task.GetTextDrawingColor();
			m_HasIcon = (task.GetIcon().Length > 0);
			m_IsFlagged = task.IsFlagged(false);
			m_IsParent = task.IsParent();
            m_IsDone = task.IsDone();
            m_IsGoodAsDone = task.IsGoodAsDone();
            m_SomeSubtasksDone = task.HasSomeSubtasksDone();
			m_IsLocked = task.IsLocked(true);
			m_ReferenceId = task.GetReferenceID();
			m_LocalDepends = task.GetLocalDependency();
		}

		public void FixupParentID(MindMapTaskItem parent)
		{
            m_ParentID = parent.ID;
		}

        public bool FixupParentalStatus(int nodeCount, UIExtension.TaskIcon taskIcons)
        {
            bool wasParent = m_IsParent;

            if (nodeCount == 0)
            {
                m_IsParent = (!m_HasIcon && taskIcons.Get(m_TaskID));
            }
            else
            {
                m_IsParent = true;
            }

            return (m_IsParent != wasParent);
        }

		public override string ToString() 
		{
			return Title;
		}

		public void Update(Task task, HashSet<Task.Attribute> attribs)
		{
			// TODO
		}

		public String Title 
		{ 
			get 
			{ 
#if DEBUG
				return String.Format("{0} ({1})", m_Title, m_TaskID); 
#else
				return m_Title;
#endif
			} 

			set // only works for the root
			{
				if (!IsTask && !String.IsNullOrWhiteSpace(value))
					m_Title = value;
			}
		}
		
		public UInt32 ID { get { return m_TaskID; } }
        public UInt32 ParentID { get { return m_ParentID; } }
		public UInt32 ReferenceId { get { return m_ReferenceId; } }
		public Color TextColor { get { return m_TextColor; } }
		public bool HasIcon { get { return m_HasIcon; } }
		public bool IsFlagged { get { return m_IsFlagged; } }
		public bool IsParent { get { return m_IsParent; } }
		public bool IsLocked { get { return m_IsLocked; } }
		public bool IsReference { get { return (m_ReferenceId != 0); } }
		public bool IsTask { get { return (m_TaskID != 0); } }
        public bool HasSomeSubtasksDone { get { return m_SomeSubtasksDone; } }
		public bool HasLocalDependencies {  get { return (m_LocalDepends != null) && (m_LocalDepends.Count > 0); } }
		public List<UInt32> LocalDependencies { get { return m_LocalDepends; } }

		public bool IsDone(bool includeGoodAsDone) 
        { 
            if (includeGoodAsDone && m_IsGoodAsDone)
                return true;

            return m_IsDone; 
        }

		public bool SetDone(bool done = true)
		{
			if (m_IsDone == done)
				return false;

			m_IsDone = done;
			return true;
		}

		public bool ProcessTaskUpdate(Task task)
		{
			if (task.GetID() != m_TaskID)
				return false;

			if (task.IsAttributeAvailable(Task.Attribute.Title))
				m_Title = task.GetTitle();

			if (task.IsAttributeAvailable(Task.Attribute.Icon))
				m_HasIcon = (task.GetIcon().Length > 0);

			if (task.IsAttributeAvailable(Task.Attribute.Flag))
				m_IsFlagged = task.IsFlagged(false);

			if (task.IsAttributeAvailable(Task.Attribute.Color))
				m_TextColor = task.GetTextDrawingColor();

            if (task.IsAttributeAvailable(Task.Attribute.SubtaskDone))
                m_SomeSubtasksDone = task.HasSomeSubtasksDone();

            if (task.IsAttributeAvailable(Task.Attribute.DoneDate))
                m_IsDone = task.IsDone();

			m_IsParent = task.IsParent();
			m_IsLocked = task.IsLocked(true);
            m_IsGoodAsDone = task.IsGoodAsDone();
			m_ReferenceId = task.GetReferenceID();

			return true;
		}
	}

	// ------------------------------------------------------------

	[System.ComponentModel.DesignerCategory("")]

	class TdlMindMapControl : MindMapControl
	{
		public event EditTaskLabelEventHandler      EditTaskLabel;
        public event EditTaskIconEventHandler       EditTaskIcon;
        public event EditTaskCompletionEventHandler EditTaskDone;

		// From Parent
		private Translator m_Trans;
		private UIExtension.TaskIcon m_TaskIcons;
		private Dictionary<UInt32, MindMapTaskItem> m_Items;

		private bool m_ShowParentAsFolder;
		private bool m_TaskColorIsBkgnd;
		private bool m_IgnoreMouseClick;
        private bool m_ShowCompletionCheckboxes;
		private bool m_StrikeThruDone;

		private TreeNode m_PreviouslySelectedNode;
		private Timer m_EditTimer;
        private Font m_BoldLabelFont, m_DoneLabelFont, m_BoldDoneLabelFont;
        private Size m_CheckboxSize;
		private Pen m_DependencyPen;

		// -------------------------------------------------------------------------

		public TdlMindMapControl(Translator trans, UIExtension.TaskIcon icons)
		{
			m_Trans = trans;
			m_TaskIcons = icons;

			m_Items = new Dictionary<UInt32, MindMapTaskItem>();

			m_DependencyPen = new Pen(Color.Black);
			m_DependencyPen.DashStyle = DashStyle.Dot;

			m_TaskColorIsBkgnd = false;
			m_IgnoreMouseClick = false;
			m_ShowParentAsFolder = false;
            m_ShowCompletionCheckboxes = true;
			m_StrikeThruDone = true;

			m_EditTimer = new Timer();
			m_EditTimer.Interval = 500;
			m_EditTimer.Tick += new EventHandler(OnEditLabelTimer);

            using (Graphics graphics = Graphics.FromHwnd(Handle))
                m_CheckboxSize = CheckBoxRenderer.GetGlyphSize(graphics, CheckBoxState.UncheckedNormal);
		}
        
        public void SetStrikeThruDone(bool strikeThruDone)
		{
			m_StrikeThruDone = strikeThruDone;

			if (m_BoldLabelFont != null)
				SetFont(m_BoldLabelFont.Name, (int)m_BoldLabelFont.Size, m_StrikeThruDone);
		}

		public new void SetFont(String fontName, int fontSize)
		{
			SetFont(fontName, fontSize, m_StrikeThruDone);
		}

		protected void SetFont(String fontName, int fontSize, bool strikeThruDone)
		{
			bool baseFontChange = ((m_BoldLabelFont == null) || (m_BoldLabelFont.Name != fontName) || (m_BoldLabelFont.Size != fontSize));
            bool doneFontChange = (baseFontChange || (m_BoldDoneLabelFont.Strikeout != strikeThruDone));

            if (baseFontChange)
                m_BoldLabelFont = new Font(fontName, fontSize, FontStyle.Bold);

            if (doneFontChange)
            {
                if (strikeThruDone)
                {
                    m_BoldDoneLabelFont = new Font(fontName, fontSize, FontStyle.Bold | FontStyle.Strikeout);
                    m_DoneLabelFont = new Font(fontName, fontSize, FontStyle.Strikeout);
                }
                else
                {
                    m_BoldDoneLabelFont = m_BoldLabelFont;
                    m_DoneLabelFont = null;
                }
            }

            if ((baseFontChange || doneFontChange) && RefreshItemFont(RootNode, true))
                RecalculatePositions();
            
            base.SetFont(fontName, fontSize);
        }

		public void UpdateTasks(TaskList tasks, UIExtension.UpdateType type)
		{
			switch (type)
			{
				case UIExtension.UpdateType.Edit:
				case UIExtension.UpdateType.New:
					if (RootNode == null)
						RebuildTreeView(tasks);
					else
						UpdateTaskAttributes(tasks);
					break;

				case UIExtension.UpdateType.Delete:
				case UIExtension.UpdateType.All:
					RebuildTreeView(tasks);
					break;

				case UIExtension.UpdateType.Unknown:
					return;
			}

			// If the root task is not a real task update the project name
			if (tasks.IsAttributeAvailable(Task.Attribute.ProjectName))
			{
				var rootItem = TaskItem(RootNode);
	
				if ((rootItem != null) && !rootItem.IsTask)
				{
					var projName = GetProjectName(tasks);
	
					if (!projName.Equals(rootItem.Title))
					{
						rootItem.Title = projName;
						RootNode.Text = projName;
					}
				}
			}
		}

		public bool SelectNodeWasPreviouslySelected
		{
			get { return (SelectedNode == m_PreviouslySelectedNode); }
		}

		public bool TaskColorIsBackground
		{
			get { return m_TaskColorIsBkgnd; }
			set
			{
				if (m_TaskColorIsBkgnd != value)
				{
					m_TaskColorIsBkgnd = value;
					Invalidate();
				}
			}
		}

		public bool ShowParentsAsFolders
		{
			get { return m_ShowParentAsFolder; }
			set
			{
				if (m_ShowParentAsFolder != value)
				{
					m_ShowParentAsFolder = value;
					Invalidate();
				}
			}
		}

        public bool ShowCompletionCheckboxes
        {
            get { return m_ShowCompletionCheckboxes; }
            set
            {
                if (m_ShowCompletionCheckboxes != value)
                {
                    m_ShowCompletionCheckboxes = value;
                    RecalculatePositions();
                }
            }
        }

        public bool WantTaskUpdate(Task.Attribute attrib)
        {
            switch (attrib)
            {
                // Note: lock state is always provided
                case Task.Attribute.Title:
                case Task.Attribute.Icon:
                case Task.Attribute.Flag:
                case Task.Attribute.Color:
                case Task.Attribute.DoneDate:
			    case Task.Attribute.Position:
			    case Task.Attribute.SubtaskDone:
				case Task.Attribute.ProjectName:
				case Task.Attribute.Dependency:
					return true;
            }

            // all else
            return false;
        }
        		
		public UInt32 HitTest(Point screenPos)
		{
			var clientPos = PointToClient(screenPos);
			var node = HitTestPositions(clientPos);

			if (node != null)
				return UniqueID(node);
			
			// else
			return 0;
		}

		public new Rectangle GetSelectedItemLabelRect()
		{
			EnsureItemVisible(SelectedItem);

			var labelRect = base.GetSelectedItemLabelRect();

			labelRect.X -= LabelPadding;
			labelRect.X += GetExtraWidth(SelectedNode);

			return labelRect;
		}

		public bool CanMoveTask(UInt32 taskId, UInt32 destParentId, UInt32 destPrevSiblingId)
		{
			if (FindNode(taskId) == null)
				return false;

			if (FindNode(destParentId) == null)
				return false;

			if ((destPrevSiblingId != 0) && (FindNode(destPrevSiblingId) == null))
				return false;

			return true;
		}

		public bool MoveTask(UInt32 taskId, UInt32 destParentId, UInt32 destPrevSiblingId)
		{
			BeginUpdate();

            var node = FindNode(taskId);
   			var prevParentNode = node.Parent;

			var destParentNode = FindNode(destParentId);
			var destPrevSiblingNode = FindNode(destPrevSiblingId);

			if ((node == null) || (destParentNode == null))
				return false;

			var srcParentNode = node.Parent;
			int srcPos = srcParentNode.Nodes.IndexOf(node);

			srcParentNode.Nodes.RemoveAt(srcPos);

			int destPos = 0; // insert at top
			
			if (destPrevSiblingNode != null)
				destPos = (destParentNode.Nodes.IndexOf(destPrevSiblingNode) + 1);

			destParentNode.Nodes.Insert(destPos, node);

            FixupParentalStatus(destParentNode, 1);
            FixupParentalStatus(prevParentNode, -1);

            FixupParentID(node, destParentNode);

			SelectedNode = node;

			EndUpdate();
			EnsureItemVisible(Item(node));

			return true;
		}

        public bool SelectTask(String text, UIExtension.SelectTask selectTask, bool caseSensitive, bool wholeWord, bool findReplace)
        {
            if ((text == String.Empty) || IsEmpty())
                return false;

			TreeNode node = null; // start node
			bool forward = true;

			switch (selectTask)
            {
            case UIExtension.SelectTask.SelectFirstTask:
				node = RootNode.Nodes[0];
                break;

            case UIExtension.SelectTask.SelectNextTask:
				node = TreeCtrl.GetNextItem(SelectedNode);
                break;

            case UIExtension.SelectTask.SelectNextTaskInclCurrent:
				node = SelectedNode;
				break;

            case UIExtension.SelectTask.SelectPrevTask:
				node = TreeCtrl.GetPrevItem(SelectedNode);

				if ((node == null) || ((node == RootNode) && !NodeIsTask(RootNode)))
					node = LastNode;

				forward = false;
				break;

            case UIExtension.SelectTask.SelectLastTask:
				node = LastNode;
				forward = false;
				break;
            }
			
			// Avoid recursion
			while (node != null)
			{ 
				if (StringUtil.Find(node.Text, text, caseSensitive, wholeWord))
				{
					SelectedNode = node;
					return true;
				}

				if (forward)
					node = TreeCtrl.GetNextItem(node);
				else
					node = TreeCtrl.GetPrevItem(node);
			}

			return false;
		}

		public bool GetTask(UIExtension.GetTask getTask, ref UInt32 taskID)
		{
			TreeNode node = FindNode(taskID);

			if (node == null)
				return false;

			switch (getTask)
			{
				case UIExtension.GetTask.GetNextTask:
					if (node.NextNode != null)
					{
						taskID = UniqueID(node.NextNode);
						return true;
					}
					break;

				case UIExtension.GetTask.GetPrevTask:
					if (node.PrevVisibleNode != null)
					{
						taskID = UniqueID(node.PrevNode);
						return true;
					}
					break;

				case UIExtension.GetTask.GetNextVisibleTask:
					if (node.NextVisibleNode != null)
					{
						taskID = UniqueID(node.NextVisibleNode);
						return true;
					}
					break;

				case UIExtension.GetTask.GetPrevVisibleTask:
					if (node.PrevVisibleNode != null)
					{
						taskID = UniqueID(node.PrevVisibleNode);
						return true;
					}
					break;

				case UIExtension.GetTask.GetNextTopLevelTask:
					{
						var topLevelParent = TopLevelParent(node);

						if ((topLevelParent != null) && (topLevelParent.NextNode != null))
						{
							taskID = UniqueID(topLevelParent.NextNode);
							return true;
						}
					}
					break;

				case UIExtension.GetTask.GetPrevTopLevelTask:
					{
						var topLevelParent = TopLevelParent(node);

						if ((topLevelParent != null) && (topLevelParent.PrevNode != null))
						{
							taskID = UniqueID(topLevelParent.PrevNode);
							return true;
						}
					}
					break;
			}

			// all else
			return false;
		}

        public bool CanSaveToImage()
        {
            return !IsEmpty();
        }
        		
        // Internal ------------------------------------------------------------

        override protected int ScaleByDPIFactor(int value)
        {
            return DPIScaling.Scale(value);
        }

        private bool RefreshItemFont(TreeNode node, bool andChildren)
        {
            var taskItem = TaskItem(node);

            if (taskItem == null)
                return false;

            Font curFont = node.NodeFont, newFont = null;

            if (taskItem.IsTask)
            {
                if (taskItem.ParentID == 0)
                {
                    if (taskItem.IsDone(false))
                        newFont = m_BoldDoneLabelFont;
                    else
                        newFont = m_BoldLabelFont;
                }
                else if (taskItem.IsDone(false))
                {
                    newFont = m_DoneLabelFont;
                }
            }

            bool fontChange = (newFont != curFont);

            if (fontChange)
                node.NodeFont = newFont;
            
            // children
            if (andChildren)
            {
                foreach (TreeNode childNode in node.Nodes)
                    fontChange |= RefreshItemFont(childNode, true);
            }

            return fontChange;
        }

		protected TreeNode LastNode
		{
			get { return TreeCtrl.GetLastItem(RootNode); }
		}

		private MindMapTaskItem TaskItem(TreeNode node)
		{
			if (node == null)
				return null;

			return (ItemData(node) as MindMapTaskItem);
		}

		private MindMapTaskItem RealTaskItem(TreeNode node)
		{
			return GetRealTaskItem(TaskItem(node));
		}

		private MindMapTaskItem TaskItem(Object itemData)
		{
			if (itemData == null)
				return null;

			return (itemData as MindMapTaskItem);
		}

		private TreeNode TopLevelParent(TreeNode node)
		{
			while ((node != null) && !IsRoot(node))
			{
				var parentNode = node.Parent;

				if (IsRoot(parentNode))
					return node;

				node = parentNode;
			}

			// else node was already null or root
			return null;
		}

		private void UpdateTaskAttributes(TaskList tasks)
		{
			var rootItem = TaskItem(RootNode);

			// A root item must already exist
			if (rootItem == null)
				return;

			var changedTaskIds = new HashSet<UInt32>();
			Task task = tasks.GetFirstTask();

			while (task.IsValid() && ProcessTaskUpdate(task, changedTaskIds))
				task = task.GetNextTask();

			if (tasks.IsAttributeAvailable(Task.Attribute.Title))
			{
				foreach (var id in changedTaskIds)
					RefreshNodeLabel(id, false);
			}

			if (tasks.IsAttributeAvailable(Task.Attribute.DoneDate))
			{
				foreach (var id in changedTaskIds)
				{
					var node = FindNode(id);
					RefreshItemFont(node, false);
				}
			}

			RecalculatePositions();
		}

		private bool ProcessTaskUpdate(Task task, HashSet<UInt32> taskIds)
		{
			if (!task.IsValid())
				return false;

			MindMapTaskItem item;
            UInt32 taskId = task.GetID();
			bool newTask = !m_Items.TryGetValue(taskId, out item);

			if (newTask)
			{
				var parentId = task.GetParentTask().GetID();
				var parentNode = FindNode(parentId);

				if ((parentId == 0) && (parentNode == null))
				{
					// If our parent's ID is zero and couldn't be found then it 
					// means the existing root item is a sibling of ours.
					// So we need to create a new non-task root, move the existing
					// root to the new root and use the new root as our parent
					var rootItem = TaskItem(RootNode);

					if (rootItem.IsTask)
					{
						var prevRootNode = RootNode;
						base.Clear();

						parentNode = AddRootNode(new MindMapTaskItem(m_Trans.Translate("Root")));
						parentNode.Nodes.Add(prevRootNode);
					}
					else
					{
						// ????
						parentNode = RootNode;
					}
				}

				AddTaskToTree(task, parentNode, true);
			}
			else if (item.ProcessTaskUpdate(task))
			{
				// Process children
				Task subtask = task.GetFirstSubtask();

				while (subtask.IsValid() && ProcessTaskUpdate(subtask, taskIds))
					subtask = subtask.GetNextTask();
			}
			else
			{
				return false;
			}

			taskIds.Add(task.GetID());

			return true;
		}

		private void RebuildTreeView(TaskList tasks)
		{
			// Snapshot the expanded tasks so we can restore them afterwards
			var expandedIDs = GetExpandedItems(RootNode);

			// And the selection
			var selID = UniqueID(SelectedNode);
			
			Clear();

			if (tasks.GetTaskCount() == 0)
				return;

			BeginUpdate();

			var task = tasks.GetFirstTask();
			bool taskIsRoot = !task.GetNextTask().IsValid(); // no siblings

			TreeNode rootNode = null;

			if (taskIsRoot)
			{
                var taskItem = new MindMapTaskItem(task);

                m_Items.Add(taskItem.ID, taskItem);
				rootNode = AddRootNode(taskItem, taskItem.ID);

                RefreshItemFont(rootNode, false);

				// First Child
				AddTaskToTree(task.GetFirstSubtask(), rootNode);
			}
			else
			{
				// There is more than one 'root' task so create a real root parent
				var projName = GetProjectName(tasks);
				rootNode = AddRootNode(new MindMapTaskItem(projName));

				AddTaskToTree(task, rootNode);
			}

			// Restore expanded state
			if (!SetExpandedItems(expandedIDs))
				rootNode.Expand();

			EndUpdate();
			SetSelectedNode(selID);
		}

		private String GetProjectName(TaskList tasks)
		{
			String rootName = tasks.GetProjectName();

			if (!String.IsNullOrWhiteSpace(rootName))
				return rootName;

			// else
			return m_Trans.Translate("Root");
		}

		private List<UInt32> GetExpandedItems(TreeNode node)
		{
			List<UInt32> expandedIDs = null;

			if ((node != null) && node.IsExpanded)
			{
				expandedIDs = new List<UInt32>();
    
                if (!IsRoot(node))
    				expandedIDs.Add(UniqueID(node));

				foreach (TreeNode child in node.Nodes)
				{
					var childIDs = GetExpandedItems(child);

					if (childIDs != null)
						expandedIDs.AddRange(childIDs);
				}
			}

			return expandedIDs;
		}

		private bool SetExpandedItems(List<UInt32> expandedNodes)
		{
            if (expandedNodes == null)
                return false;

            if (expandedNodes.Count == 0)
                return false;

            bool someSucceeded = false;

			foreach (var id in expandedNodes)
			{
				var node = FindNode(id);

                if (node != null)
                {
                    node.Expand();
                    someSucceeded = true;
                }
			}

            return someSucceeded;
		}

		protected override bool IsAcceptableDropTarget(Object draggedItemData, Object dropTargetItemData, DropPos dropPos, bool copy)
		{
			if (dropPos == MindMapControl.DropPos.On)
				return !TaskItem(dropTargetItemData).IsLocked;

			// else
			return true;
		}

		protected override bool IsAcceptableDragSource(Object itemData)
		{
			return !TaskItem(itemData).IsLocked;
		}

		protected override bool DoDrop(MindMapDragEventArgs e)
		{
			TreeNode prevParentNode = e.dragged.node.Parent;

			if (!base.DoDrop(e) || e.copyItem)
				return false;

			if (e.targetParent.node != prevParentNode)
			{
                // Fixup parent states
                // Note: our tree nodes haven't been moved yet but 
                // the application will have updated it's data structures
                // so we have to account for this in the node count passed
                FixupParentalStatus(e.targetParent.node, 1);
                FixupParentalStatus(prevParentNode, -1);

                FixupParentID(e.dragged.node, e.targetParent.node);
			}

			return true;
		}

		private void FixupParentalStatus(TreeNode parentNode, int nodeCountOffset)
        {
            var parentItem = TaskItem(parentNode);

            if (parentItem != null)
                parentItem.FixupParentalStatus((parentNode.Nodes.Count + nodeCountOffset), m_TaskIcons);
        }

		private void FixupParentID(TreeNode node, TreeNode parent)
        {
            var taskItem = TaskItem(node);

            if (taskItem != null)
            {
                taskItem.FixupParentID(TaskItem(parent));
                RefreshItemFont(node, false);
            }
        }

		protected override Font GetNodeFont(TreeNode node)
		{
            var taskItem = TaskItem(node);

            if ((taskItem != null) && taskItem.IsReference)
            {
				MindMapTaskItem refItem;

				if (m_Items.TryGetValue(taskItem.ReferenceId, out refItem))
				{
					if (refItem.ParentID == 0)
						return m_BoldLabelFont;
				}
            }

			// all else
			return base.GetNodeFont(node);
		}

		private MindMapTaskItem GetRealTaskItem(MindMapTaskItem taskItem)
		{
			if ((taskItem != null) && taskItem.IsReference)
			{
				MindMapTaskItem refItem;

				if (m_Items.TryGetValue(taskItem.ReferenceId, out refItem))
					return refItem;
			}

			// all else
			return taskItem;
		}

		protected override Color GetNodeBackgroundColor(Object itemData)
		{
			if (m_TaskColorIsBkgnd)
			{
				var taskItem = (itemData as MindMapTaskItem);
				var realItem = GetRealTaskItem(taskItem);

				if (!taskItem.TextColor.IsEmpty && !realItem.IsDone(true))
					return taskItem.TextColor;
			}

			// all else
			return base.GetNodeBackgroundColor(itemData);
		}

		protected override void DrawNodeLabel(Graphics graphics, String label, Rectangle rect,
											  NodeDrawState nodeState, NodeDrawPos nodePos,
                                              Font nodeFont, Object itemData)
		{
            var taskItem = (itemData as MindMapTaskItem);
			var realItem = GetRealTaskItem(taskItem);

			bool isSelected = (nodeState != NodeDrawState.None);
			Rectangle iconRect = Rectangle.Empty;

            if (taskItem.IsTask) // not root
            {
                // Checkbox
                Rectangle checkRect = CalcCheckboxRect(rect);

                if (m_ShowCompletionCheckboxes)
                    CheckBoxRenderer.DrawCheckBox(graphics, checkRect.Location, GetItemCheckboxState(realItem));

			    // Task icon
                if (TaskHasIcon(realItem))
                {
                    iconRect = CalcIconRect(rect);

                    if (m_TaskIcons.Get(realItem.ID))
                        m_TaskIcons.Draw(graphics, iconRect.X, iconRect.Y);

                    rect.Width = (rect.Right - iconRect.Right - 2);
                    rect.X = iconRect.Right + 2;
                }
                else if (m_ShowCompletionCheckboxes)
                {
                    rect.Width = (rect.Right - checkRect.Right - 2);
                    rect.X = checkRect.Right + 2;
                }
            }

			// Text Colour
			Color textColor = SystemColors.WindowText;

			if (!taskItem.TextColor.IsEmpty)
			{
				if (m_TaskColorIsBkgnd && !isSelected && !realItem.IsDone(true))
				{
					textColor = DrawingColor.GetBestTextColor(taskItem.TextColor);
				}
				else if (isSelected)
				{
					textColor = DrawingColor.SetLuminance(taskItem.TextColor, 0.3f);
				}
                else
                {
                    textColor = taskItem.TextColor;
                }
            }

			switch (nodeState)
			{
				case NodeDrawState.Selected:
                    UIExtension.SelectionRect.Draw(this.Handle, 
													graphics, 
													rect.X, 
													rect.Y, 
													rect.Width, 
													rect.Height, 
													false); // opaque
					break;

				case NodeDrawState.DropTarget:
                    UIExtension.SelectionRect.Draw(this.Handle, 
													graphics, 
													rect.X, 
													rect.Y, 
													rect.Width, 
													rect.Height,
													UIExtension.SelectionRect.Style.DropHighlighted,
													false); // opaque
					break;
			}

			if (DebugMode())
               graphics.DrawRectangle(new Pen(Color.Green), rect);

			// Text
			var format = DefaultLabelFormat(nodePos, isSelected);

            graphics.DrawString(label, nodeFont, new SolidBrush(textColor), rect, format);

			// Draw Windows shortcut icon if task is a reference
			if (taskItem.IsReference)
			{
				if (iconRect == Rectangle.Empty)
					iconRect = rect;
				else
					iconRect.Y = (rect.Bottom - iconRect.Height); // don't want shortcut icon centred vertically

				UIExtension.ShortcutOverlay.Draw(graphics, iconRect.X, iconRect.Y, iconRect.Width, iconRect.Height);
			}
		}

        CheckBoxState GetItemCheckboxState(MindMapTaskItem taskItem)
        {
            if (taskItem.IsDone(false))
                return CheckBoxState.CheckedNormal;

            if (taskItem.HasSomeSubtasksDone)
                return CheckBoxState.MixedNormal;

            // else
            return CheckBoxState.UncheckedNormal;
        }

		private bool NodeIsTask(TreeNode node)
        {
            return TaskItem(node).IsTask;
        }

		private bool TaskHasIcon(MindMapTaskItem taskItem)
		{
			if ((m_TaskIcons == null) || (taskItem == null) || !taskItem.IsTask)
				return false;

			return (taskItem.HasIcon || (m_ShowParentAsFolder && taskItem.IsParent));
		}

		protected override void DrawNodeConnection(Graphics graphics, Point ptFrom, Point ptTo)
		{
			int midX = ((ptFrom.X + ptTo.X) / 2);

			graphics.DrawBezier(new Pen(base.ConnectionColor), 
								ptFrom,
								new Point(midX, ptFrom.Y),
								new Point(midX, ptTo.Y),
								ptTo);
		}


		protected override void PostDraw(Graphics graphics, TreeNodeCollection nodes)
		{
			foreach (TreeNode node in nodes)
				DrawTaskDependencies(graphics, node);
		}

		protected void DrawTaskDependencies(Graphics graphics, TreeNode node)
		{
			var task = TaskItem(node);

			if ((task != null) && task.HasLocalDependencies)
			{
				foreach (var depend in task.LocalDependencies)
				{
					var dependNode = FindNode(depend);

					if ((dependNode != null) && dependNode.Parent.IsExpanded)
						DrawTaskDependency(graphics, node, dependNode);
				}
			}

			// children
			if (node.IsExpanded)
			{
			foreach (TreeNode childNode in node.Nodes)
				DrawTaskDependencies(graphics, childNode);
			}
		}

		private bool IsDependencyLeafNode(TreeNode node)
		{
			return (!node.IsExpanded || (node.FirstNode == null));
		}

		internal int DependencyOffset
		{
			get { return ScaleByDPIFactor(25); }
		}

		protected override int GraphPadding { get { return DependencyOffset; } }

		private Point[] CalcHorzDependencyArrow(Point point, int itemHeight, bool flipped)
		{
			Point[] arrow = new Point[] { point, point, point };

			int ARROW = (itemHeight / 4);

			if (flipped)
			{
				arrow[0].Offset(-ARROW, -ARROW);
				arrow[2].Offset(-ARROW, ARROW);
			}
			else
			{
				arrow[0].Offset(ARROW, -ARROW);
				arrow[2].Offset(ARROW, ARROW);
			}

			return arrow;
		}

		private void DrawHorzDependencyArrow(Graphics graphics, Point point, int itemHeight, bool flipped)
		{
			graphics.DrawLines(Pens.Black, CalcHorzDependencyArrow(point, itemHeight, flipped));

			if (flipped)
				point.X++;
			else
				point.X--;

			graphics.DrawLines(Pens.Black, CalcHorzDependencyArrow(point, itemHeight, flipped));
		}

		protected void DrawTaskDependency(Graphics graphics, TreeNode nodeFrom, TreeNode nodeTo)
		{
			if ((nodeFrom == null) || (nodeTo == null))
				return;

			MindMapItem itemFrom = Item(nodeFrom);
			MindMapItem itemTo = Item(nodeTo);

			Rectangle rectFrom = GetItemDrawRect(itemFrom.ItemBounds);
			Rectangle rectTo = GetItemDrawRect(itemTo.ItemBounds);

			bool fromIsAboveTo = (rectFrom.Bottom <= rectTo.Top);
			bool toIsAboveFrom = (rectTo.Bottom <= rectFrom.Top);

			Point ptFrom, ptTo, ptControlFrom, ptControlTo;

			// Various patterns requiring different solutions:
			bool processed = false;

			//
			// 1. Leaf tasks
			if (IsDependencyLeafNode(nodeFrom) && IsDependencyLeafNode(nodeTo))
			{
				// 1.1 Same side of the root
				if (itemFrom.IsFlipped == itemTo.IsFlipped)
				{
					int vOffset = (fromIsAboveTo ? 2 : toIsAboveFrom ? -2 : 0);
					int controlX = 0;

					if (itemFrom.IsFlipped)
					{
						// Left side
						ptFrom = RectUtil.MiddleLeft(rectFrom);
						ptTo = RectUtil.MiddleLeft(rectTo);

						controlX = (Math.Min(ptFrom.X, ptTo.X) - DependencyOffset);
					}
					else // right side
					{
						ptFrom = RectUtil.MiddleRight(rectFrom);
						ptTo = RectUtil.MiddleRight(rectTo);

						controlX = (Math.Max(ptFrom.X, ptTo.X) + DependencyOffset);
					}

					ptFrom.Y += vOffset;
					ptTo.Y -= vOffset;

					ptControlFrom = new Point(controlX, ptFrom.Y);
					ptControlTo = new Point(controlX, ptTo.Y);

					ptControlFrom.Y += vOffset;
					ptControlTo.Y -= vOffset;

					graphics.DrawBezier(Pens.Red/*m_DependencyPen*/, ptFrom, ptControlFrom, ptControlTo, ptTo);

					// Draw arrow at from end (size to match Gantt Chart)
					DrawHorzDependencyArrow(graphics, ptFrom, rectFrom.Height, itemFrom.IsFlipped);

					processed = true;
				}
				else // 1.2 Opposing sides of the root
				{
				}
			}
			//
			// 2. Leaf to Non-leaf (parent) tasks
			//    2.1 Same side of the root
			//    2.2 Opposing sides of the root
			//
			// 3. Non-leaf (parent) to Non-leaf (parent) tasks
			//    3.1 Same side of the root
			//    3.2 Opposing sides of the root
			//
			// 4. Parent to own child (always on same side of root)
			//    4.1 Child is a leaf task
			//    4.2 Child is NOT a leaf task
			//
			// 5. All other relationships
			//    5.1 One of the tasks is a leaf task
			//    5.2 NEITHER task is a leaf task


			// 			Point ptFrom = new Point((rectFrom.Left + rectFrom.Right) / 2, ((rectFrom.Top + rectFrom.Bottom) / 2));
			// 			Point ptTo = new Point((rectTo.Right + rectTo.Left) / 2, ((rectTo.Top + rectTo.Bottom) / 2));
			// 
			// 			// Don't draw connections falling wholly outside the client rectangle
			// 			Rectangle clipRect = Rectangle.Round(graphics.ClipBounds);
			// 
			// 			if (!RectFromPoints(ptFrom, ptTo).IntersectsWith(clipRect))
			// 				return;
			// 
			// 			graphics.DrawLine(Pens.Black, ptFrom, ptTo);

			// All the rest
			if (!processed)
			{
				if (itemFrom.IsFlipped)
				{
					// Left to right
					ptFrom = RectUtil.MiddleRight(rectFrom);
					ptTo = RectUtil.MiddleLeft(rectTo);
				}
				else // right to left
				{
					ptFrom = RectUtil.MiddleLeft(rectFrom);
					ptTo = RectUtil.MiddleRight(rectTo);
				}

				int controlY = (Math.Min(ptFrom.Y, ptTo.Y) - DependencyOffset);

				ptControlFrom = new Point((ptFrom.X + ptTo.X) / 2, controlY);
				ptControlTo = ptControlFrom;// new Point(ptTo.X, controlY);

				graphics.DrawBezier(Pens.Red/*m_DependencyPen*/, ptFrom, ptControlFrom, ptControlTo, ptTo);
			}

		}

		private Rectangle CalcCheckboxRect(Rectangle labelRect)
        {
            if (!m_ShowCompletionCheckboxes)
                return Rectangle.Empty;

            int left = labelRect.X;
            int top = (CentrePoint(labelRect).Y - (m_CheckboxSize.Height / 2));

            return new Rectangle(left, top, m_CheckboxSize.Width, m_CheckboxSize.Height);
        }

        private Rectangle CalcIconRect(Rectangle labelRect)
		{
            int left = (labelRect.X + 2);
            
            if (m_ShowCompletionCheckboxes)
                left += m_CheckboxSize.Width;

            int imageSize = ScaleByDPIFactor(16);
            int top = (CentrePoint(labelRect).Y - (imageSize / 2));

            return new Rectangle(left, top, imageSize, imageSize);
		}

		private new void Clear()
		{
			m_Items.Clear();

			base.Clear();
		}

		private bool AddTaskToTree(Task task, TreeNode parent, bool select = false)
		{
			if (!task.IsValid())
				return true; // not an error

			var taskID = task.GetID();
			var taskItem = new MindMapTaskItem(task);

			var node = AddNode(taskItem, parent, taskID);

			if (node == null)
				return false;

            RefreshItemFont(node, false);

			// First Child
			if (!AddTaskToTree(task.GetFirstSubtask(), node))
				return false;

			// First Sibling
			if (!AddTaskToTree(task.GetNextTask(), parent))
				return false;

			m_Items.Add(taskID, taskItem);

			if (select)
				SelectedNode = node;

			return true;
		}

		protected override int GetExtraWidth(TreeNode node)
		{
            int extraWidth = base.GetExtraWidth(node);
            var taskItem = RealTaskItem(node);

            if (m_ShowCompletionCheckboxes && taskItem.IsTask)
                extraWidth += m_CheckboxSize.Width;

			if (TaskHasIcon(taskItem))
				extraWidth += (ScaleByDPIFactor(16) + 2);

			return extraWidth;
		}

		protected override int GetMinItemHeight()
		{
            return (ScaleByDPIFactor(16) + 2);
		}

		protected override void OnMouseDoubleClick(MouseEventArgs e)
		{
			if (base.HandleMouseDoubleClick(e) || (EditTaskLabel == null))
				return;

			if (HitTestPositions(e.Location) != null)
				EditTaskLabel(this, UniqueID(SelectedNode));
		}

		protected override void OnMouseClick(MouseEventArgs e)
		{
			base.OnMouseClick(e);

			if (e.Button != MouseButtons.Left)
				return;

			if (m_IgnoreMouseClick)
			{
				m_IgnoreMouseClick = false;
				return;
			}

			TreeNode node = HitTestPositions(e.Location);

			if ((node == null) || (node != SelectedNode) || !NodeIsTask(node))
				return;

			if (HitTestExpansionButton(node, e.Location))
				return;

            var taskItem = RealTaskItem(node);

            if (!ReadOnly && !taskItem.IsLocked)
            {
                if (HitTestCheckbox(node, e.Location))
                {
					if (EditTaskDone != null)
						EditTaskDone(this, taskItem.ID, !taskItem.IsDone(false));
				}
                else if (HitTestIcon(node, e.Location))
                {
                    // Performing icon editing from a 'MouseUp' or 'MouseClick' event 
                    // causes the edit icon dialog to fail to correctly get focus but
                    // counter-intuitively it works from 'MouseDown'
                    //if (EditTaskIcon != null)
                    //    EditTaskIcon(this, UniqueID(SelectedNode));
                }
                else if (SelectNodeWasPreviouslySelected)
			    {
			        if (EditTaskLabel != null)
				        m_EditTimer.Start();
                }
            }
		}

		private bool HitTestCheckbox(TreeNode node, Point point)
        {
            if (!m_ShowCompletionCheckboxes)
                return false;

            return CalcCheckboxRect(GetItemLabelRect(node)).Contains(point);
        }

		private bool HitTestIcon(TreeNode node, Point point)
        {
			var taskItem = RealTaskItem(node);

			if (taskItem.IsLocked || !TaskHasIcon(taskItem))
				return false;

			// else
			return CalcIconRect(GetItemLabelRect(node)).Contains(point);
        }

		protected override void OnMouseDown(MouseEventArgs e)
		{
			m_EditTimer.Stop();

			switch (e.Button)
			{
				case MouseButtons.Left:
					{
						// Cache the previous selected item
						m_PreviouslySelectedNode = SelectedNode;
                        m_IgnoreMouseClick = false;

						TreeNode hit = HitTestPositions(e.Location);

                        if (hit != null)
                        {
                            m_IgnoreMouseClick = HitTestExpansionButton(hit, e.Location);

                            // Performing icon editing from a 'MouseUp' or 'MouseClick' event 
                            // causes the edit icon dialog to fail to correctly get focus but
                            // counter-intuitively it works from 'MouseDown'
                            if (!m_IgnoreMouseClick && !ReadOnly && HitTestIcon(hit, e.Location))
                            {
								// Ensure the node is selected
								SelectedNode = hit;

                                if (EditTaskIcon != null)
                                    EditTaskIcon(this, UniqueID(SelectedNode));
                            }
                        }
					}
					break;

				case MouseButtons.Right:
					{
						TreeNode hit = HitTestPositions(e.Location);

						if (hit != null)
							SelectedNode = hit;
					}
					break;
			}

			base.OnMouseDown(e);
		}

		private void OnEditLabelTimer(object sender, EventArgs e)
		{
			m_EditTimer.Stop();

			if (EditTaskLabel != null)
				EditTaskLabel(this, UniqueID(SelectedNode));
		}

		protected override void OnMouseMove(MouseEventArgs e)
		{
 			base.OnMouseMove(e);

			var node = HitTestPositions(e.Location);

			if (!ReadOnly && (node != null) && !HitTestExpansionButton(node, e.Location))
			{
				var taskItem = RealTaskItem(node);

				if (taskItem != null)
                {
                    Cursor cursor = null;

                    if (taskItem.IsLocked)
                    {
                        cursor = UIExtension.AppCursor(UIExtension.AppCursorType.LockedTask);
                    }
                    else if (TaskHasIcon(taskItem) && HitTestIcon(node, e.Location))
                    {
                        cursor = UIExtension.HandCursor();
                    }
                    
                    if (cursor != null)
                    {
                        Cursor = cursor;
                        return;
                    }
				}
			}

			// all else
			Cursor = Cursors.Arrow;
		}
	}
}

