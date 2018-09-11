#pragma once

#include <catch2/catch.hpp>

#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <set>

//namespace Catch
//{
//	template <typename It>
//	std::string rangeToString(It begin, It end)
//	{
//		using T = typename std::iterator_traits<It>::value_type;
//
//		std::string result = "{";
//
//		if(begin != end)
//		{
//			result += StringMaker<T>::convert(*begin++);
//
//			for(; begin != end; ++begin)
//			{
//				result += ", ";
//				result += StringMaker<T>::convert(*begin);
//			}
//		}
//
//		return result += "}";
//	}
//
//	template <typename T>
//	struct StringMaker<std::vector<T>>
//	{
//		static
//		std::string convert(std::vector<T> const& v)
//		{
//			return rangeToString(v.begin(), v.end());
//		}
//	};
//
//	template <typename K, typename V>
//	class StringMaker<std::map<K, V>>
//	{
//		static
//		void append(std::string& s, std::pair<K, V const> const& p)
//		{
//			s += "(";
//			s += StringMaker<K>::convert(p.first);
//			s += " -> ";
//			s += StringMaker<V>::convert(p.second);
//			s += ")";
//		}
//
//	public:
//		static
//		std::string convert(std::map<K, V> const& m)
//		{
//			std::string result = "{";
//
//			auto it = m.begin();
//
//			if(it != m.end())
//			{
//				append(result, *it);
//
//				for(++it; it != m.end(); ++it)
//				{
//					result += ", ";
//					append(result, *it);
//				}
//			}
//
//			return result += "}";
//		}
//	};
//
//	template <typename T>
//	struct StringMaker<std::set<T>>
//	{
//		static
//		std::string convert(std::set<T> const& s)
//		{
//			return rangeToString(s.begin(), s.end());
//		}
//	};
//}