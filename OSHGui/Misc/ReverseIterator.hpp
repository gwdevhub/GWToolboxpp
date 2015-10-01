#pragma once

#include <iterator>

template<typename T1, typename T2>
struct rpair
{
	rpair(T1 &&first_, T2 &&second_)
		: first(std::move(first_)),
		  second(std::move(second_))
	{

	}

	T1 first;
	T2 second;
};

template<class T>
T begin(rpair<T, T> &p)
{
	return p.first;
}
template<class T>
T begin(const rpair<T, T> &p)
{
	return p.first;
}

template<class T>
T end(rpair<T, T> &p)
{
	return p.second;
}
template<class T>
T end(const rpair<T, T> &p)
{
	return p.second;
}

#if _MSC_VER <= 1800
template<class Iterator>
std::reverse_iterator<Iterator> make_reverse_iterator(Iterator it)
{
	return std::reverse_iterator<Iterator>(it);
}

template<class Range>
rpair<std::reverse_iterator<decltype(begin(std::declval<Range>()))>, std::reverse_iterator<decltype(begin(std::declval<Range>()))>> make_reverse_range(Range&& r)
{
	using type = decltype(make_reverse_iterator(begin(r)));
	return rpair<type, type>(make_reverse_iterator(end(r)), make_reverse_iterator(begin(r)));
}
#else
template<class Range>
auto make_reverse_range(Range&& r)
{
	using type = decltype(make_reverse_iterator(begin(r)));
	return rpair<type, type>(make_reverse_iterator(end(r)), make_reverse_iterator(std::begin(r)));
}
#endif
