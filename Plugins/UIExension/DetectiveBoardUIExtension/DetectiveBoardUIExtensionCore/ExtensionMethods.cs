﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace DetectiveBoardUIExtension
{
	public static class ExtensionMethods
	{
		public static System.Drawing.Point GetPosition(this RadialTree.Point point)
		{
			return new System.Drawing.Point((int)point.X, (int)point.Y);
		}

		public static void SetPosition(this RadialTree.Point point, System.Drawing.Point dPoint)
		{
			point.X = dPoint.X;
			point.Y = dPoint.Y;
		}

		public static System.Drawing.Point GetPosition(this RadialTree.Point point, System.Drawing.Size offset)
		{
			return new System.Drawing.Point((int)point.X + offset.Width, (int)point.Y + offset.Height);
		}

		public static System.Drawing.Rectangle GetRectangle(this RadialTree.Point point, System.Drawing.Size size)
		{
			return point.GetRectangle(size, System.Drawing.Size.Empty);
		}

		public static System.Drawing.Rectangle GetRectangle(this RadialTree.Point point, System.Drawing.Size size, System.Drawing.Size offset)
		{
			var pos = GetPosition(point, offset);

			return new System.Drawing.Rectangle(pos.X - (size.Width / 2),
												pos.Y - (size.Height / 2),
												size.Width,
												size.Height);
		}

		public static System.Drawing.Point GetPosition<T>(this RadialTree.TreeNode<T> node)
		{
			return node.Point.GetPosition();
		}
		
		public static System.Drawing.Point GetPosition<T>(this RadialTree.TreeNode<T> node, System.Drawing.Size offset)
		{
			return node.Point.GetPosition(offset);
		}

		public static System.Drawing.Rectangle GetRectangle<T>(this RadialTree.TreeNode<T> node, System.Drawing.Size size)
		{
			return node.Point.GetRectangle(size);
		}

		public static System.Drawing.Rectangle GetRectangle<T>(this RadialTree.TreeNode<T> node, System.Drawing.Size size, System.Drawing.Size offset)
		{
			return node.Point.GetRectangle(size, offset);
		}

		public static System.Drawing.Rectangle Multiply(this System.Drawing.Rectangle rect, float mult)
		{
			return new System.Drawing.Rectangle(rect.Location.Multiply(mult), rect.Size.Multiply(mult));
		}

		public static System.Drawing.Size Multiply(this System.Drawing.Size size, float mult)
		{
			return new System.Drawing.Size((int)(size.Width * mult), (int)(size.Height * mult));
		}

		public static System.Drawing.Point Multiply(this System.Drawing.Point pt, float mult)
		{
			return new System.Drawing.Point((int)(pt.X * mult), (int)(pt.Y * mult));
		}

		public static System.Drawing.Point Divide(this System.Drawing.Point pt, float div)
		{
			return new System.Drawing.Point((int)(pt.X / div), (int)(pt.Y / div));
		}

		public static void MoveToHead<T>(this IList<T> list, T item)
		{
			list.Remove(item);
			list.Insert(0, item);
		}

		public static void Add<T>(this IList<T> list, IList<T> other)
		{
			foreach (T val in other)
				list.Add(val);
		}

		public static void Remove<T>(this IList<T> list, IList<T> other)
		{
			foreach (T val in other)
				list.Remove(val);
		}
	}
}
