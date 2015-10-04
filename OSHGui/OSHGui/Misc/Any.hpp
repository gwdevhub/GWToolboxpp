/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_MISC_ANY_HPP
#define OSHGUI_MISC_ANY_HPP

#include "../Exports.hpp"

namespace OSHGui
{
	namespace Misc
	{
		/**
		 * Dieser Datentyp kann jeden anderen Datentyp aufnehmen.
		 */
		class OSHGUI_EXPORT Any
		{
		private:
			//Verhindert "ISO C++ forbids declaration of 'TypeWrapper' with no type"
			class AnyTypeWrapper
			{
			public:
				virtual ~AnyTypeWrapper() { }
				virtual void* GetObject() = 0;
			};
			//---------------------------------------------------------------------------
			template<typename T>
			class TypeWrapper : public AnyTypeWrapper
			{
			public:
				TypeWrapper(const T &object)
					: obj_(object)
				{
				
				}
				//---------------------------------------------------------------------------
				virtual void* GetObject()
				{
					return &obj_;
				}
				//---------------------------------------------------------------------------

			private:
				T obj_;
			};
			//---------------------------------------------------------------------------
			unsigned int id_;
			std::shared_ptr<AnyTypeWrapper> wrapper_;
			//---------------------------------------------------------------------------
			static unsigned int NextID()
			{
				static unsigned int id = 0;
				++id;
				return id;
			}
			//---------------------------------------------------------------------------
			template<typename T>
			static unsigned int TypeID()
			{
				static unsigned int id = NextID();
				return id;
			}
			//---------------------------------------------------------------------------
			
		public:
			/**
			 * Erzeugt ein leeres Any-Objekt.
			 */
			Any()
				: id_(0)
			{

			}

			/**
			 * Erzeugt ein Any-Objekt, das das angegebene Objekt enthält.
			 *
			 * \param obj
			 */
			template<typename T>
			Any(const T &obj)
				: id_(TypeID<T>()),
				  wrapper_(new TypeWrapper<T>(obj_))
			{
				
			}
			
			/**
			 * Dieser Operator erlaubt per if (!any) { any ist leer } zu prüfen, ob das Any-Objekt leer ist.
			 */
			operator bool() const
			{
				return id_ != 0;
			}

			/**
			 * Castet ein Any-Objekt zu dem in ihm befindlichen Datentyp. Falls ein falscher Datentyp angegeben wird,
			 * wird eine Exception ausgelöst.
			 *
			 * \return das aufgenommene Objekt
			 */
			template<typename T>
			T& CastTo() const
			{
				if (TypeID<T>() == id_)
				{
					return *static_cast<T*>(wrapper_->GetObject());
				}
				else
				{
					#ifndef OSHGUI_DONTUSEEXCEPTIONS
					throw InvalidOperationException("Cannot cast object");
					#endif
					throw 1;
				}
			}
		};
	}
}

#endif