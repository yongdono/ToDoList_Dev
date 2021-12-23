﻿using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Drawing.Imaging;
using System.Drawing.Drawing2D;
using System.Data;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Runtime.InteropServices;
using System.Diagnostics;
using System.Windows.Forms.VisualStyles;

namespace PertNetworkUIExtension
{
	public delegate void SelectionChangeEventHandler(object sender, object itemData);
	public delegate bool DragDropChangeEventHandler(object sender, PertNetworkDragEventArgs e);

	[System.ComponentModel.DesignerCategory("")]

	public class PertNetworkDragEventItem
	{
		public PertNetworkDragEventItem( /* TODO*/ )
		{
		}
	}

	public class PertNetworkDragEventArgs : EventArgs
	{
		public PertNetworkDragEventArgs( /* TODO*/ )
		{
		}

	}

	public partial class PertNetworkControl : ListView
    {
		// Win32 Imports -----------------------------------------------------------------

		[DllImport("User32.dll")]
		static extern int SendMessage(IntPtr hWnd, int msg, int wParam = 0, int lParam = 0);

		// --------------------------

		[DllImport("User32.dll")]
		static extern uint GetDoubleClickTime();
		
		// --------------------------

		[DllImport("User32.dll")]
		static extern int GetSystemMetrics(int index);
		
		const int SM_CXDOUBLECLK = 36;
		const int SM_CYDOUBLECLK = 37;
		const int SM_CXDRAG = 68;
		const int SM_CYDRAG = 69;

		// Constants ---------------------------------------------------------------------

        virtual protected int ScaleByDPIFactor(int value)
        {
            return value;
        }

		virtual protected int ItemHorzSeparation { get { return ScaleByDPIFactor(40); } }
		virtual protected int ItemVertSeparation { get { return ScaleByDPIFactor(4); } }
        virtual protected int InsertionMarkerHeight { get { return ScaleByDPIFactor(6); } }
		virtual protected int LabelPadding { get { return ScaleByDPIFactor(2); } }
        virtual protected int GraphPadding { get { return ScaleByDPIFactor(6); } }

		protected float ZoomFactor { get { return m_ZoomFactor; } }
		protected bool IsZoomed { get { return (m_ZoomFactor < 1.0f); } }

		protected enum NodeDrawState
		{
			None,
			Selected,
			DropTarget,
		}

		protected enum DropPos
        {
            None,
            On,
            Above,
            Below,
        }

		// Data --------------------------------------------------------------------------

        private Point m_DrawOffset;
        private DropPos m_DropPos;
		private Timer m_DragTimer;
		private Color m_ConnectionColor;
		private int m_LastDragTick = 0;
        private int m_ThemedGlyphSize = 0;
		private float m_ZoomFactor = 1f;

		private bool m_FirstPaint = true;
        private bool m_HoldRedraw = false;
        private bool m_IsSavingToImage = false;

#if DEBUG
		private int m_RecalcDuration;
#endif

		// Public ------------------------------------------------------------------------

		public event SelectionChangeEventHandler SelectionChange;
		public event DragDropChangeEventHandler DragDropChange;

        public PertNetworkControl()
        {
            m_DrawOffset = new Point(0, 0);
            m_DropPos = DropPos.None;
			m_ConnectionColor = Color.Magenta;

			InitializeComponent();
		}

		public void SetFont(String fontName, int fontSize)
        {
            if ((this.Font.Name == fontName) && (this.Font.Size == fontSize))
                return;

            this.Font = new Font(fontName, fontSize, FontStyle.Regular);
        }

		public Color ConnectionColor
		{
			get { return m_ConnectionColor; }
			set 
			{
				if (value != SystemColors.Window)
				{
					m_ConnectionColor = value;
					Invalidate();
				}
			}
		}

        public bool ReadOnly
        {
            set;
            get;
        }

        public bool SetSelectedNode(uint uniqueID)
        {
//             var node = FindNode(uniqueID);
// 
// 			if (node == null)
// 				return false;
// 
//             SelectedNode = node;
//			EnsureItemVisible(Item(node));

            return true;
        }

        public bool RefreshNodeLabel(uint uniqueID, bool invalidate)
        {
//             TreeNode node = FindNode(uniqueID);
// 
// 			if (node == null)
// 				return false;
// 
//             node.Text = ItemData(node).ToString();

			if (invalidate)
				Invalidate();

            return true;
        }

		public Rectangle GetSelectedItemLabelRect()
		{
			return new Rectangle(0, 0, 10, 10);// GetItemLabelRect(SelectedNode);
		}

        public Bitmap SaveToImage()
        {
			// Cache state
			/*
						Point scrollPos = new Point(HorizontalScroll.Value, VerticalScroll.Value);
						Point drawOffset = new Point(m_DrawOffset.X, m_DrawOffset.Y);

						// And reset
						m_IsSavingToImage = true;
						m_DrawOffset = new Point(0, 0);

						HorizontalScroll.Value = 0;
						VerticalScroll.Value = 0;

						if (!scrollPos.IsEmpty)
							PerformLayout();

						int border = BorderWidth;

						// Total size of the graph
						Rectangle graphRect = Rectangle.Inflate(RootItem.TotalBounds, GraphPadding, GraphPadding);

						// The portion of the client rect we are interested in 
						// (excluding the top and left borders)
						Rectangle srcRect = Rectangle.FromLTRB(border, 
															   border, 
															   ClientRectangle.Width - border, 
															   ClientRectangle.Height - border);

						// The output image
						Bitmap finalImage = new Bitmap(graphRect.Width, graphRect.Height, PixelFormat.Format32bppRgb);

						// The temporary image allowing us to clip out the top and left borders
						Bitmap srcImage = new Bitmap(ClientRectangle.Width, ClientRectangle.Height, PixelFormat.Format32bppRgb);

						// The current position in the output image for rendering the temporary image
						Rectangle drawRect = srcRect;
						drawRect.Offset(-border, -border);

						// Note: If the last horz or vert page is empty because of an 
						// exact division then it will get handled within the loop
						int numHorzPages = ((graphRect.Width / drawRect.Width) + 1);
						int numVertPages = ((graphRect.Height / drawRect.Height) + 1);

						using (Graphics graphics = Graphics.FromImage(finalImage))
						{
							for (int vertPage = 0; vertPage < numVertPages; vertPage++)
							{
								for (int horzPage = 0; horzPage < numHorzPages; horzPage++)
								{
									// TODO
								}

								// TODO
							}
						}

						// Restore state
						m_IsSavingToImage = false;
						m_DrawOffset = drawOffset;

						HorizontalScroll.Value = scrollPos.X;
						VerticalScroll.Value = scrollPos.Y;

						if (!scrollPos.IsEmpty)
							PerformLayout();

						return finalImage;
			*/
			return null;
        }

		// Message Handlers -----------------------------------------------------------

/*
		protected void BeginUpdate()
		{
			HoldRedraw = true;

			EnableSelectionNotifications(false);
		}

		protected void EndUpdate()
		{
			HoldRedraw = false;

			EnableSelectionNotifications(true);
		}
*/

		protected virtual void PostDraw(Graphics graphics, TreeNodeCollection nodes)
		{
			// For derived classes
		}

		protected override void OnPaintBackground(PaintEventArgs e)
		{
			//base.OnPaintBackground(e);
		}

        protected override void OnMouseDoubleClick(MouseEventArgs e)
        {
			// TODO
        }

        protected override void OnMouseDown(MouseEventArgs e)
        {
			base.OnMouseDown(e);

			// TODO
		}

		protected override void OnMouseWheel(MouseEventArgs e)
		{
			if ((ModifierKeys & Keys.Control) == Keys.Control)
			{
/*
				float newFactor = m_ZoomFactor;

				if (e.Delta > 0)
				{
					newFactor += 0.1f;
					newFactor = Math.Min(newFactor, 1.0f);
				}
				else
				{
					newFactor -= 0.1f;
					newFactor = Math.Max(newFactor, 0.4f);
				}

				if (newFactor != m_ZoomFactor)
				{
					Cursor = Cursors.WaitCursor;

					// Convert mouse pos to relative coordinates
					float relX = ((e.Location.X + HorizontalScroll.Value) / (float)HorizontalScroll.Maximum);
					float relY = ((e.Location.Y + VerticalScroll.Value) / (float)VerticalScroll.Maximum);

					// Prevent all selection and expansion changes for the duration
 					BeginUpdate();

					// The process of changing the fonts and recalculating the 
					// item height can cause the tree-view to spontaneously 
					// collapse tree nodes so we save the expansion state
					// and restore it afterwards
					var expandedNodes = GetExpandedNodes(RootNode);

					m_ZoomFactor = newFactor;
					UpdateTreeFont(false);

					// 'Cleanup'
					SetExpandedNodes(expandedNodes);
 					EndUpdate();

					// Scroll the view to keep the mouse located in the 
					// same relative position as before
					if (HorizontalScroll.Visible)
					{
						int newX = (int)(relX * HorizontalScroll.Maximum) - e.Location.X;

						HorizontalScroll.Value = Validate(newX, HorizontalScroll);
					}

					if (VerticalScroll.Visible)
					{
						int newY = (int)(relY * VerticalScroll.Maximum) - e.Location.Y;

						VerticalScroll.Value = Validate(newY, VerticalScroll);
					}

					PerformLayout();
				}
*/
			}
			else
			{
				// Default scroll
				base.OnMouseWheel(e);
			}
		}

		static int Validate(int pos, ScrollProperties scroll)
		{
			return Math.Max(scroll.Minimum, Math.Min(pos, scroll.Maximum));
		}

		protected override void OnMouseMove(MouseEventArgs e)
		{
			base.OnMouseMove(e);

			if ((e.Button == MouseButtons.Left) && m_DragTimer.Enabled)
			{
                Debug.Assert(!ReadOnly);

				if (CheckStartDragging(e.Location))
					m_DragTimer.Stop();
			}
		}

		protected override void OnMouseUp(MouseEventArgs e)
		{
			m_DragTimer.Stop();

			base.OnMouseUp(e);

			if (e.Button == MouseButtons.Left)
			{
				// TODO
			}
		}

		protected void OnDragTimer(object sender, EventArgs e)
		{
            Debug.Assert(!ReadOnly);

			m_DragTimer.Stop();

			bool mouseDown = ((MouseButtons & MouseButtons.Left) == MouseButtons.Left);

			if (mouseDown)
				CheckStartDragging(MousePosition);
		}
	
		protected override void OnDragOver(DragEventArgs e)
		{
            Debug.Assert(!ReadOnly);

			// TODO
		}

		protected override void OnDragDrop(DragEventArgs e)
		{
            Debug.Assert(!ReadOnly);

			// TODO
		}

		protected override void OnQueryContinueDrag(QueryContinueDragEventArgs e)
		{
            Debug.Assert(!ReadOnly);

            base.OnQueryContinueDrag(e);

			if (e.EscapePressed)
			{
				// TODO
			}
		}

		protected override void OnDragLeave(EventArgs e)
		{
            Debug.Assert(!ReadOnly);

            base.OnDragLeave(e);

			// TODO

			Invalidate();
		}
		
		protected override void OnSizeChanged(EventArgs e)
		{
			base.OnSizeChanged(e);
		}


		protected override void OnGotFocus(EventArgs e)
		{
			base.OnGotFocus(e);

			Invalidate(GetSelectedItemLabelRect());
		}

		protected override void OnLostFocus(EventArgs e)
		{
			base.OnLostFocus(e);

			Invalidate(GetSelectedItemLabelRect());
		}

		protected override void OnFontChanged(EventArgs e)
		{
			base.OnFontChanged(e);
			
			// Always reset the logical zoom else we've no way of 
			// accurately calculating the actual zoom
			m_ZoomFactor = 1f;

			//UpdateTreeFont(true);
		}

		// Hook for derived classes
		virtual protected bool RefreshNodeFont(TreeNode node, bool andChildren)
		{
			return false;
		}

		protected override void OnKeyDown(KeyEventArgs e)
		{
			if (HandleCursorKey(e.KeyCode))
				return;

// 			switch (e.KeyCode)
// 			{
// 			}

			// else
			base.OnKeyDown(e);
		}

        // Internals -----------------------------------------------------------

        private int BorderWidth
        {
            get
            {
                switch (BorderStyle)
                {
                    case BorderStyle.FixedSingle:
                        return 1;

                    case BorderStyle.Fixed3D:
                        return 2;
                }

                return 0;
            }
        }

		protected bool HoldRedraw
		{
			get { return m_HoldRedraw; }
			set 
			{
				if (m_HoldRedraw != value)
				{
					if (!value) // release redraw
						Invalidate();
				}

				m_HoldRedraw = value; 
			}
		}

		private bool CheckStartDragging(Point cursor)
		{
            Debug.Assert(!ReadOnly);

			// TODO

			return false;
		}
		
		virtual protected bool IsAcceptableDropTarget(Object draggedItemData, Object dropTargetItemData, DropPos dropPos, bool copy)
		{
            Debug.Assert(!ReadOnly);

            return true;
		}

		virtual protected bool IsAcceptableDragSource(Object itemData)
		{
            Debug.Assert(!ReadOnly);

            return (itemData != null);
		}

		private Rectangle GetDoubleClickRect(Point cursor)
		{
			var rect = new Rectangle(cursor.X, cursor.Y, 0, 0);
			rect.Inflate(GetSystemMetrics(SM_CXDOUBLECLK) / 2, GetSystemMetrics(SM_CYDOUBLECLK) / 2);

			return rect;
		}

		private Rectangle GetDragRect(Point cursor)
		{
            Debug.Assert(!ReadOnly);

            var rect = new Rectangle(cursor.X, cursor.Y, 0, 0);
			rect.Inflate(GetSystemMetrics(SM_CXDRAG) / 2, GetSystemMetrics(SM_CYDRAG) / 2);

			return rect;
		}

		private void DoDrop(/* TODO */)
		{
            Debug.Assert(!ReadOnly);

			// TODO
		}

		virtual protected bool DoDrop(PertNetworkDragEventArgs e)
		{
            Debug.Assert(!ReadOnly);

            if (DragDropChange != null)
				return DragDropChange(this, e);

			// else
			return true;
		}

		protected void EnsureItemVisible(PertNetworkItem item)
        {
            if (item == null)
                return;

            Rectangle itemRect = GetItemDrawRect(item.ItemBounds);

            if (ClientRectangle.Contains(itemRect))
                return;

/*
            if (HorizontalScroll.Visible)
            {
                int xOffset = 0;

                if (itemRect.Left < ClientRectangle.Left)
                {
                    xOffset = (itemRect.Left - ClientRectangle.Left);
                }
                else if (itemRect.Right > ClientRectangle.Right)
                {
                    xOffset = (itemRect.Right - ClientRectangle.Right);
                }

                if (xOffset != 0)
                {
                    int scrollX = (HorizontalScroll.Value + xOffset);
  
                    HorizontalScroll.Value = Validate(scrollX, HorizontalScroll);
                }
            }

            if (VerticalScroll.Visible)
            {
                int yOffset = 0;

                if (itemRect.Top < ClientRectangle.Top)
                {
                    yOffset = (itemRect.Top - ClientRectangle.Top);
                }
                else if (itemRect.Bottom > ClientRectangle.Bottom)
                {
                    yOffset = (itemRect.Bottom - ClientRectangle.Bottom);
                }

                if (yOffset != 0)
                {
                    int scrollY = (VerticalScroll.Value + yOffset);
  
                    VerticalScroll.Value = Validate(scrollY, VerticalScroll);
                }
            }
*/

            PerformLayout();
            Invalidate();
        }

		protected void EnableSelectionNotifications(bool enable)
		{
			// TODO
		}

		protected bool HandleCursorKey(Keys key)
		{
			// TODO

			return false;
		}

// 		protected virtual int GetExtraWidth(TreeNode node)
// 		{
// 			return (2 * LabelPadding);
// 		}

		protected virtual int GetMinItemHeight()
		{
			return 10;
		}

		protected bool IsEmpty()
		{
			return true;
		}

		protected PertNetworkItem SelectedItem
		{
			get
			{
				return null; // TODO
			}
		}

		protected Font ScaledFont(Font font)
		{
			if ((font != null) && (m_ZoomFactor < 1.0))
				return new Font(font.FontFamily, font.Size * m_ZoomFactor, font.Style);

			// else
			return font;
		}

		protected Rectangle RectFromPoints(Point pt1, Point pt2)
		{
			return Rectangle.FromLTRB(Math.Min(pt1.X, pt2.X),
										Math.Min(pt1.Y, pt2.Y),
										Math.Max(pt1.X, pt2.X),
										Math.Max(pt1.Y, pt2.Y));
		}

		protected Rectangle GetItemDrawRect(Rectangle itemRect)
		{
			Rectangle drawPos = itemRect;

// 			drawPos.Offset(m_DrawOffset);
//             drawPos.Offset(-HorizontalScroll.Value, -VerticalScroll.Value);
//             drawPos.Offset(GraphPadding, GraphPadding);
			
			return drawPos;
		}

		protected Rectangle GetItemHitRect(Rectangle itemRect)
		{
			Rectangle drawPos = GetItemDrawRect(itemRect);

			return Rectangle.Inflate(drawPos, 2, 0);
		}

		virtual protected Color GetNodeBackgroundColor(Object itemData)
		{
			return Color.Empty;
		}

        protected PertNetworkItem HitTestPositions(Point point)
        {
            // TODO

            return null;
        }

    }

}
