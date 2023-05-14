﻿using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

using Abstractspoon.Tdl.PluginHelpers;

namespace DayViewUIExtension
{
	public partial class DayViewCreateTimeBlockDlg : Form
	{
		public DayViewCreateTimeBlockDlg()
		{
			InitializeComponent();
		}

		public DayViewCreateTimeBlockDlg(IEnumerable<TaskItem> taskItems, 
										 UIExtension.TaskIcon taskIcons, 
										 WorkingWeek workWeek,
										 uint taskId,
										 Calendar.AppointmentDates dates)
			:
			this()
		{
			m_TaskCombo.Initialise(taskItems, taskIcons, taskId);
			m_Attributes.Initialise(workWeek, dates, false);
		}

		public uint SelectedTaskId
		{
			get { return m_TaskCombo.SelectedTaskId; }
		}

		public DateTime FromDate { get { return m_Attributes.FromDate; } }
		public DateTime ToDate { get { return m_Attributes.ToDate; } }

		public TimeSpan FromTime { get { return m_Attributes.FromTime; } }
		public TimeSpan ToTime { get { return m_Attributes.ToTime; } }

		public List<DayOfWeek> DaysOfWeek { get { return m_Attributes.DaysOfWeek; } }
		public bool SyncTimeBlocksToTaskDates { get { return m_Attributes.SyncTimeBlocksToTaskDates; } }
	}
}
