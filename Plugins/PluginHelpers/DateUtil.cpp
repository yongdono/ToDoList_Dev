// PluginHelpers.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "DateUtil.h"
#include "Preferences.h"
#include "Win32.h"

#include <Shared\DateHelper.h>

////////////////////////////////////////////////////////////////////////////////////////////////

using namespace Abstractspoon::Tdl::PluginHelpers;

using namespace System::Collections::Generic;
using namespace System::Drawing;

////////////////////////////////////////////////////////////////////////////////////////////////

WorkingDay::WorkingDay()
	:
	m_StartOfDayInHours(9),
	m_StartOfLunchInHours(13),
	m_EndOfLunchInHours(14),
	m_WorkingLengthInHours(8)
{

}

WorkingDay::WorkingDay(Preferences^ prefs)
	:
	m_StartOfDayInHours(9),
	m_StartOfLunchInHours(13),
	m_EndOfLunchInHours(14),
	m_WorkingLengthInHours(8)
{
	Load(prefs);
}

double WorkingDay::StartOfDayInHours()
{
	return m_StartOfDayInHours;
}

double WorkingDay::EndOfDayInHours()
{
	return (m_StartOfDayInHours + DayLengthInHours(true));
}

double WorkingDay::StartOfLunchInHours()
{
	return m_StartOfLunchInHours;
}

double WorkingDay::EndOfLunchInHours()
{
	return m_EndOfLunchInHours;
}

DateTime WorkingDay::StartOfDay(DateTime date)
{
	return date.Date.AddMilliseconds(HoursToMilleseconds(m_StartOfDayInHours));
}

DateTime WorkingDay::EndOfDay(DateTime date)
{
	return date.Date.AddMilliseconds(HoursToMilleseconds(EndOfDayInHours()));
}

DateTime WorkingDay::StartOfLunch(DateTime date)
{
	return date.Date.AddMilliseconds(HoursToMilleseconds(m_StartOfLunchInHours));
}

DateTime WorkingDay::EndOfLunch(DateTime date)
{
	return date.Date.AddMilliseconds(HoursToMilleseconds(m_EndOfLunchInHours));
}

Int32 WorkingDay::HoursToMilleseconds(double hours)
{
	return (Int32)(hours * 60 * 60 * 1000);
}

double WorkingDay::CalculateDurationInHours(double fromHour, double toHour)
{
	fromHour = Math::Max(fromHour, m_StartOfDayInHours);
	toHour = Math::Min(toHour, EndOfDayInHours());

	if (fromHour >= toHour)
		return 0;

	if (fromHour >= m_StartOfLunchInHours)
		fromHour = Math::Max(fromHour, m_EndOfLunchInHours);

	if (toHour <= m_EndOfLunchInHours)
		toHour = Math::Min(toHour, m_StartOfLunchInHours);

	double dDuration = (toHour - fromHour);

	if (fromHour < m_StartOfLunchInHours && toHour > m_EndOfLunchInHours)
		dDuration -= LunchLengthInHours();

	return Math::Max(dDuration, 0.0);
}

double WorkingDay::GetTimeOfDayInHours(DateTime date)
{
	return (double)(date.Hour + (date.Minute / 60.0) + (date.Second / 3600.0) + (date.Millisecond / 3600000));
}

double WorkingDay::DayLengthInHours(bool includingLunch)
{
	if (includingLunch)
		return (m_WorkingLengthInHours + LunchLengthInHours());

	// else
	return m_WorkingLengthInHours;
}

double WorkingDay::LunchLengthInHours()
{
	return (m_EndOfLunchInHours - m_StartOfLunchInHours);
}

void WorkingDay::Load(Preferences^ prefs)
{
	m_StartOfDayInHours = prefs->GetProfileDouble(L"Preferences", L"StartOfWorkdayInHours", 9.0);
	m_WorkingLengthInHours = prefs->GetProfileDouble(L"Preferences", L"HoursInDay", 8.0);
	m_StartOfLunchInHours = prefs->GetProfileDouble(L"Preferences", L"StartOfLunchInHours", 13.0);

	if (prefs->GetProfileBool(L"Preferences", L"HasLunchBreak", true))
		m_EndOfLunchInHours = prefs->GetProfileDouble(L"Preferences", L"EndOfLunchInHours", 14.0);
	else
		m_EndOfLunchInHours = m_StartOfLunchInHours;
	
	// Validate lunch break
	if ((m_StartOfLunchInHours < m_StartOfDayInHours) ||
		(m_EndOfLunchInHours > EndOfDayInHours()) ||
		(m_StartOfLunchInHours > m_EndOfLunchInHours))
	{
		m_StartOfLunchInHours = m_EndOfLunchInHours;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////

enum 
{
	SUNDAY = 0X01,
	MONDAY = 0X02,
	TUESDAY = 0X04,
	WEDNESDAY = 0X08,
	THURSDAY = 0X10,
	FRIDAY = 0X20,
	SATURDAY = 0X40,
};

WorkingWeek::WorkingWeek() 
	: 
	m_WorkingDay(gcnew WorkingDay()),
	m_WeekendDays(gcnew List<System::DayOfWeek>())
{
	m_WeekendDays->Add(System::DayOfWeek::Saturday);
	m_WeekendDays->Add(System::DayOfWeek::Sunday);
}

WorkingWeek::WorkingWeek(Preferences^ prefs)
	:
	m_WorkingDay(gcnew WorkingDay()),
	m_WeekendDays(gcnew List<System::DayOfWeek>())
{
	Load(prefs);
}

void WorkingWeek::Load(Preferences^ prefs)
{
	m_WorkingDay->Load(prefs);

	int dwWeekends = prefs->GetProfileInt("Preferences", "Weekends", 0);
	m_WeekendDays = DateUtil::MapDaysOfWeek(dwWeekends);
}

double WorkingWeek::CalculateDurationInHours(System::DateTime from, DateTime to)
{
	int nDaysDuration = (to.Date - from.Date).Days;

	if (nDaysDuration < 0)
		return 0;

	double fromTimeOfDay = m_WorkingDay->GetTimeOfDayInHours(from);
	double toTimeOfDay = m_WorkingDay->GetTimeOfDayInHours(to);
	double dHoursDuration = 0;

	if (nDaysDuration == 0)
	{
		// from and to are same day
		if (!IsWeekend(from))
			dHoursDuration = m_WorkingDay->CalculateDurationInHours(fromTimeOfDay, toTimeOfDay);
	}
	else
	{
		if (nDaysDuration > 1)
		{
			// count whole days
			from = from.AddDays(1).Date;

			while (from <= to)
			{
				if (!IsWeekend(from))
					dHoursDuration += m_WorkingDay->DayLengthInHours(false);

				from = from.AddDays(1);
			}
		}
		
		// part days
		if (!IsWeekend(from))
			dHoursDuration += m_WorkingDay->CalculateDurationInHours(fromTimeOfDay, m_WorkingDay->EndOfDayInHours());

		if (!IsWeekend(to))
			dHoursDuration += m_WorkingDay->CalculateDurationInHours(m_WorkingDay->StartOfDayInHours(), toTimeOfDay);
	}

	return dHoursDuration;
}

bool WorkingWeek::IsWeekend(DateTime date)
{
	return m_WeekendDays->Contains(date.DayOfWeek);
}

////////////////////////////////////////////////////////////////////////////////////////////////

int DateUtil::WeekOfYear(DateTime date)
{
	return CDateHelper::GetWeekofYear(date.ToOADate());
}

int DateUtil::GetMaxDayOfWeekNameWidth(Graphics^ graphics, Font^ font, bool shortName)
{
	HDC hDC = Win32::GetHdc(graphics->GetHdc());

	HFONT hFont = Win32::GetHfont(font->ToHfont());
	HFONT hOldFont = (HFONT)::SelectObject(hDC, hFont);

	int width = CDateHelper::GetMaxDayOfWeekNameWidth(CDC::FromHandle(hDC), (shortName ? TRUE : FALSE));

	// cleanup
	::SelectObject(hDC, hOldFont);
	::DeleteObject(hFont);
	graphics->ReleaseHdc();

	return width;
}

int DateUtil::GetMaxMonthNameWidth(Graphics^ graphics, Font^ font, bool shortName)
{
	HDC hDC = Win32::GetHdc(graphics->GetHdc());

	HFONT hFont = Win32::GetHfont(font->ToHfont());
	HFONT hOldFont = (HFONT)::SelectObject(hDC, hFont);

	int width = CDateHelper::GetMaxMonthNameWidth(CDC::FromHandle(hDC), (shortName ? TRUE : FALSE));

	// cleanup
	::SelectObject(hDC, hOldFont);
	::DeleteObject(hFont);
	graphics->ReleaseHdc();

	return width;
}

String^ DateUtil::GetMonthName(int nMonth, bool shortName)
{
	return gcnew String(CDateHelper::GetMonthName(nMonth, (shortName ? TRUE : FALSE)));
}

int DateUtil::DateInMonths(DateTime date)
{
	return ((date.Year * 12) + date.Month);
}

DateTime DateUtil::DateFromMonths(int nMonths)
{
	int nYear = (nMonths / 12);
	int nMonth = (nMonths % 12);

	return DateTime(nYear, nMonth, 1);
}

TimeSpan DateUtil::TimeOnly(DateTime date)
{
	return (date - date.Date);
}

int DateUtil::MapDaysOfWeek(List<System::DayOfWeek>^ days)
{
	int dwDays = 0;

	if (days->Contains(System::DayOfWeek::Sunday))
		dwDays |= SUNDAY;

	if (days->Contains(System::DayOfWeek::Saturday))
		dwDays |= SATURDAY;

	if (days->Contains(System::DayOfWeek::Monday))
		dwDays |= MONDAY;

	if (days->Contains(System::DayOfWeek::Tuesday))
		dwDays |= TUESDAY;

	if (days->Contains(System::DayOfWeek::Wednesday))
		dwDays |= WEDNESDAY;

	if (days->Contains(System::DayOfWeek::Thursday))
		dwDays |= THURSDAY;

	if (days->Contains(System::DayOfWeek::Friday))
		dwDays |= FRIDAY;

	return dwDays;
}

List<System::DayOfWeek>^ DateUtil::MapDaysOfWeek(int dwDays)
{
	auto days = gcnew List<System::DayOfWeek>();

	if ((dwDays & SUNDAY) == SUNDAY)
		days->Add(System::DayOfWeek::Sunday);

	if ((dwDays & SATURDAY) == SATURDAY)
		days->Add(System::DayOfWeek::Saturday);

	if ((dwDays & MONDAY) == MONDAY)
		days->Add(System::DayOfWeek::Monday);

	if ((dwDays & TUESDAY) == TUESDAY)
		days->Add(System::DayOfWeek::Tuesday);

	if ((dwDays & WEDNESDAY) == WEDNESDAY)
		days->Add(System::DayOfWeek::Wednesday);

	if ((dwDays & THURSDAY) == THURSDAY)
		days->Add(System::DayOfWeek::Thursday);

	if ((dwDays & FRIDAY) == FRIDAY)
		days->Add(System::DayOfWeek::Friday);

	return days;
}
