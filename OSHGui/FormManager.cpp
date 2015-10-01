/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "FormManager.hpp"
#include "Application.hpp"
#include "Controls/Form.hpp"
#include "Misc/Exceptions.hpp"

namespace OSHGui
{
	const std::shared_ptr<Form> FormManager::GetForeMost() const
	{
		if (!forms_.empty())
		{
			return forms_.back().Form;
		}
		return nullptr;
	}
	//---------------------------------------------------------------------------
	const std::shared_ptr<Form>& FormManager::operator[](int index) const
	{
		if (index < 0 || index >= (int)forms_.size())
		{
			#ifndef OSHGUI_DONTUSEEXCEPTIONS
			throw Misc::ArgumentOutOfRangeException("index");
			#endif
		}

		return forms_[index].Form;
	}
	//---------------------------------------------------------------------------
	int FormManager::GetFormCount() const
	{
		return (int)forms_.size();
	}
	//---------------------------------------------------------------------------
	const std::shared_ptr<Form>& FormManager::GetMainForm() const
	{
		return mainForm_;
	}
	//---------------------------------------------------------------------------
	void FormManager::BringToFront(const std::shared_ptr<Form> &form)
	{
		if (form == nullptr)
		{
			#ifndef OSHGUI_DONTUSEEXCEPTIONS
			throw Misc::ArgumentNullException("form");
			#endif
			return;
		}

		if (forms_.empty())
		{
			#ifndef OSHGUI_DONTUSEEXCEPTIONS
			throw Misc::InvalidOperationException("FormList is empty.");
			#endif
			return;
		}

		if (forms_.back().Form == form) //last form in vector is front form
		{
			return;
		}
		
		FormInfo info;
		bool isRegistered = false;
		for (auto it = forms_.begin(); it != forms_.end(); ++it)
		{
			info = *it;
			if (info.Form == form)
			{
				forms_.erase(it);
				isRegistered = true;
				break;
			}
		}

		if (!isRegistered)
		{
			#ifndef OSHGUI_DONTUSEEXCEPTIONS
			throw Misc::InvalidOperationException("'form' is not registered.");
			#endif
			return;
		}
		
		forms_.push_back(info);
	}
	//---------------------------------------------------------------------------
	bool FormManager::IsRegistered(const std::shared_ptr<Form> &form)
	{
		for (auto &info : forms_)
		{
			if (info.Form == form)
			{
				return true;
			}
		}
		
		return false;
	}
	//---------------------------------------------------------------------------
	void FormManager::RegisterMainForm(const std::shared_ptr<Form> &mainForm)
	{
		mainForm_ = mainForm;
		RegisterForm(mainForm_);
	}
	//---------------------------------------------------------------------------
	void FormManager::RegisterForm(const std::shared_ptr<Form> &form)
	{
		RegisterForm(form, nullptr);
	}
	//---------------------------------------------------------------------------
	void FormManager::RegisterForm(const std::shared_ptr<Form> &form, std::function<void()> closeFunction)
	{
		if (form == nullptr)
		{
			#ifndef OSHGUI_DONTUSEEXCEPTIONS
			throw Misc::ArgumentNullException("form");
			#endif
			return;
		}
		
		if (IsRegistered(form))
		{
			return;
		}

		FormInfo info = { 
			form,
			closeFunction,
			false
		};
		forms_.push_back(info);
	}
	//---------------------------------------------------------------------------
	void FormManager::UnregisterForm(const std::shared_ptr<Form> &form)
	{
		if (form == nullptr)
		{
			#ifndef OSHGUI_DONTUSEEXCEPTIONS
			throw Misc::ArgumentNullException("form");
			#endif
			return;
		}

		for (auto &info : forms_)
		{
			if (info.Form == form)
			{
				if (info.CloseFunction)
				{
					info.CloseFunction();
				}
				if (info.Form != mainForm_)
				{
					info.Remove = true;
				}
				else
				{
					Application::Instance().Disable();
				}
				return;
			}
		}
	}
	//---------------------------------------------------------------------------
	void FormManager::RemoveUnregisteredForms()
	{
		for (auto it = forms_.begin(); it != forms_.end();)
		{
			if ((*it).Remove)
			{
				it = forms_.erase(it);
			}
			else
			{
				++it;
			}
		}
	}
	//---------------------------------------------------------------------------
	FormManager::FormIterator FormManager::GetEnumerator()
	{
		return FormIterator(*this);
	}
	//---------------------------------------------------------------------------
	FormManager::FormIterator::FormIterator(FormManager &fm)
		: it_(fm.forms_.rbegin()),
		  end_(fm.forms_.rend())
	{
		
	}
	//---------------------------------------------------------------------------
	void FormManager::FormIterator::operator++()
	{
		++it_;
	}
	//---------------------------------------------------------------------------
	bool FormManager::FormIterator::operator()()
	{
		return it_ != end_;
	}
	//---------------------------------------------------------------------------
	std::shared_ptr<Form>& FormManager::FormIterator::operator*()
	{
		return (*it_).Form;
	}
	//---------------------------------------------------------------------------
}