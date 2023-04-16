﻿using System;
using System.Diagnostics;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Drawing;

namespace DetectiveBoardUIExtension
{
	public class Geometry2D
	{
		public static Rectangle GetCentredRect(Point pos, int sideLength)
		{
			return new Rectangle((pos.X - (sideLength / 2)), (pos.Y - (sideLength / 2)), sideLength, sideLength);
		}

		public enum AngleAxis
		{
			FromHorizontal,
			FromVertical
		}

		public static double DegreesToRadians(double degrees)
		{
			return ((degrees * Math.PI) / 180);
		}

		public static double RadiansToDegrees(double radians)
		{
			return ((radians * 180) / Math.PI);
		}

		public static float BestTextAngleInDegrees(Point ptFrom, Point ptTo, AngleAxis axis)
		{
			if (ptFrom.X <= ptTo.X)
				return DegreesBetween(ptTo, ptFrom, axis);

			// else
			return DegreesBetween(ptFrom, ptTo, axis);
		}

		public static float DegreesBetween(Point ptFrom, Point ptTo, AngleAxis axis)
		{
			var diff = Difference(ptTo, ptFrom);
			var radians = Math.Atan2(diff.Y, diff.X);
			var degrees = RadiansToDegrees(radians);

			if (axis == AngleAxis.FromVertical)
				degrees -= 90;

			while (degrees < 0)
				degrees += 360;

			return (float)degrees;
		}

		public static Rectangle RectFromPoints(Point pt1, Point pt2)
		{
			return Rectangle.FromLTRB(Math.Min(pt1.X, pt2.X),
									  Math.Min(pt1.Y, pt2.Y),
									  Math.Max(pt1.X, pt2.X),
									  Math.Max(pt1.Y, pt2.Y));
		}

		public static Point Difference(Point ptStart, Point ptEnd)
		{
			return new Point(ptEnd.X - ptStart.X, ptEnd.Y - ptStart.Y);
		}

		public static Point MidPoint(Point ptStart, Point ptEnd)
		{
			return new Point((ptStart.X + ptEnd.X) / 2, (ptStart.Y + ptEnd.Y) / 2);
		}

		public static double Distance(Point ptStart, Point ptEnd)
		{
			return Math.Sqrt(DistanceSquared(ptStart, ptEnd));
		}

		public static double DistanceSquared(Point ptStart, Point ptEnd)
		{
			var ptDiff = Difference(ptEnd, ptStart);

			return ((ptDiff.X * ptDiff.X) + (ptDiff.Y * ptDiff.Y));
		}

		public static bool DistanceFromPointToSegment(Point pt, Point segStart, Point segEnd, ref double distance, ref Point ptIntersection)
		{
			double distSqrd = DistanceSquared(segStart, segEnd);

			if (distSqrd == 0.0)
			{
				return false;
			}

			double U = ((pt.X - segStart.X) * (segEnd.X - segStart.X)) +
					   ((pt.Y - segStart.Y) * (segEnd.Y - segStart.Y));

			U /= distSqrd;

			if ((U < 0.0) || (U > 1.0))
			{
				// closest point does not fall within the line segment
				return false;
			}

			ptIntersection.X = (int)(segStart.X + (U * (segEnd.X - segStart.X)));
			ptIntersection.Y = (int)(segStart.Y + (U * (segEnd.Y - segStart.Y)));

			distance = Distance(pt, ptIntersection);

			return true;
		}

		public static bool HitTest(Point[] points, 
									Point point, 
									double tolerance,
									ref int segment, 
									ref Point ptIntersection)
		{
			for (int i = 1; i < points.Count(); i++)
			{
				Point ptStart = points[i - 1];
				Point ptEnd = points[i];

				double distance = double.MaxValue;
				Point ptOnSeg = Point.Empty;

				if (Geometry2D.DistanceFromPointToSegment(point,
											   ptStart,
											   ptEnd,
											   ref distance,
											   ref ptOnSeg))
				{
					if (distance <= tolerance)
					{
						ptIntersection = ptOnSeg;
						segment = (i - 1);

						// We can stop as soon as we hit tolerance
						return true;
					}
				}
			}

			return false;
		}

		public static int HitTest(Point[] points, Point point, double tolerance)
		{
			int segment = -1;
			Point ptUnused = Point.Empty;

			if (HitTest(points, point, tolerance, ref segment, ref ptUnused))
				return segment;

			return -1;
		}

		public static bool HitTestSegment(Point ptStart, Point ptEnd, Point point, double tolerance)
		{
			double distance = double.MaxValue;
			Point ptOnSeg = Point.Empty;

			if (Geometry2D.DistanceFromPointToSegment(point, ptStart, ptEnd,
														ref distance, ref ptOnSeg))
			{
				return (distance <= tolerance);
			}

			// else
			return false;
		}

		public static Point SegmentMidPoint(Point[] points, int segment)
		{
			if ((segment < 0) || (segment >= points.Count()))
				return Point.Empty;

			return MidPoint(points[segment], points[segment + 1]);
		}

		public static Point Centroid(Rectangle rect)
		{
			return new Point(rect.Left + (rect.Width / 2), rect.Top + (rect.Height / 2));
		}

		public static bool IntersectLineSegments(Point line1Start, Point line1End, Point line2Start, Point line2End, out Point ptIntersect)
		{
			// Start by finding the intersection of the two line vectors
			ptIntersect = Point.Empty;

			// Line 1
			int A1 = (line1End.Y - line1Start.Y);
			int B1 = (line1Start.X - line1End.X);
			int C1 = ((A1 * line1Start.X) + (B1 * line1Start.Y));

			// Line 2
			int A2 = (line2End.Y - line2Start.Y);
			int B2 = (line2Start.X - line2End.X);
			int C2 = ((A2 * line2Start.X) + (B2 * line2Start.Y));

			float det = (float)((A1 * B2) - (A2 * B1));

			if (det == 0)
			{
				// Parallel lines
				return false;
			}

			ptIntersect.X = (int)(((B2 * C1) - (B1 * C2)) / det);
			ptIntersect.Y = (int)(((A1 * C2) - (A2 * C1)) / det);

			// If the intersection is within both bounding boxes
			// then the segments have a proper intersection
			var line1Rect = RectFromPoints(line1Start, line1End);

			if (!Geometry2D.LineExtentsContainsPoint(line1Rect, ptIntersect))
				return false;
			
			var line2Rect = RectFromPoints(line2Start, line2End);

			return Geometry2D.LineExtentsContainsPoint(line2Rect, ptIntersect);
		}

		private static bool LineExtentsContainsPoint(Rectangle lineExtents, Point pt)
		{
			if (lineExtents.Contains(pt))
				return true;

			if (lineExtents.Width == 0)
			{
				// Vertical
				return ((pt.X == lineExtents.X) && (pt.Y < lineExtents.Bottom) && (pt.Y >= lineExtents.Top));
			}

			if (lineExtents.Height == 0)
			{
				// Horizontal
				return ((pt.Y == lineExtents.Y) && (pt.X >= lineExtents.Left) && (pt.X < lineExtents.Right));
			}

			// all else
			return false;
		}

		public static int IntersectLineSegmentWithRectangle(Point lineStart, Point lineEnd, Rectangle rect, out Point[] ptIntersect)
		{
			// Weed out easy cases ---------------------------------
			ptIntersect = new Point[2] { Point.Empty, Point.Empty };

			// Both points to left of rect
			if ((lineStart.X < rect.Left) && (lineEnd.X < rect.Left))
				return 0;

			// Both points above rect
			if ((lineStart.Y < rect.Top) && (lineEnd.Y < rect.Top))
				return 0;

			// Both points to right of rect
			if ((lineStart.X > rect.Right) && (lineEnd.X > rect.Right))
				return 0;

			// Both points below rect
			if ((lineStart.Y > rect.Bottom) && (lineEnd.Y > rect.Bottom))
				return 0;

			// Intersect with each edge ----------------------------

			int p = 0;

			// Left edge
			if (IntersectLineSegments(lineStart, lineEnd, new Point(rect.Left, rect.Bottom), new Point(rect.Left, rect.Top), out ptIntersect[p]))
				p++;

			// Top edge
			if (IntersectLineSegments(lineStart, lineEnd, new Point(rect.Left, rect.Top), new Point(rect.Right, rect.Top), out ptIntersect[p]))
				p++;

			// Right edge
			if ((p < 2) && IntersectLineSegments(lineStart, lineEnd, new Point(rect.Right, rect.Top), new Point(rect.Right, rect.Bottom), out ptIntersect[p]))
				p++;

			// Bottom edge
			if ((p < 2) && IntersectLineSegments(lineStart, lineEnd, new Point(rect.Left, rect.Bottom), new Point(rect.Right, rect.Bottom), out ptIntersect[p]))
				p++;

			return p;
		}

	}

}
