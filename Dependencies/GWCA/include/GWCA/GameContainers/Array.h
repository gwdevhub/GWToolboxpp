#pragma once

#include <type_traits>

#include <GWCA/Utilities/Debug.h>
#include <GWCA/Managers/MemoryMgr.h>

namespace GW {

	// Base Array class without m_param
	template <typename T>
	class BaseArray {
	public:
		typedef       T* iterator;
		typedef const T* const_iterator;

		iterator       begin() { return m_buffer; }
		const_iterator begin() const { return m_buffer; }
		iterator       end() { return m_buffer + m_size; }
		const_iterator end()   const { return m_buffer + m_size; }

		BaseArray()
			: m_buffer(nullptr)
			, m_capacity(0)
			, m_size(0)
		{
		}

		BaseArray(const BaseArray&) = delete;
		BaseArray& operator=(const BaseArray&) = delete;

		T& at(size_t index) {
			GWCA_ASSERT(m_buffer && index < m_size);
			return m_buffer[index];
		}

		T* get(size_t index) const {
			return m_buffer && index < m_size ? &m_buffer[index] : nullptr;
		}

		const T& at(size_t index) const {
			GWCA_ASSERT(m_buffer && index < m_size);
			return m_buffer[index];
		}

		T& operator[](uint32_t index) {
			return at(index);
		}

		const T& operator[](uint32_t index) const {
			return at(index);
		}
		bool valid() const { return (!m_buffer || !(reinterpret_cast<uintptr_t>(m_buffer) & 0x3)) && m_size <= m_capacity; }
		void clear() { m_size = 0; }

		uint32_t size()     const { return m_size; }
		uint32_t capacity() const { return m_capacity; }
		// Resize using GW's heap - safe to use with GW-managed arrays
		bool resize(uint32_t new_size) {
			if (new_size > m_capacity) {
				auto* new_buf = static_cast<T*>(GW::MemoryMgr::MemRealloc(m_buffer, new_size * sizeof(T)));
				if (!new_buf) return false;
				m_buffer = new_buf;
				m_capacity = new_size;
			}
			m_size = new_size;
			return true;
		}

	public:
		T* m_buffer;    // +h0000
		uint32_t m_capacity;  // +h0004
		uint32_t m_size;      // +h0008
	}; // Size: 0x000C

	// Extended Array class with m_param
	template <typename T>
	class Array : public BaseArray<T> {
	public:
		Array()
			: BaseArray<T>()
			, m_param(0)
		{
		}

		Array(const Array&) = delete;
		Array& operator=(const Array&) = delete;

		uint32_t m_param;     // +h000C
	}; // Size: 0x0010

	// Use for arrays that are sorted on an integer key
	// Examples are skillbars, questlog, etc. A lot of WorldContext arrays are bsearchable
	// Key is the field that the array is sorted/indexed on.
	// 
	// ex  typedef ArraySorted<Quest,&Quest::quest_id> QuestLog;
	//	   QuestLog& log = GetQuestLogOrWhatever();
	//	   Quest* q = log.find(QUEST_ID_MALLYX_THE_UNYIELDING);
	template <typename T, auto Key>
		requires requires(T* t) { t->*Key; }
	class ArraySorted : public Array<T>
	{
	public:
		using key_type = std::decay_t<decltype(static_cast<T*>(nullptr)->*Key)>;

		// Since the array is ordered by the key we can bsearch off it
		inline T* find(key_type key)
		{
			T* lo = this->begin();
			T* hi = this->end();
			while (lo < hi) {
				T* mid = lo + (hi - lo) / 2;
				if (mid->*Key < key)
					lo = mid + 1;
				else
					hi = mid;
			}
			return (lo != this->end() && lo->*Key == key) ? lo : nullptr;
		}
	};

}
