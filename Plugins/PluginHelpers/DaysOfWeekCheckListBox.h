#pragma once

#include <Shared\DayOfWeekCheckListBox.h>

////////////////////////////////////////////////////////////////////////////////////////////////

using namespace System;

////////////////////////////////////////////////////////////////////////////////////////////////

namespace Abstractspoon
{
	namespace Tdl
	{
		namespace PluginHelpers
		{
			class HostedDaysOfWeekCheckListBox
			{
			public:
				static HostedDaysOfWeekCheckListBox* Attach(HWND hwndParent, HFONT hFont);
				
				void Detach();

				DWORD GetSelectedDays();
				void SetSelectedDays(DWORD dwDays);

				void DrawItem(WPARAM wp, LPARAM lp);

			protected:
				HostedDaysOfWeekCheckListBox(HWND hwndParent);

			private:
				CWnd m_WndOfManagedHandle;
				CDayOfWeekCheckListBox m_ListBox;
			};

			// -------------------------------------------------------------------

			public ref class DaysOfWeekCheckListBox : Windows::Forms::Control
			{
			public:
				DaysOfWeekCheckListBox();

				Collections::Generic::List<System::DayOfWeek>^ GetSelectedDays();
				void SetSelectedDays(Collections::Generic::List<System::DayOfWeek>^ days);

				event System::EventHandler^ ChangeEvent;

			private:
				IntPtr m_pMFCInfo = IntPtr::Zero;

			protected:
				void WndProc(Windows::Forms::Message% m) override;
				void OnHandleCreated(EventArgs^ e) override;
				void OnHandleDestroyed(EventArgs^ e) override;
			};

		}
	}
}