#ifndef OSHGUI_MISC_REVERSEITERATOR_HPP
#define OSHGUI_MISC_REVERSEITERATOR_HPP

#include <utility>

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
T begin(rpair<T, T> p)
{
	return p.first;
}
template<class T>
T end(rpair<T, T> p)
{
	return p.second;
}

template<class Iterator>
std::reverse_iterator<Iterator> make_reverse_iterator(Iterator it)
{
	return std::reverse_iterator<Iterator>(it);
}

template<class Range>
rpair<std::reverse_iterator<decltype(begin(std::declval<Range>()))>, std::reverse_iterator<decltype(begin(std::declval<Range>()))>> make_reverse_range(Range&& r)
{
	return rpair<decltype(make_reverse_iterator(end(r))), decltype(make_reverse_iterator(begin(r)))>(make_reverse_iterator(end(r)), make_reverse_iterator(begin(r)));
}

#endif
