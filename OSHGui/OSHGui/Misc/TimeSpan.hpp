/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

//Credits Microsoft .NET
#ifndef OSHGUI_MISC_TIMESPAN_HPP
#define OSHGUI_MISC_TIMESPAN_HPP

#include "../Exports.hpp"
#include "Strings.hpp"

namespace OSHGui
{
	namespace Misc
	{
		/**
		 * Stellt ein Zeitintervall dar.
		 */
		class TimeSpan
		{
		private:
			static const double MillisecondsPerTick;
			static const double SecondsPerTick;
			static const double MinutesPerTick;
			static const double HoursPerTick;
			static const double DaysPerTick;
			
			static const int MillisPerSecond;
			static const int MillisPerMinute;
			static const int MillisPerHour;
			static const int MillisPerDay;
			
			static const long long MaxSeconds;
			static const long long MinSeconds;
			static const long long MaxMilliSeconds;
			static const long long MinMilliSeconds;
			
			static const long long TicksPerTenthSecond;
			
			long long ticks_;
			
		public:
			/**
			 * Stellt die Anzahl der Ticks pro Tag dar.
			 */
			static const long long TicksPerDay;
			/**
			 * Stellt die Anzahl der Ticks pro Stunde dar.
			 */
			static const long long TicksPerHour;
			/**
			 * Stellt die Anzahl der Ticks pro Minute dar.
			 */
			static const long long TicksPerMinute;
			/**
			 * Stellt die Anzahl der Ticks pro Sekunde dar.
			 */
			static const long long TicksPerSecond;
			/**
			 * Stellt die Anzahl der Ticks pro Millisekunde dar.
			 */
			static const long long TicksPerMillisecond;

			/**
			 * Stellt den TimeSpan-Wert für 0 dar.
			 */
			static const TimeSpan Zero;
			/**
			 * Stellt den maximalen TimeSpan-Wert dar.
			 */
			static const TimeSpan MaxValue;
			/**
			 * Stellt den minimalen TimeSpan-Wert dar.
			 */
			static const TimeSpan MinValue;
			
		private:
			static TimeSpan Interval(double value, int scale);
			static long long TimeToTicks(int hour, int minute, int second);

		public:
			/**
			 * Initialisiert eine neue TimeSpan (siehe TimeSpan::Zero).
			 */
			TimeSpan();
			/**
			 * Initialisiert eine neue TimeSpan für die angegebene Anzahl Ticks.
			 *
			 * \param ticks eine in der Einheit 100 Nanosekunden ausgedrückte Zeitspanne
			 */
			TimeSpan(long long ticks);
			/**
			 * Initialisiert eine neue TimeSpan mit der angegebenen Anzahl von Stunden, Minuten und Sekunden.
			 *
			 * \param hours Anzahl der Stunden
			 * \param minutes Anzahl der Minuten
			 * \param seconds Anzahl der Sekunden
			 */
			TimeSpan(int hours, int minutes, int seconds);
			/**
			 * Initialisiert eine neue TimeSpan mit der angegebenen Anzahl von Tagen, Stunden,
			 * Minuten und Sekunden.
			 *
			 * \param days Anzahl der Tage
			 * \param hours Anzahl der Stunden
			 * \param minutes Anzahl der Minuten
			 * \param seconds Anzahl der Sekunden
			 */
			TimeSpan(int days, int hours, int minutes, int seconds);
			/**
			 * Initialisiert eine neue TimeSpan mit der angegebenen Anzahl von Tagen, Stunden,
			 * Minuten, Sekunden und Millisekunden.
			 *
			 * \param days Anzahl der Tage
			 * \param hours Anzahl der Stunden
			 * \param minutes Anzahl der Minuten
			 * \param seconds Anzahl der Sekunden
			 * \param milliseconds Anzahl der Millisekunden
			 */
			TimeSpan(int days, int hours, int minutes, int seconds, int milliseconds);
			
			/**
			 * Ruft die Tageskomponente des Zeitintervalls ab, das durch die
			 * aktuelle TimeSpan-Struktur dargestellt wird.
			 *
			 * \return die Tage
			 */
			int GetDays() const;
			/**
			 * Ruft den Wert der aktuellen TimeSpan-Struktur in ganzen Tagen und
			 * Bruchteilen von Tagen ab.
			 *
			 * \return die ganzen Tage und Bruchteile
			 */
			double GetTotalDays() const;
			/**
			 * Ruft die Stundenkomponente des Zeitintervalls ab, das durch die
			 * aktuelle TimeSpan-Struktur dargestellt wird.
			 *
			 * \return die Stunden
			 */
			int GetHours() const;
			/**
			 * Ruft den Wert der aktuellen TimeSpan-Struktur in ganzen Stunden und
			 * Bruchteilen von Stunden ab.
			 *
			 * \return die ganzen Stunden und Bruchteile
			 */
			double GetTotalHours() const;
			/**
			 * Ruft die Minutenkomponente des Zeitintervalls ab, das durch die
			 * aktuelle TimeSpan-Struktur dargestellt wird.
			 *
			 * \return die Minuten
			 */
			int GetMinutes() const;
			/**
			 * Ruft den Wert der aktuellen TimeSpan-Struktur in ganzen Minuten und
			 * Bruchteilen von Minuten ab.
			 *
			 * \return die ganzen Minuten und Bruchteile
			 */
			double GetTotalMinutes() const;
			/**
			 * Ruft die Sekundenkomponente des Zeitintervalls ab, das durch die
			 * aktuelle TimeSpan-Struktur dargestellt wird.
			 *
			 * \return die Sekunden
			 */
			int GetSeconds() const;
			/**
			 * Ruft den Wert der aktuellen TimeSpan-Struktur in ganzen Sekunden und
			 * Bruchteilen von Sekunden ab.
			 *
			 * \return die ganzen Sekunden und Bruchteile
			 */
			double GetTotalSeconds() const;
			/**
			 * Ruft die Millisekundenkomponente des Zeitintervalls ab, das durch die
			 * aktuelle TimeSpan-Struktur dargestellt wird.
			 *
			 * \return die Millisekunden
			 */
			int GetMilliseconds() const;
			/**
			 * Ruft den Wert der aktuellen TimeSpan-Struktur in ganzen Millisekunden und
			 * Bruchteilen von Millisekunden ab.
			 *
			 * \return die ganzen Millisekunden und Bruchteile
			 */
			double GetTotalMilliseconds() const;
			/**
			 * Ruft die Anzahl der Ticks des Zeitintervalls ab, das durch die
			 * aktuelle TimeSpan-Struktur dargestellt wird.
			 *
			 * \return die Ticks
			 */
			long long GetTicks() const;
			
			/**
			 * Addiert die angegebene TimeSpan zu dieser Instanz.
			 *
			 * \return eine TimeSpan
			 */
			const TimeSpan Add(TimeSpan ts) const;
			/**
			 * Subtrahiert die angegebene TimeSpan von dieser Instanz.
			 *
			 * \return eine TimeSpan
			 */
			const TimeSpan Subtract(TimeSpan ts) const;
			
			bool operator == (const TimeSpan &ts) const;
			bool operator != (const TimeSpan &ts) const;
			bool operator < (const TimeSpan &ts) const;
			bool operator > (const TimeSpan &ts) const;
			bool operator <= (const TimeSpan &ts) const;
			bool operator >= (const TimeSpan &ts) const;
			const TimeSpan operator - () const;
			const TimeSpan operator + () const;
			const TimeSpan operator - (const TimeSpan &ts) const;
			const TimeSpan operator + (const TimeSpan &ts) const;
			
			/**
			 * Gibt eine TimeSpan zurück, deren Wert der absoluten Wert dieser Instanz ist.
			 *
			 * \return die TimeSpan
			 */
			TimeSpan Duration() const;
			/**
			 * Gibt eine TimeSpan zurück, deren Wert der negierte Wert dieser Instanz ist.
			 *
			 * \return die TimeSpan
			 */
			TimeSpan Negate() const;
			
			/**
			 * Gibt eine TimeSpan zurück, die eine angegebene Anzahl von Tagen darstellt.
			 * Die Angabe wird auf die nächste Millisekunde gerundet.
			 *
			 * \param value eine Anzahl von Tagen, auf die nächste Millisekunde gerundet
			 * \return eine TimeSpan
			 */
			static TimeSpan FromDays(double value);
			/**
			 * Gibt eine TimeSpan zurück, die eine angegebene Anzahl von Stunden darstellt.
			 * Die Angabe wird auf die nächste Millisekunde gerundet.
			 *
			 * \param value eine Anzahl von Stunden, auf die nächste Millisekunde gerundet
			 * \return eine TimeSpan
			 */
			static TimeSpan FromHours(double value);
			/**
			 * Gibt eine TimeSpan zurück, die eine angegebene Anzahl von Minuten darstellt.
			 * Die Angabe wird auf die nächste Millisekunde gerundet.
			 *
			 * \param value eine Anzahl von Minuten, auf die nächste Millisekunde gerundet
			 * \return eine TimeSpan
			 */
			static TimeSpan FromMinutes(double value);
			/**
			 * Gibt eine TimeSpan zurück, die eine angegebene Anzahl von Sekunden darstellt.
			 * Die Angabe wird auf die nächste Millisekunde gerundet.
			 *
			 * \param value eine Anzahl von Sekunden, auf die nächste Millisekunde gerundet
			 * \return eine TimeSpan
			 */
			static TimeSpan FromSeconds(double value);
			/**
			 * Gibt eine TimeSpan zurück, die eine angegebene Anzahl von Millisekunden darstellt.
			 *
			 * \param value eine Anzahl von Millisekunden
			 * \return eine TimeSpan
			 */
			static TimeSpan FromMilliseconds(double value);
			/**
			 * Gibt eine TimeSpan zurück, die einen angegebenen Zeitraum in Ticks darstellt.
			 *
			 * \param value eine in der Einheit 100 Nanosekunden ausgedrückte Zeitspanne
			 * \return eine TimeSpan
			 */
			static TimeSpan FromTicks(long long value);
			
			/**
			 * Gibt die Zeichenfolgendarstellung des Werts dieser Instanz zurück.
			 */
			AnsiString ToString() const;
		};
	}
}

#endif
