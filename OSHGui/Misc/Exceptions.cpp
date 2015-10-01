/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include <iostream>
#include "Exceptions.hpp"

namespace OSHGui
{
	namespace Misc
	{
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		Exception::Exception(Misc::AnsiString _message)
			: message(std::move(_message))
		{
			std::cerr << what() << std::endl;
		}
		//---------------------------------------------------------------------------
		Exception::~Exception()
		{
	
		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		const Misc::AnsiString& Exception::GetMessage() const
		{
			return message;
		}
		//---------------------------------------------------------------------------
		const char* Exception::what() const throw()
		{
			return message.c_str();
		}
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		ArgumentException::ArgumentException(Misc::AnsiString _paramName)
			: paramName(std::move(_paramName)),
			  Exception("Unexpected argument.\nParameter name: " + paramName)
		{

		}
		//----------------------------------------------------------------------------
		ArgumentException::ArgumentException(Misc::AnsiString _paramName, Misc::AnsiString message)
			: paramName(std::move(_paramName)),
			  Exception(std::move(message))
		{

		}
		//---------------------------------------------------------------------------
		//Getter/Setter
		//---------------------------------------------------------------------------
		const Misc::AnsiString& ArgumentException::GetParamName() const
		{
			return paramName;
		}
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		ArgumentNullException::ArgumentNullException(const Misc::AnsiString &paramName)
			: ArgumentException(paramName, "Value cannot be null.")
		{

		}
		//---------------------------------------------------------------------------
		ArgumentNullException::ArgumentNullException(const Misc::AnsiString &paramName, const Misc::AnsiString &message)
			: ArgumentException(paramName, message)
		{

		}
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		ArgumentOutOfRangeException::ArgumentOutOfRangeException(const Misc::AnsiString &paramName)
			: ArgumentException(paramName, "Value was out of valid range.")
		{

		}
		//---------------------------------------------------------------------------
		ArgumentOutOfRangeException::ArgumentOutOfRangeException(const Misc::AnsiString &paramName, const Misc::AnsiString &message)
			: ArgumentException(paramName, message)
		{

		}
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		InvalidOperationException::InvalidOperationException(const Misc::AnsiString &message)
			: Exception(message)
		{

		}
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		InvalidThemeException::InvalidThemeException(const Misc::AnsiString &message)
			: Exception(message)
		{

		}
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		NotSupportedException::NotSupportedException()
			: Exception()
		{

		}
		//---------------------------------------------------------------------------
		//Constructor
		//---------------------------------------------------------------------------
		FileNotFoundException::FileNotFoundException()
			: Exception() { }
		FileNotFoundException::FileNotFoundException(const Misc::AnsiString &message)
			: Exception(message) { }
		//---------------------------------------------------------------------------
	}
}
