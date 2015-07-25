/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_EXCEPTIONS_HPP
#define OSHGUI_EXCEPTIONS_HPP

#include <exception>
#include "../Exports.hpp"
#include "Strings.hpp"

namespace OSHGui
{
	namespace Misc
	{
		/**
		 * Stellt Fehler dar, die beim Ausführen einer Anwendung auftreten.
		 */
		class OSHGUI_EXPORT Exception : public std::exception
		{
		public:
			/**
			 * Initialisiert eine neue Instanz der Exception-Klasse.
			 *
			 * \param message die Meldung, in der der Fehler beschrieben wird
			 */
			Exception(Misc::AnsiString message = "");
			virtual ~Exception() throw();
		
			/**
			 * Ruft die Meldung ab, die die aktuelle Ausnahme beschreibt.
			 *
			 * \return message
			 */
			const Misc::AnsiString& GetMessage() const;

			/**
			 * Ruft die Zeichenfolgenentsprechung der aktuellen Ausnahme ab.
			 *
			 * \return
			 */
			//overwrite std::exception what()
			const char* what() const throw();
		
		protected:
			Misc::AnsiString message;
			Misc::AnsiString file;
			int line;
			
		private:
			//copying prohibited
			/*Exception(const Exception&);
			void operator = (const Exception&);*/
		};
	
		/**
		 * Die Ausnahme, die ausgelöst wird, wenn eines der Argumente für eine Methode ungültig ist.
		 */
		class OSHGUI_EXPORT ArgumentException : public Exception
		{
		public:
			/**
			 * Initialisiert eine neue Instanz der ArgumentException-Klasse.
			 *
			 * \param paramName der Name des Arguments
			 */
			ArgumentException(Misc::AnsiString paramName = "");
			/**
			 * Initialisiert eine neue Instanz der ArgumentException-Klasse.
			 *
			 * \param paramName der Name des Arguments
			 * \param message die Meldung, in der der Fehler beschrieben wird
			 */
			ArgumentException(Misc::AnsiString paramName,
							  Misc::AnsiString message);
		
			/**
			 * Ruft den Namen des Parameters ab, der die Ausnahme auslöste.
			 *
			 * \return paramName
			 */
			const Misc::AnsiString& GetParamName() const;
	
		private:
			Misc::AnsiString paramName;
		};
	
		/**
		 * Die Ausnahme, die ausgelöst wird, wenn ein NULL-Verweis an eine Methode
		 * übergeben wird, für die dies kein gültiges Argument ist.
		 */
		class OSHGUI_EXPORT ArgumentNullException : public ArgumentException
		{
		public:
			/**
			 * Initialisiert eine neue ArgumentNullException der Exception-Klasse.
			 *
			 * \param paramName der Name des Arguments
			 */
			ArgumentNullException(const Misc::AnsiString &paramName);
			/**
			 * Initialisiert eine neue ArgumentNullException der Exception-Klasse.
			 *
			 * \param paramName der Name des Arguments
			 * \param message die Meldung, in der der Fehler beschrieben wird
			 */
			ArgumentNullException(const Misc::AnsiString &paramName,
								  const Misc::AnsiString &message);
		};
	
		/**
		 * Die Ausnahme, die ausgelöst wird, wenn der Wert eines Arguments nicht in dem Wertebereich
		 * liegt, der durch die aufgerufene Methode als zulässig definiert ist.
		 */
		class OSHGUI_EXPORT ArgumentOutOfRangeException : public ArgumentException
		{
		public:
			/**
			 * Initialisiert eine neue Instanz der ArgumentOutOfRangeException-Klasse.
			 *
			 * \param paramName der Name des Arguments
			 */
			ArgumentOutOfRangeException(const Misc::AnsiString &paramName);
			/**
			 * Initialisiert eine neue Instanz der ArgumentOutOfRangeException-Klasse.
			 *
			 * \param paramName der Name des Arguments
			 * \param message die Meldung, in der der Fehler beschrieben wird
			 */
			ArgumentOutOfRangeException(const Misc::AnsiString &paramName,
										const Misc::AnsiString &message);
		};
	
		/**
		 * Die Ausnahme, die ausgelöst wird, wenn der Aufruf einer Methode
		 * aufgrund des aktuellen Zustands des Objekts ungültig ist.
		 */
		class OSHGUI_EXPORT InvalidOperationException : public Exception
		{
		public:
			/**
			 * Initialisiert eine neue Instanz der InvalidOperationException-Klasse.
			 *
			 * \param message die Meldung, in der der Fehler beschrieben wird
			 */
			InvalidOperationException(const Misc::AnsiString &message);
		};
		
		/**
		 * Die Ausnahme, die ausgelöst wird, wenn beim Verarbeiten eines Themes
		 * ein Fehler aufgetreten ist.
		 */
		class OSHGUI_EXPORT InvalidThemeException : public Exception
		{
		public:
			/**
			 * Initialisiert eine neue Instanz der InvalidThemeException-Klasse.
			 *
			 * \param message die Meldung, in der der Fehler beschrieben wird
			 */
			InvalidThemeException(const Misc::AnsiString &message);
		};

		/**
		 * Die Ausnahme, die ausgelöst wird, wenn eine Funktion nicht unterstützt wird.
		 */
		class OSHGUI_EXPORT NotSupportedException : public Exception
		{
		public:
			/**
			 * Initialisiert eine NotSupportedException.
			 */
			NotSupportedException();
		};
		
		/**
		 * Die Ausnahme, die ausgelöst wird, wenn eine Datei nicht gefunden werden konnte.
		 */
		class OSHGUI_EXPORT FileNotFoundException : public Exception
		{
		public:
			/**
			 * Initialisiert eine FileNotFoundException.
			 */
			FileNotFoundException();
		};
	}
}

#endif