/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

//Credits Microsoft .NET
#ifndef OSHGUI_MISC_DATETIME_HPP
#define OSHGUI_MISC_DATETIME_HPP

#include "../Exports.hpp"
#include "Strings.hpp"
#include "TimeSpan.hpp"

namespace OSHGui
{
	namespace Misc
	{
		/**
		 * Gibt an, ob ein DateTime-Objekt eine lokale Zeit, eine koordinate Weltzeit
		 * (UTC) angibt oder weder lokale Zeit noch UTC angegeben sind.
		 */
		enum class DateTimeKind
		{
			Unspecified = 0,
			Utc,
			Local
		};
		
		/**
		 * Gibt den Wochentag an.
		 */
		enum class DayOfWeek
		{
			Sunday = 0,
			Monday,
			Tuesday,
			Wednesday,
			Thursday,
			Friday,
			Saturday
		};

		/**
		 * Stellt einen Zeitpunkt dar, der durch Datum und Uhrzeit dargestellt wird.
		 */
		class OSHGUI_EXPORT DateTime
		{
		private:
			enum class DatePart
			{
				Year = 0,
				DayOfYear,
				Month,
				Day
			};
		
			//Number of 100ns ticks per time unit
			static const unsigned long long TicksPerMillisecond;
			static const unsigned long long TicksPerSecond;
			static const unsigned long long TicksPerMinute;
			static const unsigned long long TicksPerHour;
			static const unsigned long long TicksPerDay;

			// Number of milliseconds per time unit
			static const unsigned int MillisPerSecond;
			static const unsigned int MillisPerMinute;
			static const unsigned int MillisPerHour;
			static const unsigned int MillisPerDay;

			// Number of days in a non-leap year
			static const unsigned int DaysPerYear;
			// Number of days in 4 years
			static const unsigned int DaysPer4Years;
			// Number of days in 100 years
			static const unsigned int DaysPer100Years;
			// Number of days in 400 years
			static const unsigned int DaysPer400Years;

			// Number of days from 1/1/0001 to 12/31/1600
			static const unsigned int DaysTo1601;
			// Number of days from 1/1/0001 to 12/30/1899
			static const unsigned int DaysTo1899;
			// Number of days from 1/1/0001 to 12/31/9999
			static const unsigned int DaysTo10000;

			static const long long MinTicks;
			static const long long MaxTicks;
			static const unsigned long long MaxMillis;
 
			static const unsigned long long TimezoneOffset;
			static const unsigned long long FileTimeOffset;
			static const unsigned long long DoubleDateOffset;

			static const int DaysToMonth365[13];
			static const int DaysToMonth366[13];
 
			static const unsigned long long TicksMask;
			static const unsigned long long FlagsMask;
			static const unsigned long long LocalMask;
			static const long long TicksCeiling;
			static const unsigned long long KindUnspecified;
			static const unsigned long long KindUtc;
			static const unsigned long long KindLocal;
			static const unsigned long long KindLocalAmbiguousDst;
			static const int KindShift;

			static const AnsiString DayNames[7];
			static const AnsiString MonthNames[12];

			unsigned long long dateData_;

		public:
			/**
			 * Stellt den kleinstmöglichen Wert von DateTime dar.
			 */
			static const DateTime MinValue;
			/**
			 * Stellt den größtmöglichen Wert von DateTime dar.
			 */
			static const DateTime MaxValue;

		private:
			DateTime(unsigned long long dateData);
			DateTime(long long ticks, DateTimeKind kind, bool isAmbiguousDst);
			
			long long GetInternalTicks() const;
			unsigned long long GetInternalKind() const;
			
			DateTime Add(double value, int scale) const;
			
			static long long DateToTicks(int year, int month, int day);
			static long long TimeToTicks(int hour, int minute, int second);
			
			int GetDatePart(DatePart part) const;
			
		public:
			/**
			 * Initialisiert eine neue Instanz der DateTime-Struktur als MinValue.
			 */
			DateTime();
			/**
			 * Initialisiert eine neue Instanz der DateTime-Struktur mit der angegebenen Anzahl an Ticks.
			 *
			 * \param ticks Eine in der Einheit 100 Nanosekunden ausgedrückte Datums- und Uhrzeitangabe
			 */
			DateTime(long long ticks);
			/**
			 * Initialisiert eine neue Instanz der DateTime-Struktur mit der angegebenen Anzahl an Ticks
			 * und koordinierter Weltzeit (UTC) oder lokaler Zeit.
			 *
			 * \param ticks Eine in der Einheit 100 Nanosekunden ausgedrückte Datums- und Uhrzeitangabe
			 * \param kind Einer der DateTimeKind-Werte, der angibt, ob ticks eine lokale Zeit, UTC angibt oder keine Angabe enthält
			 */
			DateTime(long long ticks, DateTimeKind kind);
			/**
			 * Initialisiert eine neue Instanz der DateTime-Struktur mit dem angegebenen Jahr, Monat und Tag.
			 *
			 * \param year das Jahr (1 bis 9999)
			 * \param month der Monat (1 bis 12)
			 * \param day der Tag (1 bis zur Anzahl der Tage in month)
			 */
			DateTime(int year, int month, int day);
			/**
			 * Initialisiert eine neue Instanz der DateTime-Struktur mit dem angegebenen Jahr, Monat, Tag, Stunde, Minute und Sekunde.
			 *
			 * \param year das Jahr (1 bis 9999)
			 * \param month der Monat (1 bis 12)
			 * \param day der Tag (1 bis zur Anzahl der Tage in month)
			 * \param hour die Stunde (0 bis 23)
			 * \param minute die Minute (0 bis 59)
			 * \param second die Sekunde (0 bis 59)
			 */
			DateTime(int year, int month, int day, int hour, int minute, int second);
			/**
			 * Initialisiert eine neue Instanz der DateTime-Struktur mit dem angegebenen Jahr, Monat, Tag, Stunde, Minute, Sekunde
			 * und koordinierter Weltzeit (UTC) oder lokaler Zeit.
			 *
			 * \param year das Jahr (1 bis 9999)
			 * \param month der Monat (1 bis 12)
			 * \param day der Tag (1 bis zur Anzahl der Tage in month)
			 * \param hour die Stunden (0 bis 23)
			 * \param minute die Minuten (0 bis 59)
			 * \param second die Sekunden (0 bis 59)
			 * \param kind Einer der DateTimeKind-Werte, der angibt, ob ticks eine lokale Zeit, UTC angibt oder keine Angabe enthält
			 */
			DateTime(int year, int month, int day, int hour, int minute, int second, DateTimeKind kind);
			/**
			 * Initialisiert eine neue Instanz der DateTime-Struktur mit dem angegebenen Jahr, Monat, Tag, Stunde, Minute, Sekunde
			 * und Milisekunden.
			 *
			 * \param year das Jahr (1 bis 9999)
			 * \param month der Monat (1 bis 12)
			 * \param day der Tag (1 bis zur Anzahl der Tage in month)
			 * \param hour die Stunden (0 bis 23)
			 * \param minute die Minuten (0 bis 59)
			 * \param second die Sekunden (0 bis 59)
			 * \param millisecond die Millisekunden (0 bis 999)
			 */
			DateTime(int year, int month, int day, int hour, int minute, int second, int millisecond);
			/**
			 * Initialisiert eine neue Instanz der DateTime-Struktur mit dem angegebenen Jahr, Monat, Tag, Stunde, Minute, Sekunde,
			 * Milisekunden und koordinierter Weltzeit (UTC) oder lokaler Zeit.
			 *
			 * \param year das Jahr (1 bis 9999)
			 * \param month der Monat (1 bis 12)
			 * \param day der Tag (1 bis zur Anzahl der Tage in month)
			 * \param hour die Stunden (0 bis 23)
			 * \param minute die Minuten (0 bis 59)
			 * \param second die Sekunden (0 bis 59)
			 * \param millisecond die Millisekunden (0 bis 999)
			 * \param kind Einer der DateTimeKind-Werte, der angibt, ob ticks eine lokale Zeit, UTC angibt oder keine Angabe enthält
			 */
			DateTime(int year, int month, int day, int hour, int minute, int second, int millisecond, DateTimeKind kind);
			
			/**
			 * Ruft die Datumskomponente dieser Instanz ab.
			 *
			 * \return die Datumskomponente
			 */
			DateTime GetDate() const;
			/**
			 * Ruft die Uhrzeit für diese Instanz ab.
			 *
			 * \return die Uhrzeit
			 */
			TimeSpan GetTimeOfDay() const;
			/**
			 * Ruft das Jahr ab, das durch diese Instanz dargestellt wird.
			 *
			 * \return das Jahr
			 */
			int GetYear() const;
			/**
			 * Ruft den Monat ab, der durch diese Instanz dargestellt wird.
			 *
			 * \return der Monat
			 */
			int GetMonth() const;
			/**
			 * Ruft den Tag des Monats ab, der durch diese Instanz dargestellt wird.
			 *
			 * \return der Tag
			 */
			int GetDay() const;
			/**
			 * Ruft den Wochentag ab, der durch diese Instanz dargestellt wird.
			 *
			 * \return der Wochentag
			 */
			DayOfWeek GetDayOfWeek() const;
			/**
			 * Ruft den Tag des Jahres ab, der durch diese Instanz dargestellt wird.
			 *
			 * \return der Tag
			 */
			int GetDayOfYear() const;
			/**
			 * Ruft die Komponente für die Stunden des Datums ab, das durch diese Instanz dargestellt wird.
			 *
			 * \return die Stunden
			 */
			int GetHour() const;
			/**
			 * Ruft die Komponente für die Minuten des Datums ab, das durch diese Instanz dargestellt wird.
			 *
			 * \return die Minuten
			 */
			int GetMinute() const;
			/**
			 * Ruft die Komponente für die Sekunden des Datums ab, das durch diese Instanz dargestellt wird.
			 *
			 * \return die Sekunden
			 */
			int GetSecond() const;
			/**
			 * Ruft die Komponente für die Millisekunden des Datums ab, das durch diese Instanz dargestellt wird.
			 *
			 * \return die Millisekunden
			 */
			int GetMillisecond() const;
			/**
			 * Ruft die Anzahl der Ticks ab, die Datum und Uhrzeit dieser Instanz dargestellen.
			 *
			 * \return die Ticks
			 */
			long long GetTicks() const;
			
			/**
			 * Ruft einen Wert ab, der angibt, ob die durch diese Instanz dargestellte Zeit
			 * auf lokaler Zeit, koordinierter Weltzeit (UTC) oder keinem von beidem basiert.
			 *
			 * \return der DateTimeKind-Wert
			 */
			DateTimeKind GetKind() const;
			
			/**
			 * Ruft ein DateTime-Objekt ab, das auf das aktuelle Datum und die aktuelle Zeit
			 * auf dem lokalen Rechner als lokale Zeit festgelegt ist.
			 *
			 * \return das DateTime-Objekt
			 */
			static DateTime GetNow();
			/**
			 * Ruft ein DateTime-Objekt ab, das auf das aktuelle Datum und die aktuelle Zeit
			 * auf dem lokalen Rechner als lokale Zeit festgelegt ist.
			 *
			 * \return das DateTime-Objekt
			 */
			static DateTime GetUtcNow();
			/**
			 * Ruft das aktuelle Datum ab.
			 *
			 * \return das aktuelle Datum
			 */
			static DateTime GetToday();
			
			/**
			 * Addiert den Wert der angegebenen TimeSpan zum Wert dieser Instanz.
			 *
			 * \param value ein TimeSpan-Objekt, das ein positives oder negatives Zeitintervall darstellt
			 * \return eine DateTime-Struktur
			 */
			DateTime Add(TimeSpan value) const;
			/**
			 * Addiert die angegebene Anzahl von Jahren zum Wert dieser Instanz und gibt die neue DateTime-Struktur zurück.
			 *
			 * \param value eine Anzahl von Jahren. value kann negativ sein.
			 * \return eine DateTime-Struktur
			 */
			DateTime AddYears(int value) const;
			/**
			 * Addiert die angegebene Anzahl von Monaten zum Wert dieser Instanz und gibt die neue DateTime-Struktur zurück.
			 *
			 * \param months eine Anzahl von Monaten. value kann negativ sein.
			 * \return eine DateTime-Struktur
			 */
			DateTime AddMonths(int months) const;
			/**
			 * Addiert die angegebene Anzahl von Tagen zum Wert dieser Instanz und gibt die neue DateTime-Struktur zurück.
			 *
			 * \param value eine Zahl, die aus ganzen Tagen und dem Bruchteil eines Tages besteht. value kann negativ sein.
			 * \return eine DateTime-Struktur
			 */
			DateTime AddDays(double value) const;
			/**
			 * Addiert die angegebene Anzahl von Stunden zum Wert dieser Instanz und gibt die neue DateTime-Struktur zurück.
			 *
			 * \param value eine Zahl, die aus ganzen Stunden und dem Bruchteil einer Stunde besteht. value kann negativ sein.
			 * \return eine DateTime-Struktur
			 */
			DateTime AddHours(double value) const;
			/**
			 * Addiert die angegebene Anzahl von Minuten zum Wert dieser Instanz und gibt die neue DateTime-Struktur zurück.
			 *
			 * \param value eine Zahl, die aus ganzen Minuten und dem Bruchteil einer Minute besteht. value kann negativ sein.
			 * \return eine DateTime-Struktur
			 */
			DateTime AddMinutes(double value) const;
			/**
			 * Addiert die angegebene Anzahl von Sekunden zum Wert dieser Instanz und gibt die neue DateTime-Struktur zurück.
			 *
			 * \param value eine Zahl, die aus ganzen Sekunden und dem Bruchteil einer Sekunde besteht. value kann negativ sein.
			 * \return eine DateTime-Struktur
			 */
			DateTime AddSeconds(double value) const;
			/**
			 * Addiert die angegebene Anzahl von Millisekunden zum Wert dieser Instanz und gibt die neue DateTime-Struktur zurück.
			 *
			 * \param value eine Zahl, die aus ganzen Millisekunden und dem Bruchteil einer Millisekunde besteht. value kann negativ sein.
			 * \return eine DateTime-Struktur
			 */
			DateTime AddMilliseconds(double value) const;
			/**
			 * Addiert die angegebene Anzahl von Ticks zum Wert dieser Instanz und gibt die neue DateTime-Struktur zurück.
			 *
			 * \param value eine Anzahl von 100-Nanosekunden-Ticks. value kann negativ sein.
			 * \return eine DateTime-Struktur
			 */
			DateTime AddTicks(long long value) const;
			
			bool operator == (const DateTime &time) const;
			bool operator != (const DateTime &time) const;
			bool operator < (const DateTime &time) const;
			bool operator > (const DateTime &time) const;
			bool operator <= (const DateTime &time) const;
			bool operator >= (const DateTime &time) const;
			const DateTime operator - (const TimeSpan &ts) const;
			const DateTime operator + (const TimeSpan &ts) const;
			const TimeSpan operator - (const DateTime &time) const;
			const TimeSpan operator + (const DateTime &time) const;
			
			/**
			 * Ruft die Anzahl der Tage im angegeben Monat und Jahr ab.
			 *
			 * \param year das Jahr (1 bis 9999)
			 * \param month der Monat (1 bis 12)
			 * \return die Anzahl der Tage
			 */
			static int DaysInMonth(int year, int month);
			
			/**
			 * Erstellt ein neues DateTime-Objekt, das die gleiche Zeit wie die angegebene DateTime darstellt, aber entsprechend dem
			 * DateTimeKind-Wert in Ortszeit, koordinierter Weltzeit (UTC) oder keinem von beidem angegeben ist.
			 *
			 * \param value die DateTime-Struktur
			 * \param kind Einer der DateTimeKind-Werte, der angibt, ob ticks eine lokale Zeit, UTC angibt oder keine Angabe enthält
			 * \return die neue DateTime-Struktur
			 */
			static DateTime SpecifyKind(DateTime value, DateTimeKind kind);
			
			/**
			 * Ruft ab, ob das angegebene Jahr ein Schaltjahr ist.
			 *
			 * \param year das Jahr (1 bis 9999)
			 * \return ja/nein
			 */
			static bool IsLeapYear(int year);
			
			/**
			 * Ruft den Unterschied der aktuellen Zeitzone zur UTC ab.
			 *
			 * \return der Zeitzonenunterschied
			 */
			static long long GetTimezoneOffset();

			/**
			 * Gibt eine Zeichenfolgendarstellung des DateTime-Objekts zurück.
			 * Format: d.m.Y H:i:s => 23.04.2011 02:23:48
			 *
			 * \return die Zeichenfolgendarstellung
			 */
			AnsiString ToString();
			/**
			 * Gibt eine Zeichenfolgendarstellung des DateTime-Objekts zurück.
			 * Format: l, d. F Y => Saturday, 23. April 2011
			 *
			 * \return die Zeichenfolgendarstellung
			 */
			AnsiString ToLongDateString();
			/**
			 * Gibt eine Zeichenfolgendarstellung des DateTime-Objekts zurück.
			 * Format: H:i:s => 02:23:48
			 *
			 * \return die Zeichenfolgendarstellung
			 */
			AnsiString ToLongTimeString();
			/**
			 * Gibt eine Zeichenfolgendarstellung des DateTime-Objekts zurück.
			 * Format: d.m.Y => 23.04.2011
			 *
			 * \return die Zeichenfolgendarstellung
			 */
			AnsiString ToShortDateString();
			/**
			 * Gibt eine Zeichenfolgendarstellung des DateTime-Objekts zurück.
			 * Format: H:i => 02:23
			 *
			 * \return die Zeichenfolgendarstellung
			 */
			AnsiString ToShortTimeString();
		};
	}
}

#endif
