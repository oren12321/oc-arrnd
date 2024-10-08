#ifndef OC_ARRAY_H
#define OC_ARRAY_H

#include <cstdint>
#include <memory>
#include <initializer_list>
#include <stdexcept>
#include <limits>
#include <algorithm>
#include <numeric>
#include <variant>
#include <sstream>
#include <cmath>
#include <ostream>
#include <cassert>
#include <iterator>
#include <functional>
#include <complex>
#include <tuple>
#include <string>
#include <typeinfo>
#ifndef _MSC_VER
#include <cxxabi.h>
#endif
#include <ranges>
#include <span>
#include <bitset>

// expension of std::complex type overloaded operators
namespace std {
template <typename T>
[[nodiscard]] inline constexpr bool operator<(const std::complex<T>& lhs, const std::complex<T>& rhs)
{
    return std::abs(lhs) < std::abs(rhs);
}
template <typename T>
[[nodiscard]] inline constexpr bool operator<(const std::complex<T>& lhs, const T& rhs)
{
    return std::abs(lhs) < rhs;
}
template <typename T>
[[nodiscard]] inline constexpr bool operator<(const T& lhs, const std::complex<T>& rhs)
{
    return lhs < std::abs(rhs);
}

template <typename T>
[[nodiscard]] inline constexpr bool operator<=(const std::complex<T>& lhs, const std::complex<T>& rhs)
{
    return std::abs(lhs) <= std::abs(rhs);
}
template <typename T>
[[nodiscard]] inline constexpr bool operator<=(const std::complex<T>& lhs, const T& rhs)
{
    return std::abs(lhs) <= rhs;
}
template <typename T>
[[nodiscard]] inline constexpr bool operator<=(const T& lhs, const std::complex<T>& rhs)
{
    return lhs <= std::abs(rhs);
}

template <typename T>
[[nodiscard]] inline constexpr bool operator>(const std::complex<T>& lhs, const std::complex<T>& rhs)
{
    return std::abs(lhs) > std::abs(rhs);
}
template <typename T>
[[nodiscard]] inline constexpr bool operator>(const std::complex<T>& lhs, const T& rhs)
{
    return std::abs(lhs) > rhs;
}
template <typename T>
[[nodiscard]] inline constexpr bool operator>(const T& lhs, const std::complex<T>& rhs)
{
    return lhs > std::abs(rhs);
}

template <typename T>
[[nodiscard]] inline constexpr bool operator>=(const std::complex<T>& lhs, const std::complex<T>& rhs)
{
    return std::abs(lhs) >= std::abs(rhs);
}
template <typename T>
[[nodiscard]] inline constexpr bool operator>=(const std::complex<T>& lhs, const T& rhs)
{
    return std::abs(lhs) >= rhs;
}
template <typename T>
[[nodiscard]] inline constexpr bool operator>=(const T& lhs, const std::complex<T>& rhs)
{
    return lhs >= std::abs(rhs);
}
}

// swap function for zip class iterator usage in std algorithms
// because its operator* not returning reference
namespace std {
template <typename Tuple>
void swap(Tuple&& lhs, Tuple&& rhs) noexcept
{
    lhs.swap(rhs);
}
}

namespace oc::arrnd {
namespace details {
    // reference: http://stackoverflow.com/a/20170989/1593077
    template <typename T, bool AddCvref = false>
    std::string type_name()
    {
        using bare_type = std::remove_cvref_t<T>;

        std::unique_ptr<char, void (*)(void*)> type_ptr(
#ifndef _MSC_VER
            abi::__cxa_demangle(typeid(bare_type).name(), nullptr, nullptr, nullptr),
#else
            nullptr,
#endif
            std::free);

        std::string type_name = (type_ptr ? type_ptr.get() : typeid(bare_type).name());
        if (AddCvref) {
            if (std::is_const_v<bare_type>)
                type_name += " const";
            if (std::is_volatile_v<bare_type>)
                type_name += " volatile";
            if (std::is_lvalue_reference_v<T>)
                type_name += "&";
            else if (std::is_rvalue_reference_v<T>)
                type_name += "&&";
        }
        return type_name;
    }
}
}

namespace oc::arrnd {
namespace details {
    template <typename T, template <typename...> typename TT>
    inline constexpr bool is_template_type_v = std::false_type{};

    template <template <typename...> typename TT, typename... Args>
    inline constexpr bool is_template_type_v<TT<Args...>, TT> = std::true_type{};

    template <typename T, template <typename...> typename TT>
    concept template_type = is_template_type_v<std::remove_cvref_t<T>, TT>;

    template <typename Iter>
    using iterator_value_t = typename std::iterator_traits<Iter>::value_type;

    template <typename Iter>
    concept iterator_type = std::input_or_output_iterator<Iter>;

    template <typename Iter, typename T>
    concept iterator_of_type = iterator_type<Iter> && std::is_same_v<iterator_value_t<Iter>, T>;

    template <typename Iter, template <typename...> typename TT>
    concept iterator_of_template_type = iterator_type<Iter> && template_type<iterator_value_t<Iter>, TT>;

    template <typename Cont>
    concept iterable_type = requires(Cont&& c) {
                                std::begin(c);
                                std::end(c);
                            };

    template <typename Cont>
    using iterable_value_t = std::remove_reference_t<decltype(*std::begin(std::declval<Cont&>()))>;

    template <typename Cont, typename T>
    concept iterable_of_type = iterable_type<Cont> && std::is_same_v<iterable_value_t<Cont>, T>;

    template <typename Cont, template <typename...> typename TT>
    concept iterable_of_template_type = iterable_type<Cont> && template_type<iterable_value_t<Cont>, TT>;

    template <typename T, std::size_t... Ns>
    constexpr std::size_t array_elements_count(std::index_sequence<Ns...>)
    {
        return (1 * ... * std::extent<T, Ns>{});
    }
    template <typename T>
    constexpr std::size_t array_elements_count()
    {
        return array_elements_count<T>(std::make_index_sequence<std::rank<T>{}>());
    }

    template <typename T, typename U>
    T (&array_cast(U& u))
    [array_elements_count<U>()]
    {
        auto ptr = reinterpret_cast<T*>(u);
        T(&res)[array_elements_count<U>()] = *reinterpret_cast<T(*)[array_elements_count<U>()]>(ptr);
        return res;
    }

    template <typename T, typename U>
    const T (&const_array_cast(const U& u))[array_elements_count<U>()]
    {
        auto ptr = reinterpret_cast<const T*>(u);
        const T(&res)[array_elements_count<U>()] = *reinterpret_cast<const T(*)[array_elements_count<U>()]>(ptr);
        return res;
    }

    template <typename T>
    struct iterator_type_of {
        using type = typename T::iterator;
    };

    template <typename T>
    struct iterator_type_of<T*> {
        using type = T*;
    };

    template <typename T, std::size_t N>
    struct iterator_type_of<T[N]> {
        using type = T*;
    };

    template <typename T, std::size_t N>
    struct iterator_type_of<T (&)[N]> {
        using type = T*;
    };

    template <typename T, std::size_t N>
    struct iterator_type_of<const T (&)[N]> {
        using type = T*;
    };

    template <typename T>
    using iterator_type_of_t = iterator_type_of<T>::type;

    template <typename T>
    struct reverse_iterator_type_of {
        using type = typename T::reverse_iterator;
    };

    template <typename T>
    struct reverse_iterator_type_of<T*> {
        using type = std::reverse_iterator<iterator_type_of<T>>;
    };

    template <typename T, std::size_t N>
    struct reverse_iterator_type_of<T[N]> {
        using type = std::reverse_iterator<iterator_type_of<T>>;
    };

    template <typename T, std::size_t N>
    struct reverse_iterator_type_of<T (&)[N]> {
        using type = std::reverse_iterator<iterator_type_of<T>>;
    };

    template <typename T, std::size_t N>
    struct reverse_iterator_type_of<const T (&)[N]> {
        using type = std::reverse_iterator<iterator_type_of<T>>;
    };

    template <typename T>
    using reverse_iterator_type_of_t = reverse_iterator_type_of<T>::type;
}

using details::array_cast;
using details::const_array_cast;
}

// custom implementation of containers/iterators zipping
namespace oc::arrnd {
namespace details {
    template <typename... _Tp>
    bool variadic_or(_Tp&&... args)
    {
        return (... || args);
    }

    template <typename Tuple, std::size_t... I>
    bool any_equals(Tuple&& t1, Tuple&& t2, std::index_sequence<I...>)
    {
        return variadic_or(std::get<I>(std::forward<Tuple>(t1)) == std::get<I>(std::forward<Tuple>(t2))...);
    }

    template <typename... _Tp>
    bool variadic_and(_Tp&&... args)
    {
        return (... && args);
    }

    template <typename Tuple, std::size_t... I>
    bool all_equals(Tuple&& t1, Tuple&& t2, std::index_sequence<I...>)
    {
        return variadic_and(std::get<I>(std::forward<Tuple>(t1)) == std::get<I>(std::forward<Tuple>(t2))...);
    }

    template <typename Tuple, std::size_t... I>
    bool all_lesseq(Tuple&& t1, Tuple&& t2, std::index_sequence<I...>)
    {
        return variadic_and(std::get<I>(std::forward<Tuple>(t1)) <= std::get<I>(std::forward<Tuple>(t2))...);
    }

    template <typename Cont, typename... Args>
    class zipped_container {
    public:
        using container_type = Cont;
        using iterator_type = iterator_type_of_t<Cont>;
        using reverse_iterator_type = reverse_iterator_type_of_t<Cont>;

        constexpr zipped_container() = default;

        constexpr zipped_container(Cont& cont, Args&&... args)
            : cont_(std::addressof(cont))
            , args_(std::forward_as_tuple(std::forward<Args>(args)...))
        { }

        constexpr zipped_container(Cont&& cont, Args&&... args)
            : copy_(std::forward<Cont>(cont))
            , cont_(&copy_)
            , args_(std::forward_as_tuple(std::forward<Args>(args)...))
        { }

        constexpr zipped_container(const zipped_container& other)
            : copy_(other.copy_)
            , cont_(other.cont_ == std::addressof(other.copy_) ? &copy_ : other.cont_)
            , args_(other.args_)
        { }
        constexpr zipped_container(zipped_container&& other)
            : copy_(std::move(other.copy_))
            , cont_(other.cont_ == std::addressof(other.copy_) ? &copy_ : other.cont_)
            , args_(std::move(other.args_))
        {
            other.cont_ = nullptr;
        }

        constexpr zipped_container& operator=(const zipped_container& other)
        {
            if (&other == this) {
                return *this;
            }

            copy_ = other.copy_;
            cont_ = (other.cont_ == std::addressof(other.copy_) ? &copy_ : other.cont_);
            args_ = other.args_;
            return *this;
        }
        constexpr zipped_container& operator=(zipped_container&& other)
        {
            if (&other == this) {
                return *this;
            }

            copy_ = std::move(other.copy_);
            cont_ = (other.cont_ == std::addressof(other.copy_) ? &copy_ : other.cont_);
            args_ = std::move(other.args_);
            other.cont_ = nullptr;
            return *this;
        }

        constexpr virtual ~zipped_container() = default;

        constexpr auto& cont() noexcept
        {
            return *cont_;
        }
        constexpr const auto& cont() const noexcept
        {
            return *cont_;
        }

        constexpr auto& args() noexcept
        {
            return args_;
        }
        constexpr const auto& args() const
        {
            return args_;
        }

    private:
        Cont copy_;
        Cont* cont_ = nullptr;
        std::tuple<Args...> args_ = std::tuple<>{};
    };

    template <typename Cont, typename... Args>
    class zipped_raw_array {
    public:
        using container_type = Cont;
        using iterator_type = iterator_type_of_t<container_type>;
        using reverse_iterator_type = reverse_iterator_type_of_t<container_type>;

        constexpr zipped_raw_array() = default;

        constexpr zipped_raw_array(Cont& cont, Args&&... args)
            : cont_(std::addressof(cont))
            , args_(std::forward_as_tuple(std::forward<Args>(args)...))
        { }

        constexpr zipped_raw_array(const zipped_raw_array& other) = default;
        constexpr zipped_raw_array(zipped_raw_array&& other) = default;

        constexpr zipped_raw_array& operator=(const zipped_raw_array& other) = default;
        constexpr zipped_raw_array& operator=(zipped_raw_array&& other) = default;

        constexpr virtual ~zipped_raw_array() = default;

        constexpr auto& cont() noexcept
        {
            return *cont_;
        }
        constexpr const auto& cont() const noexcept
        {
            return *cont_;
        }

        constexpr auto& args() noexcept
        {
            return args_;
        }
        constexpr const auto& args() const
        {
            return args_;
        }

    private:
        Cont* cont_ = nullptr;
        std::tuple<Args...> args_ = std::tuple<>{};
    };

    template <iterator_type InputIt>
    class zipped_iterator {
    public:
        struct unknown { };
        using container_type = unknown;
        using iterator_type = InputIt;
        using reverse_iterator_type = InputIt;

        constexpr zipped_iterator() = default;

        constexpr zipped_iterator(InputIt first, InputIt last)
            : first_(first)
            , last_(last)
        { }

        constexpr zipped_iterator(const zipped_iterator&) = default;
        constexpr zipped_iterator(zipped_iterator&&) = default;

        constexpr zipped_iterator& operator=(const zipped_iterator&) = default;
        constexpr zipped_iterator& operator=(zipped_iterator&&) = default;

        constexpr virtual ~zipped_iterator() = default;

        constexpr auto& first() noexcept
        {
            return first_;
        }
        constexpr const auto& first() const noexcept
        {
            return first_;
        }

        constexpr auto& last() noexcept
        {
            return last_;
        }
        constexpr const auto& last() const
        {
            return last_;
        }

        constexpr auto& args() noexcept
        {
            return args_;
        }
        constexpr const auto& args() const
        {
            return args_;
        }

    private:
        InputIt first_;
        InputIt last_;
        std::tuple<> args_ = std::tuple<>{};
    };

    template <iterable_type Cont, typename... Args>
    [[nodiscard]] inline constexpr auto zipped(Cont&& cont, Args&&... args)
    {
        if constexpr (std::is_array_v<std::remove_cvref_t<Cont>>) {
            return zipped_raw_array<std::remove_reference_t<Cont>, Args...>(
                std::forward<Cont>(cont), std::forward<Args>(args)...);
        } else {
            if constexpr (std::is_lvalue_reference_v<Cont>) {
                std::add_lvalue_reference_t<std::remove_const_t<std::remove_reference_t<Cont>>> nc_cont
                    = const_cast<std::add_lvalue_reference_t<std::remove_const_t<std::remove_reference_t<Cont>>>>(cont);
                return zipped_container<std::remove_reference_t<decltype(nc_cont)>, Args...>(
                    std::forward<decltype(nc_cont)>(nc_cont), std::forward<Args>(args)...);
            } else {
                return zipped_container<std::remove_reference_t<Cont>, Args...>(
                    std::forward<Cont>(cont), std::forward<Args>(args)...);
            }
        }
    }

    template <iterator_type InputIt>
    [[nodiscard]] inline constexpr auto zipped(InputIt first, InputIt last)
    {
        return zipped_iterator<InputIt>(first, last);
    }

    template <typename... ItPack>
    class zip {
    public:
        struct iterator {
            using iterator_category = std::random_access_iterator_tag;
            using value_type = std::tuple<std::iter_value_t<typename ItPack::iterator_type>...>;
            using reference = std::tuple<std::iter_reference_t<typename ItPack::iterator_type>...>;
            using difference_type = std::int64_t;

            [[nodiscard]] constexpr reference operator*() const
            {
                return std::apply(
                    []<typename... Ts>(Ts&&... e) {
                        return reference(*std::forward<Ts>(e)...);
                    },
                    data_);
            }

            constexpr iterator& operator++()
            {
                std::apply(
                    [&]<typename... Ts>(Ts&&... e) {
                        data_ = std::make_tuple(++std::forward<Ts>(e)...);
                    },
                    data_);
                return *this;
            }

            constexpr iterator operator++(int)
            {
                iterator temp{*this};
                ++(*this);
                return temp;
            }

            constexpr iterator& operator+=(std::int64_t count)
            {
                std::apply(
                    [&]<typename... Ts>(Ts&&... e) {
                        data_ = std::make_tuple((std::forward<Ts>(e) += count)...);
                    },
                    data_);
                return *this;
            }

            [[nodiscard]] constexpr iterator operator+(std::int64_t count) const
            {
                iterator temp{*this};
                temp += count;
                return temp;
            }

            constexpr iterator& operator--()
            {
                std::apply(
                    [&]<typename... Ts>(Ts&&... e) {
                        data_ = std::make_tuple(--std::forward<Ts>(e)...);
                    },
                    data_);
                return *this;
            }

            constexpr iterator operator--(int)
            {
                iterator temp{*this};
                --(*this);
                return temp;
            }

            constexpr iterator& operator-=(std::int64_t count)
            {
                std::apply(
                    [&]<typename... Ts>(Ts&&... e) {
                        data_ = std::make_tuple((std::forward<Ts>(e) -= count)...);
                    },
                    data_);
                return *this;
            }

            [[nodiscard]] constexpr iterator operator-(std::int64_t count) const
            {
                iterator temp{*this};
                temp -= count;
                return temp;
            }

            [[nodiscard]] constexpr auto operator!=(const iterator& iter) const
            {
                return !any_equals(data_, iter.data_, std::index_sequence_for<typename ItPack::container_type...>{});
            }

            [[nodiscard]] constexpr auto operator==(const iterator& iter) const
            {
                return all_equals(data_, iter.data_, std::index_sequence_for<typename ItPack::container_type...>{});
            }

            [[nodiscard]] constexpr reference operator[](std::int64_t index) const
            {
                return std::apply(
                    [index]<typename... Ts>(Ts&&... e) {
                        return reference(std::forward<Ts>(e)[index]...);
                    },
                    data_);
            }

            [[nodiscard]] constexpr bool operator<(const iterator& iter) const noexcept
            {
                return all_lesseq(data_, iter.data_, std::index_sequence_for<typename ItPack::container_type...>{});
            }

            [[nodiscard]] constexpr std::int64_t operator-(const iterator& iter) const noexcept
            {
                auto impl
                    = []<typename T1, typename T2, std::size_t... I>(T1&& t1, T2&& t2, std::index_sequence<I...>) {
                          return std::max({
                              static_cast<difference_type>(
                                  (std::get<I>(std::forward<T1>(t1)) - std::get<I>(std::forward<T2>(t2))))...,
                          });
                      };

                auto diffs = impl(data_, iter.data_, std::index_sequence_for<typename ItPack::container_type...>{});
                return diffs;
            }

            std::tuple<typename ItPack::iterator_type...> data_;
        };

        struct reverse_iterator {
            using iterator_category = std::random_access_iterator_tag;
            using value_type = std::tuple<std::iter_value_t<typename ItPack::reverse_iterator_type>...>;
            using reference = std::tuple<std::iter_reference_t<typename ItPack::reverse_iterator_type>...>;
            using difference_type = std::int64_t;

            [[nodiscard]] constexpr reference operator*() const
            {
                return std::apply(
                    []<typename... Ts>(Ts&&... e) {
                        return reference(*std::forward<Ts>(e)...);
                    },
                    data_);
            }

            constexpr reverse_iterator& operator++()
            {
                std::apply(
                    [&]<typename... Ts>(Ts&&... e) {
                        data_ = std::make_tuple(++std::forward<Ts>(e)...);
                    },
                    data_);
                return *this;
            }

            constexpr reverse_iterator operator++(int)
            {
                reverse_iterator temp{*this};
                ++(*this);
                return temp;
            }

            constexpr reverse_iterator& operator+=(std::int64_t count)
            {
                std::apply(
                    [&]<typename... Ts>(Ts&&... e) {
                        data_ = std::make_tuple((std::forward<Ts>(e) += count)...);
                    },
                    data_);
                return *this;
            }

            [[nodiscard]] constexpr reverse_iterator operator+(std::int64_t count) const
            {
                reverse_iterator temp{*this};
                temp += count;
                return temp;
            }

            constexpr reverse_iterator& operator--()
            {
                std::apply(
                    [&]<typename... Ts>(Ts&&... e) {
                        data_ = std::make_tuple(--std::forward<Ts>(e)...);
                    },
                    data_);
                return *this;
            }

            constexpr reverse_iterator operator--(int)
            {
                reverse_iterator temp{*this};
                --(*this);
                return temp;
            }

            constexpr reverse_iterator& operator-=(std::int64_t count)
            {
                std::apply(
                    [&]<typename... Ts>(Ts&&... e) {
                        data_ = std::make_tuple((std::forward<Ts>(e) -= count)...);
                    },
                    data_);
                return *this;
            }

            [[nodiscard]] constexpr reverse_iterator operator-(std::int64_t count) const
            {
                reverse_iterator temp{*this};
                temp -= count;
                return temp;
            }

            [[nodiscard]] constexpr auto operator!=(const reverse_iterator& iter) const
            {
                return !any_equals(data_, iter.data_, std::index_sequence_for<typename ItPack::container_type...>{});
            }

            [[nodiscard]] constexpr auto operator==(const reverse_iterator& iter) const
            {
                return all_equals(data_, iter.data_, std::index_sequence_for<typename ItPack::container_type...>{});
            }

            [[nodiscard]] constexpr reference operator[](std::int64_t index) const
            {
                return std::apply(
                    [index]<typename... Ts>(Ts&&... e) {
                        return reference(std::forward<Ts>(e)[index]...);
                    },
                    data_);
            }

            [[nodiscard]] constexpr bool operator<(const reverse_iterator& iter) const noexcept
            {
                return all_lesseq(data_, iter.data_, std::index_sequence_for<typename ItPack::container_type...>{});
            }

            [[nodiscard]] constexpr std::int64_t operator-(const reverse_iterator& iter) const noexcept
            {
                auto impl
                    = []<typename T1, typename T2, std::size_t... I>(T1&& t1, T2&& t2, std::index_sequence<I...>) {
                          return std::max({
                              (std::get<I>(std::forward<T1>(t1)) - std::get<I>(std::forward<T2>(t2)))...,
                          });
                      };

                auto diffs = impl(data_, iter.data_, std::index_sequence_for<typename ItPack::container_type...>{});
                return diffs;
            }

            std::tuple<typename ItPack::reverse_iterator_type...> data_;
        };

        using value_type = std::tuple<std::iter_value_t<typename ItPack::iterator_type>&...>;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using reference = value_type&;
        using const_reference = const value_type&;
        using pointer = value_type*;
        using const_pointer = const value_type*;
        using const_iterator = iterator;
        using const_reverse_iterator = reverse_iterator;

        zip() = default;

        zip(ItPack... packs)
            : packs_(std::forward_as_tuple(packs...))
        { }

        auto begin()
        {
            auto impl = []<typename P, std::size_t... I>(P&& ip, std::index_sequence<I...>) {
                if constexpr (template_type<P, zipped_container> || template_type<P, zipped_raw_array>) {
                    using std::begin;
                    return begin(ip.cont(), std::get<I>(ip.args())...);
                } else {
                    return ip.first();
                }
            };

            return iterator(std::apply(
                [&]<typename... Ts>(Ts&&... e) {
                    return std::make_tuple(impl(std::forward<Ts>(e),
                        std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<decltype(e.args())>>>{})...);
                },
                packs_));
        }

        auto begin() const
        {
            auto impl = []<typename P, std::size_t... I>(P&& ip, std::index_sequence<I...>) {
                if constexpr (template_type<P, zipped_container> || template_type<P, zipped_raw_array>) {
                    using std::begin;
                    return begin(const_cast<typename std::remove_cvref_t<P>::container_type&>(ip.cont()),
                        std::get<I>(ip.args())...);
                } else {
                    return ip.first();
                }
            };

            return iterator(std::apply(
                [&]<typename... Ts>(Ts&&... e) {
                    return std::make_tuple(impl(std::forward<Ts>(e),
                        std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<decltype(e.args())>>>{})...);
                },
                packs_));
        }

        auto end()
        {
            auto impl = []<typename P, std::size_t... I>(P&& ip, std::index_sequence<I...>) {
                if constexpr (template_type<P, zipped_container> || template_type<P, zipped_raw_array>) {
                    using std::end;
                    return end(ip.cont(), std::get<I>(ip.args())...);
                } else {
                    return ip.last();
                }
            };

            return iterator{std::apply(
                [&]<typename... Ts>(Ts&&... e) {
                    return std::make_tuple(impl(std::forward<Ts>(e),
                        std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<decltype(e.args())>>>{})...);
                },
                packs_)};
        }

        auto end() const
        {
            auto impl = []<typename P, std::size_t... I>(P&& ip, std::index_sequence<I...>) {
                if constexpr (template_type<P, zipped_container> || template_type<P, zipped_raw_array>) {
                    using std::end;
                    return end(const_cast<typename std::remove_cvref_t<P>::container_type&>(ip.cont()),
                        std::get<I>(ip.args())...);
                } else {
                    return ip.last();
                }
            };

            return iterator{std::apply(
                [&]<typename... Ts>(Ts&&... e) {
                    return std::make_tuple(impl(std::forward<Ts>(e),
                        std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<decltype(e.args())>>>{})...);
                },
                packs_)};
        }

        auto rbegin()
        {
            auto impl = []<typename P, std::size_t... I>(P&& ip, std::index_sequence<I...>) {
                if constexpr (template_type<P, zipped_container> || template_type<P, zipped_raw_array>) {
                    using std::rbegin;
                    return rbegin(ip.cont(), std::get<I>(ip.args())...);
                } else {
                    return ip.first();
                }
            };

            return reverse_iterator(std::apply(
                [&]<typename... Ts>(Ts&&... e) {
                    return std::make_tuple(impl(std::forward<Ts>(e),
                        std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<decltype(e.args())>>>{})...);
                },
                packs_));
        }

        auto rbegin() const
        {
            auto impl = []<typename P, std::size_t... I>(P&& ip, std::index_sequence<I...>) {
                if constexpr (template_type<P, zipped_container> || template_type<P, zipped_raw_array>) {
                    using std::rbegin;
                    return rbegin(const_cast<typename std::remove_cvref_t<P>::container_type&>(ip.cont()),
                        std::get<I>(ip.args())...);
                } else {
                    return ip.first();
                }
            };

            return reverse_iterator(std::apply(
                [&]<typename... Ts>(Ts&&... e) {
                    return std::make_tuple(impl(std::forward<Ts>(e),
                        std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<decltype(e.args())>>>{})...);
                },
                packs_));
        }

        auto rend()
        {
            auto impl = []<typename P, std::size_t... I>(P&& ip, std::index_sequence<I...>) {
                if constexpr (template_type<P, zipped_container> || template_type<P, zipped_raw_array>) {
                    using std::rend;
                    return rend(ip.cont(), std::get<I>(ip.args())...);
                } else {
                    return ip.last();
                }
            };

            return reverse_iterator{std::apply(
                [&]<typename... Ts>(Ts&&... e) {
                    return std::make_tuple(impl(std::forward<Ts>(e),
                        std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<decltype(e.args())>>>{})...);
                },
                packs_)};
        }

        auto rend() const
        {
            auto impl = []<typename P, std::size_t... I>(P&& ip, std::index_sequence<I...>) {
                if constexpr (template_type<P, zipped_container> || template_type<P, zipped_raw_array>) {
                    using std::rend;
                    return rend(const_cast<typename std::remove_cvref_t<P>::container_type&>(ip.cont()),
                        std::get<I>(ip.args())...);
                } else {
                    return ip.last();
                }
            };

            return reverse_iterator{std::apply(
                [&]<typename... Ts>(Ts&&... e) {
                    return std::make_tuple(impl(std::forward<Ts>(e),
                        std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<decltype(e.args())>>>{})...);
                },
                packs_)};
        }

    private:
        std::tuple<ItPack...> packs_;
    };
}

using details::zipped;
using details::zip;
}

namespace oc::arrnd {
namespace details {
    template <typename T>
    struct simple_allocator {
        using value_type = T;

        static constexpr std::size_t max_size = std::size_t(-1) / sizeof(T);

        simple_allocator() = default;

        template <typename U>
        constexpr simple_allocator(const simple_allocator<U>&) noexcept
        { }

        [[nodiscard]] constexpr T* allocate(std::size_t n)
        {
            if (n > max_size) {
                throw std::bad_alloc{};
            }
            return static_cast<T*>(::operator new[](n * sizeof(value_type)));
        }

        constexpr void deallocate(T* p, std::size_t n) noexcept
        {
            assert("pre " && p != nullptr);
            ::operator delete[](p, n * sizeof(value_type));
        }
    };

    template <typename T, typename U>
    [[nodiscard]] constexpr bool operator==(const simple_allocator<T>&, const simple_allocator<U>&)
    {
        return sizeof(T) == sizeof(U);
    }

    template <typename T, typename U>
    [[nodiscard]] constexpr bool operator!=(const simple_allocator<T>& lhs, const simple_allocator<U>& rhs)
    {
        return !(lhs == rhs);
    }

}

using details::simple_allocator;
}

namespace oc::arrnd {
namespace details {
    template <typename T, typename Allocator = simple_allocator<T>>
    class simple_vector {
        static_assert(std::is_same_v<T, typename Allocator::value_type>);

    public:
        using value_type = T;
        using allocator_type = Allocator;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using reference = T&;
        using const_reference = const T&;
        using pointer = T*;
        using const_pointer = const T*;
        using iterator = T*;
        using const_iterator = const T*;
        using reverse_iterator = std::reverse_iterator<pointer>;
        using const_reverse_iterator = std::reverse_iterator<const_pointer>;

        simple_vector() = default;

        explicit constexpr simple_vector(size_type size)
            : size_(size)
            , capacity_(size)
        {
            if (size > 0) {
                ptr_ = alloc_.allocate(size);
                if constexpr (!std::is_fundamental_v<value_type>) {
                    std::uninitialized_default_construct_n(ptr_, size);
                }
            }
        }

        template <typename U>
        explicit constexpr simple_vector(size_type size, const U& value)
            : size_(size)
            , capacity_(size)
        {
            if (size > 0) {
                ptr_ = alloc_.allocate(size);
                std::uninitialized_fill_n(ptr_, size, value);
            }
        }

        template <iterator_type InputIt>
        explicit constexpr simple_vector(InputIt first, InputIt last)
            : size_(std::distance(first, last))
            , capacity_(std::distance(first, last))
        {
            difference_type dist = std::distance(first, last);
            if (dist < 0) {
                throw std::invalid_argument("negative distance between iterators");
            }
            if (dist > 0) {
                ptr_ = alloc_.allocate(dist);
                std::uninitialized_copy_n(first, dist, ptr_);
            }
        }

        template <iterable_type Cont>
        explicit constexpr simple_vector(const Cont& values)
            : simple_vector(std::begin(values), std::end(values))
        { }

        explicit constexpr simple_vector(std::initializer_list<value_type> values)
            : simple_vector(values.begin(), values.end())
        { }

        constexpr void clear()
        {
            if (!empty()) {
                if constexpr (!std::is_fundamental_v<value_type>) {
                    std::destroy_n(ptr_, size_);
                }
            }
            size_ = 0;
        }

        constexpr simple_vector(const simple_vector& other)
            : alloc_(other.alloc_)
            , size_(other.size_)
            , capacity_(other.capacity_)
        {
            if (other.capacity_ > 0) {
                ptr_ = alloc_.allocate(other.capacity_);
                std::uninitialized_copy_n(other.ptr_, other.size_, ptr_);
            }
        }

        constexpr simple_vector& operator=(const simple_vector& other)
        {
            if (this == &other) {
                return *this;
            }

            if (capacity_ > 0) {
                clear();
                alloc_.deallocate(ptr_, capacity_);
                capacity_ = 0;
            }

            alloc_ = other.alloc_;
            size_ = other.size_;
            capacity_ = other.capacity_;

            if (other.capacity_ > 0) {
                ptr_ = alloc_.allocate(other.capacity_);
                std::uninitialized_copy_n(other.ptr_, other.size_, ptr_);
            }

            return *this;
        }

        constexpr simple_vector(simple_vector&& other) noexcept
            : alloc_(std::move(other.alloc_))
            , size_(other.size_)
            , capacity_(other.capacity_)
        {
            ptr_ = other.ptr_;

            other.ptr_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
        }

        constexpr simple_vector& operator=(simple_vector&& other) noexcept
        {
            if (this == &other) {
                return *this;
            }

            if (capacity_ > 0) {
                clear();
                alloc_.deallocate(ptr_, capacity_);
                capacity_ = 0;
            }

            alloc_ = std::move(other.alloc_);
            size_ = other.size_;
            capacity_ = other.capacity_;
            ptr_ = other.ptr_;

            other.ptr_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;

            return *this;
        }

        constexpr ~simple_vector() noexcept
        {
            if (size_ > 0) {
                clear();
            }
            if (capacity_ > 0) {
                alloc_.deallocate(ptr_, capacity_);
            }
        }

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return size_ == 0;
        }

        [[nodiscard]] constexpr size_type size() const noexcept
        {
            return size_;
        }

        [[nodiscard]] constexpr size_type capacity() const noexcept
        {
            return capacity_;
        }

        [[nodiscard]] constexpr pointer data() const noexcept
        {
            return ptr_;
        }

        [[nodiscard]] constexpr reference operator[](size_type index) noexcept
        {
            assert(index < size_);
            return ptr_[index];
        }

        [[nodiscard]] constexpr const_reference operator[](size_type index) const noexcept
        {
            assert(index < size_);
            return ptr_[index];
        }

        constexpr void resize(size_type count)
        {
            if (count == 0) {
                clear();
            } else if (count < size_) {
                if constexpr (!std::is_fundamental_v<value_type>) {
                    std::destroy_n(ptr_ + count, size_ - count);
                }
                size_ = count;
            } else if (count > size_) {
                if (count <= capacity_) {
                    if constexpr (!std::is_fundamental_v<value_type>) {
                        std::uninitialized_default_construct_n(ptr_ + size_, count - size_);
                    }
                    size_ = count;
                } else {
                    pointer new_ptr = alloc_.allocate(count);
                    std::uninitialized_move_n(ptr_, size_, new_ptr);
                    if constexpr (!std::is_fundamental_v<value_type>) {
                        std::uninitialized_default_construct_n(new_ptr + size_, count - size_);
                    }

                    if (capacity_ > 0) {
                        alloc_.deallocate(ptr_, capacity_);
                    }

                    size_ = count;
                    capacity_ = count;
                    ptr_ = new_ptr;
                }
            }
        }

        constexpr void reserve(size_type new_cap)
        {
            if (new_cap > capacity_) {
                pointer new_ptr = alloc_.allocate(new_cap);
                std::uninitialized_move_n(ptr_, size_, new_ptr);

                if (capacity_ > 0) {
                    alloc_.deallocate(ptr_, capacity_);
                }
                ptr_ = new_ptr;
                capacity_ = new_cap;
            }
        }

        constexpr void append(size_type count)
        {
            if (size_ + count <= capacity_ && size_ + count > size_) {
                if constexpr (!std::is_fundamental_v<value_type>) {
                    std::uninitialized_default_construct_n(ptr_ + size_, count);
                }
                size_ += count;
            } else if (size_ + count > capacity_) {
                size_type new_cap = static_cast<size_type>(1.5 * (size_ + count));
                pointer new_ptr = alloc_.allocate(new_cap);
                std::uninitialized_move_n(ptr_, size_, new_ptr);
                if constexpr (!std::is_fundamental_v<value_type>) {
                    std::uninitialized_default_construct_n(new_ptr + size_, count);
                }
                if (capacity_ > 0) {
                    alloc_.deallocate(ptr_, capacity_);
                }

                ptr_ = new_ptr;
                capacity_ = new_cap;
                size_ += count;
            }
        }

        constexpr void shrink_to_fit()
        {
            if (capacity_ > size_) {
                pointer new_ptr = alloc_.allocate(size_);
                std::uninitialized_move_n(ptr_, size_, new_ptr);

                alloc_.deallocate(ptr_, capacity_);
                ptr_ = new_ptr;
                capacity_ = size_;
            }
        }

        [[nodiscard]] constexpr pointer begin() noexcept
        {
            return ptr_;
        }

        [[nodiscard]] constexpr pointer end() noexcept
        {
            return ptr_ + size_;
        }

        [[nodiscard]] constexpr const_pointer begin() const noexcept
        {
            return ptr_;
        }

        [[nodiscard]] constexpr const_pointer end() const noexcept
        {
            return ptr_ + size_;
        }

        [[nodiscard]] constexpr const_pointer cbegin() const noexcept
        {
            return ptr_;
        }

        [[nodiscard]] constexpr const_pointer cend() const noexcept
        {
            return ptr_ + size_;
        }

        [[nodiscard]] constexpr std::reverse_iterator<pointer> rbegin() noexcept
        {
            return std::make_reverse_iterator(end());
        }

        [[nodiscard]] constexpr std::reverse_iterator<pointer> rend() noexcept
        {
            return std::make_reverse_iterator(begin());
        }

        [[nodiscard]] constexpr std::reverse_iterator<const_pointer> crbegin() const noexcept
        {
            return std::make_reverse_iterator(cend());
        }

        [[nodiscard]] constexpr std::reverse_iterator<const_pointer> crend() const noexcept
        {
            return std::make_reverse_iterator(cbegin());
        }

        [[nodiscard]] constexpr const_reference back() const noexcept
        {
            return ptr_[size_ - 1];
        }

        [[nodiscard]] constexpr reference back() noexcept
        {
            return ptr_[size_ - 1];
        }

        [[nodiscard]] constexpr const_reference front() const noexcept
        {
            return ptr_[0];
        }

        [[nodiscard]] constexpr reference front() noexcept
        {
            return ptr_[0];
        }

        template <iterator_type InputIt>
        constexpr iterator insert(const_iterator pos, InputIt first, InputIt last)
        {
            difference_type in_dist = std::distance(first, last);
            if (in_dist < 0) {
                throw std::invalid_argument("negative distance between iterators");
            }

            difference_type pos_dist = std::distance(cbegin(), pos);
            if (pos_dist < 0 || pos_dist > size_) {
                throw std::invalid_argument("unbound input pos");
            }

            if (in_dist == 0) {
                return ptr_ + pos_dist;
            }

            append(in_dist);

            auto new_pos = begin() + pos_dist;

            auto rpos_start = rbegin() + in_dist;
            auto rpos_stop = rend() - pos_dist;
            std::move(rpos_start, rpos_stop, rpos_start - in_dist);

            std::copy(first, last, new_pos);

            return new_pos;
        }

        template <typename U>
        constexpr iterator insert(const_iterator pos, size_type count, const U& value)
        {
            difference_type pos_dist = std::distance(cbegin(), pos);
            if (pos_dist < 0 || pos_dist > size_) {
                throw std::invalid_argument("unbound input pos");
            }

            if (count == 0) {
                return ptr_ + pos_dist;
            }

            append(count);

            auto new_pos = begin() + pos_dist;

            auto rpos_start = rbegin() + count;
            auto rpos_stop = rend() - pos_dist;
            std::move(rpos_start, rpos_stop, rpos_start - count);

            std::fill_n(new_pos, count, value);

            return new_pos;
        }

        constexpr iterator erase(const_iterator first, const_iterator last)
        {
            difference_type dist = std::distance(first, last);
            if (dist < 0) {
                throw std::invalid_argument("negative distance between iterators");
            }

            difference_type first_dist = std::distance(cbegin(), first);
            if (first_dist < 0 || first_dist > size_) {
                throw std::invalid_argument("unbound input first");
            }

            difference_type last_dist = std::distance(cbegin(), last);
            if (last_dist < 0 || last_dist > size_) {
                throw std::invalid_argument("unbound input last");
            }

            if (dist == 0) {
                return ptr_ + last_dist - 1;
            }

            std::move(last, cend(), ptr_ + first_dist);

            resize(size_ - dist);

            return ptr_ + last_dist - 1;
        }

        constexpr iterator erase(const_iterator pos)
        {
            return erase(pos, pos + 1);
        }

    private:
        size_type size_ = 0;
        size_type capacity_ = 0;
        pointer ptr_ = nullptr;
        allocator_type alloc_;
    };

    template <typename T, std::size_t Capacity>
    class simple_array {
        static_assert(Capacity > 0);

    public:
        using value_type = T;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using reference = T&;
        using const_reference = const T&;
        using pointer = T*;
        using const_pointer = const T*;
        using iterator = T*;
        using const_iterator = const T*;
        using reverse_iterator = std::reverse_iterator<pointer>;
        using const_reverse_iterator = std::reverse_iterator<const_pointer>;

        simple_array() = default;

        explicit constexpr simple_array(size_type size)
            : size_(size)
        {
            if (size > Capacity) {
                throw std::invalid_argument("size bigger than maximum capacity");
            }
        }

        template <typename U>
        explicit constexpr simple_array(size_type size, const U& value)
            : simple_array(size)
        {
            std::fill_n(ptr_, size, value);
        }

        template <iterator_type InputIt>
        explicit constexpr simple_array(InputIt first, InputIt last)
            : simple_array(std::distance(first, last))
        {
            difference_type dist = std::distance(first, last);
            if (dist < 0) {
                throw std::invalid_argument("negative distance between iterators");
            }
            if (dist > 0) {
                std::copy_n(first, dist, ptr_);
            }
        }

        template <iterable_type Cont>
        explicit constexpr simple_array(const Cont& values)
            : simple_array(std::begin(values), std::end(values))
        { }

        explicit constexpr simple_array(std::initializer_list<value_type> values)
            : simple_array(values.begin(), values.end())
        { }

        constexpr void clear()
        {
            if (!empty()) {
                if constexpr (!std::is_fundamental_v<value_type>) {
                    std::destroy_n(ptr_, size_);
                }
            }
            size_ = 0;
        }

        constexpr simple_array(const simple_array& other)
            : simple_array(other.ptr_, other.ptr_ + other.size_)
        { }

        constexpr simple_array& operator=(const simple_array& other)
        {
            if (this == &other) {
                return *this;
            }

            if (other.empty()) {
                clear();
                return *this;
            }

            std::copy(other.ptr_, other.ptr_ + other.size_, ptr_);
            if constexpr (!std::is_fundamental_v<value_type>) {
                if (other.size_ < size_) {
                    std::destroy_n(ptr_ + other.size_, size_ - other.size_);
                }
            }
            size_ = other.size_;

            return *this;
        }

        constexpr simple_array(simple_array&& other) noexcept
            : size_(other.size_)
        {
            std::move(other.ptr_, other.ptr_ + other.size_, ptr_);

            other.size_ = 0;
        }

        constexpr simple_array& operator=(simple_array&& other) noexcept
        {
            if (this == &other) {
                return *this;
            }

            if (other.empty()) {
                clear();
                return *this;
            }

            std::move(other.ptr_, other.ptr_ + other.size_, ptr_);
            if constexpr (!std::is_fundamental_v<value_type>) {
                if (other.size_ < size_) {
                    std::destroy_n(ptr_ + other.size_, size_ - other.size_);
                }
            }
            size_ = other.size_;

            other.size_ = 0;

            return *this;
        }

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return size_ == 0;
        }

        [[nodiscard]] constexpr size_type size() const noexcept
        {
            return size_;
        }

        [[nodiscard]] constexpr size_type capacity() const noexcept
        {
            return Capacity;
        }

        [[nodiscard]] constexpr pointer data() const noexcept
        {
            return const_cast<pointer>(ptr_);
        }

        [[nodiscard]] constexpr reference operator[](size_type index) noexcept
        {
            assert(index < size_);
            return ptr_[index];
        }

        [[nodiscard]] constexpr const_reference operator[](size_type index) const noexcept
        {
            assert(index < size_);
            return ptr_[index];
        }

        constexpr void resize(size_type count)
        {
            if (count > Capacity) {
                throw std::invalid_argument("resize count > Capacity");
            }

            if (count < size_) {
                if constexpr (!std::is_fundamental_v<value_type>) {
                    std::destroy_n(ptr_ + count, size_ - count);
                }
                size_ = count;
            } else if (count > size_) {
                size_ = count;
            }
        }

        constexpr void reserve(size_type new_cap)
        {
            if (new_cap != Capacity) {
                throw std::invalid_argument("new capacity different from fixed Capacity");
            }
        }

        constexpr void append(size_type count)
        {
            if (size_ + count > Capacity) {
                throw std::invalid_argument("append size is bigger than fixed Capacity");
            }
            size_ += count;
        }

        constexpr void shrink_to_fit()
        {
            // noop since Capacity is fixed value
        }

        [[nodiscard]] constexpr pointer begin() noexcept
        {
            return ptr_;
        }

        [[nodiscard]] constexpr pointer end() noexcept
        {
            return ptr_ + size_;
        }

        [[nodiscard]] constexpr const_pointer begin() const noexcept
        {
            return ptr_;
        }

        [[nodiscard]] constexpr const_pointer end() const noexcept
        {
            return ptr_ + size_;
        }

        [[nodiscard]] constexpr const_pointer cbegin() const noexcept
        {
            return ptr_;
        }

        [[nodiscard]] constexpr const_pointer cend() const noexcept
        {
            return ptr_ + size_;
        }

        [[nodiscard]] constexpr std::reverse_iterator<pointer> rbegin() noexcept
        {
            return std::make_reverse_iterator(end());
        }

        [[nodiscard]] constexpr std::reverse_iterator<pointer> rend() noexcept
        {
            return std::make_reverse_iterator(begin());
        }

        [[nodiscard]] constexpr std::reverse_iterator<const_pointer> crbegin() const noexcept
        {
            return std::make_reverse_iterator(cend());
        }

        [[nodiscard]] constexpr std::reverse_iterator<const_pointer> crend() const noexcept
        {
            return std::make_reverse_iterator(cbegin());
        }

        [[nodiscard]] constexpr const_reference back() const noexcept
        {
            return ptr_[size_ - 1];
        }

        [[nodiscard]] constexpr reference back() noexcept
        {
            return ptr_[size_ - 1];
        }

        [[nodiscard]] constexpr const_reference front() const noexcept
        {
            return ptr_[0];
        }

        [[nodiscard]] constexpr reference front() noexcept
        {
            return ptr_[0];
        }

        template <iterator_type InputIt>
        constexpr iterator insert(const_iterator pos, InputIt first, InputIt last)
        {
            difference_type in_dist = std::distance(first, last);
            if (in_dist < 0) {
                throw std::invalid_argument("negative distance between iterators");
            }

            difference_type pos_dist = std::distance(cbegin(), pos);
            if (pos_dist < 0 || pos_dist > size_) {
                throw std::invalid_argument("unbound input pos");
            }

            if (in_dist == 0) {
                return ptr_ + pos_dist;
            }

            append(in_dist);

            auto new_pos = begin() + pos_dist;

            auto rpos_start = rbegin() + in_dist;
            auto rpos_stop = rend() - pos_dist;
            std::move(rpos_start, rpos_stop, rpos_start - in_dist);

            std::copy(first, last, new_pos);

            return new_pos;
        }

        template <typename U>
        constexpr iterator insert(const_iterator pos, size_type count, const U& value)
        {
            difference_type pos_dist = std::distance(cbegin(), pos);
            if (pos_dist < 0 || pos_dist > size_) {
                throw std::invalid_argument("unbound input pos");
            }

            if (count == 0) {
                return ptr_ + pos_dist;
            }

            append(count);

            auto new_pos = begin() + pos_dist;

            auto rpos_start = rbegin() + count;
            auto rpos_stop = rend() - pos_dist;
            std::move(rpos_start, rpos_stop, rpos_start - count);

            std::fill_n(new_pos, count, value);

            return new_pos;
        }

        constexpr iterator erase(const_iterator first, const_iterator last)
        {
            difference_type dist = std::distance(first, last);
            if (dist < 0) {
                throw std::invalid_argument("negative distance between iterators");
            }

            difference_type first_dist = std::distance(cbegin(), first);
            if (first_dist < 0 || first_dist > size_) {
                throw std::invalid_argument("unbound input first");
            }

            difference_type last_dist = std::distance(cbegin(), last);
            if (last_dist < 0 || last_dist > size_) {
                throw std::invalid_argument("unbound input last");
            }

            if (dist == 0) {
                return ptr_ + last_dist - 1;
            }

            std::move(last, cend(), ptr_ + first_dist);

            resize(size_ - dist);

            return ptr_ + last_dist - 1;
        }

        constexpr iterator erase(const_iterator pos)
        {
            return erase(pos, pos + 1);
        }

    private:
        value_type ptr_[Capacity];
        size_type size_ = 0;
    };
}

using details::simple_allocator;
using details::simple_vector;
using details::simple_array;
}

namespace oc::arrnd {
namespace details {
    template <typename T, template <typename> typename Allocator = simple_allocator>
    struct simple_vector_traits {
        using storage_type = simple_vector<T, Allocator<T>>;
        template <typename U>
        using replaced_type = simple_vector_traits<U, Allocator>;
        template <typename U>
        using allocator_template_type = Allocator<U>;
    };

    template <typename T, std::int64_t N>
    struct simple_array_traits {
        using storage_type = simple_array<T, N>;
        template <typename U>
        using replaced_type = simple_array_traits<U, N>;
        template <typename U>
        using allocator_template_type = simple_allocator<U>;
    };
}

using details::simple_vector_traits;
using details::simple_array_traits;
}

namespace oc::arrnd {
namespace details {
    template <typename T1, typename T2>
    using tol_type = decltype(std::remove_cvref_t<T1>{} - std::remove_cvref_t<T2>{});

    template <std::integral T>
    [[nodiscard]] inline constexpr T default_atol() noexcept
    {
        return T{0};
    }

    template <typename T>
        requires(std::floating_point<T> || template_type<T, std::complex>)
    [[nodiscard]] inline constexpr T default_atol() noexcept
    {
        return T{1e-8};
    }

    template <std::integral T>
    [[nodiscard]] inline constexpr T default_rtol() noexcept
    {
        return T{0};
    }

    template <typename T>
        requires(std::floating_point<T> || template_type<T, std::complex>)
    [[nodiscard]] inline constexpr T default_rtol() noexcept
    {
        return T{1e-5};
    }

    template <typename T1, typename T2>
        requires((std::is_arithmetic_v<T1> || template_type<T1, std::complex>)
            && (std::is_arithmetic_v<T2> || template_type<T2, std::complex>))
    [[nodiscard]] inline constexpr bool close(const T1& a, const T2& b,
        const tol_type<T1, T2>& atol = default_atol<tol_type<T1, T2>>(),
        const tol_type<T1, T2>& rtol = default_rtol<tol_type<T1, T2>>()) noexcept
    {
        using std::abs;
        if (a == b) {
            return true;
        }
        const tol_type<T1, T2> reps{rtol * (abs(a) > abs(b) ? abs(a) : abs(b))};
        return abs(a - b) <= (atol > reps ? atol : reps);
    }

    template <typename T>
    [[nodiscard]] inline constexpr auto sign(const T& value)
    {
        return (T{0} < value) - (value < T{0});
    }
}
}

namespace oc::arrnd {
namespace details {
    // represents right-open interval
    template <typename T = std::int64_t>
        requires(std::is_fundamental_v<T>)
    class interval {
    public:
        using size_type = T;

        static constexpr size_type neginf = std::numeric_limits<size_type>::min();
        static constexpr size_type posinf = std::numeric_limits<size_type>::max();

        constexpr interval(size_type start, size_type stop, size_type step = 1)
            : start_(start)
            , stop_(stop)
            , step_(step)
        {
            if (start > stop || step <= 0) {
                throw std::invalid_argument("invalid interval");
            }

            if constexpr (std::is_integral_v<size_type> && std::is_signed_v<size_type>) {
                if (start != neginf && stop != posinf && start < 0 && stop > posinf + start) {
                    throw std::overflow_error("interval absdiff does not fit size_type");
                }
            }
        }

        constexpr interval() = default; // interval of first element
        constexpr interval(const interval&) = default;
        constexpr interval& operator=(const interval&) = default;
        constexpr interval(interval&&) = default;
        constexpr interval& operator=(interval&&) = default;

        [[nodiscard]] constexpr size_type start() const noexcept
        {
            return start_;
        }

        [[nodiscard]] constexpr size_type stop() const noexcept
        {
            return stop_;
        }

        [[nodiscard]] constexpr size_type step() const noexcept
        {
            return step_;
        }

        // conversion from interval<size_type> to interval<U>
        template <typename U>
            requires(std::is_fundamental_v<U>)
        [[nodiscard]] constexpr operator interval<U>() const
        {
            if constexpr (std::is_same_v<size_type, U>) {
                return *this;
            } else {
                if (step_ < std::numeric_limits<U>::min() || step_ > std::numeric_limits<U>::max()) {
                    throw std::overflow_error(
                        "invalid interval casting - step value is not fit to converted size type");
                }

                if (isunbound(*this)) {
                    if (!isleftbound(*this) && !isrightbound(*this)) {
                        return interval<U>::full(step_);
                    }

                    if (isleftbound(*this)
                        && (start_ >= std::numeric_limits<U>::min() || start_ <= std::numeric_limits<U>::max())) {
                        return interval<U>::from(start_, step_);
                    }

                    if (isrightbound(*this)
                        && (stop_ >= std::numeric_limits<U>::min() || stop_ <= std::numeric_limits<U>::max())) {
                        return interval<U>::to(stop_, step_);
                    }
                }

                // conversion of negative values from signed to unsigned is undefined
                if constexpr (std::is_signed_v<size_type> && std::is_unsigned_v<U>) {
                    if (start_ < 0 || stop_ < 0) {
                        throw std::overflow_error(
                            "invalid interval casting - undifned conversion from negative signed to unsigned");
                    }
                }

                if (start_ < std::numeric_limits<U>::min() || start_ > std::numeric_limits<U>::max()) {
                    throw std::overflow_error(
                        "invalid interval casting - start value is not fit to converted size type");
                }

                if (stop_ < std::numeric_limits<U>::min() || stop_ > std::numeric_limits<U>::max()) {
                    throw std::overflow_error(
                        "invalid interval casting - stop value is not fit to converted size type");
                }

                return interval<U>(start_, stop_, step_);
            }
        }

        [[nodiscard]] static constexpr interval full(size_type step = 1) noexcept
        {
            if constexpr (std::is_signed_v<size_type>) {
                return interval{neginf, posinf, step};
            } else {
                return interval{0, posinf, step};
            }
        }

        [[nodiscard]] static constexpr interval from(size_type start, size_type step = 1) noexcept
        {
            return interval{start, posinf, step};
        }

        [[nodiscard]] static constexpr interval to(size_type stop, size_type step = 1) noexcept
        {
            if constexpr (std::is_signed_v<size_type>) {
                return interval{neginf, stop, step};
            } else {
                return interval{0, stop, step};
            }
        }

        [[nodiscard]] static constexpr interval at(size_type pos) noexcept
        {
            return interval{pos, pos + 1, 1};
        }

        [[nodiscard]] static constexpr interval between(size_type start, size_type stop, size_type step = 1) noexcept
        {
            return interval{start, stop, step};
        }

    private:
        size_type start_ = 0;
        size_type stop_ = 1;
        size_type step_ = 1;
    };

    template <typename T>
        requires(std::is_fundamental_v<T>)
    [[nodiscard]] inline constexpr bool isbound(interval<T> ival) noexcept
    {
        if constexpr (std::is_signed_v<T>) {
            return ival.start() != interval<T>::neginf && ival.stop() != interval<T>::posinf;
        } else {
            return ival.stop() != interval<T>::posinf;
        }
    }

    template <typename T>
        requires(std::is_fundamental_v<T>)
    [[nodiscard]] inline constexpr bool isleftbound(interval<T> ival) noexcept
    {
        if constexpr (std::is_signed_v<T>) {
            return ival.start() != interval<T>::neginf;
        } else {
            return true;
        }
    }

    template <typename T>
        requires(std::is_fundamental_v<T>)
    [[nodiscard]] inline constexpr bool isrightbound(interval<T> ival) noexcept
    {
        return ival.stop() != interval<T>::posinf;
    }

    template <typename T>
        requires(std::is_fundamental_v<T>)
    [[nodiscard]] inline constexpr bool isunbound(interval<T> ival) noexcept
    {
        return !isbound(ival);
    }

    template <typename T>
        requires(std::is_fundamental_v<T>)
    [[nodiscard]] inline constexpr bool isbetween(interval<T> ival, auto start, auto stop) noexcept
        requires(std::is_fundamental_v<decltype(start)> and std::is_fundamental_v<decltype(stop)>)
    {
        return !empty(ival) && ival.start() >= start && ival.stop() <= stop;
    }

    template <typename T>
        requires(std::is_fundamental_v<T>)
    [[nodiscard]] inline constexpr bool isbetween(interval<T> ival, auto value) noexcept
        requires(std::is_fundamental_v<decltype(value)>)
    {
        return !empty(ival) && value >= ival.start() && value < ival.stop();
    }

    template <typename T>
        requires(std::is_fundamental_v<T>)
    [[nodiscard]] inline constexpr bool empty(interval<T> ival) noexcept
    {
        return ival.start() == ival.stop();
    }

    template <typename T>
        requires(std::is_fundamental_v<T>)
    [[nodiscard]] inline constexpr T absdiff(interval<T> ival) noexcept
    {
        if (empty(ival)) {
            return 0;
        }
        if (isunbound(ival)) {
            return interval<T>::posinf;
        }
        if constexpr (std::is_signed_v<T>) {
            return static_cast<T>(std::ceil(static_cast<double>(std::abs(ival.stop() - ival.start())) / ival.step()));
        } else {
            return static_cast<T>(std::ceil(static_cast<double>(ival.stop() - ival.start()) / ival.step()));
        }
    }

    template <typename T>
        requires(std::is_fundamental_v<T>)
    [[nodiscard]] inline constexpr interval<T> bound(interval<T> ival, auto start, auto stop)
        requires(std::is_fundamental_v<decltype(start)> && std::is_fundamental_v<decltype(stop)>)
    {
        if constexpr (std::is_signed_v<T>) {
            return interval<T>{ival.start() == interval<T>::neginf ? start : ival.start(),
                ival.stop() == interval<T>::posinf ? stop : ival.stop(), ival.step()};
        } else {
            return interval<T>{ival.start(), ival.stop() == interval<T>::posinf ? stop : ival.stop(), ival.step()};
        }
    }

    template <typename T, typename U>
        requires(std::is_fundamental_v<T> && std::is_fundamental_v<U>)
    [[nodiscard]] inline constexpr bool operator==(const interval<T>& lhs, const interval<U>& rhs) noexcept
    {
        return (empty(lhs) && empty(rhs))
            || (lhs.start() == rhs.start() && lhs.stop() == rhs.stop() && lhs.step() == rhs.step());
    }

    template <typename T>
    inline constexpr std::ostream& operator<<(std::ostream& os, const interval<T>& ival)
    {
        if (empty(ival)) {
            os << "empty";
            return os;
        }

        os << "[" << ival.start() << "," << ival.stop() << "," << ival.step() << ")";
        return os;
    }
}

using details::interval;
using details::isbound;
using details::isleftbound;
using details::isrightbound;
using details::isunbound;
using details::isbetween;
using details::empty;
using details::absdiff;
using details::bound;
}

// custom internal general usage concepts and traits
namespace oc::arrnd {
namespace details {
    template <typename Iter>
    concept iterator_of_type_integral = iterator_type<Iter> && std::integral<iterator_value_t<Iter>>;

    template <typename Cont>
    concept iterable_of_type_integral = iterable_type<Cont> && std::integral<iterable_value_t<Cont>>;

    template <typename Iter>
    concept iterator_of_type_interval = iterator_type<Iter> && template_type<iterator_value_t<Iter>, interval>;

    template <typename Cont>
    concept iterable_of_type_interval = iterable_type<Cont> && template_type<iterable_value_t<Cont>, interval>;

    template <typename T>
    struct overflow_check_multiplies {
        constexpr T operator()(T lhs, T rhs) const
        {
            if (lhs > std::numeric_limits<T>::max() / rhs) {
                throw std::overflow_error("invalid multiplication");
            }
            return lhs * rhs;
        }
    };

    template <typename T>
    struct overflow_check_plus {
        constexpr T operator()(T lhs, T rhs) const
        {
            if (lhs > std::numeric_limits<T>::max() - rhs) {
                throw std::overflow_error("invalid addition");
            }
            return lhs + rhs;
        }
    };
}
}

namespace oc::arrnd {
namespace details {
    enum class arrnd_iterator_position { none, begin, end, rbegin, rend };

    enum class arrnd_hint : std::size_t {
        none = 0,
        continuous = std::size_t{1} << 0,
        sliced = std::size_t{1} << 1,
        transposed = std::size_t{1} << 2,
        bitscount = 3,
    };

    [[nodiscard]] inline constexpr arrnd_hint operator|(const arrnd_hint& lhs, const arrnd_hint& rhs) noexcept
    {
        return static_cast<arrnd_hint>(static_cast<std::underlying_type_t<arrnd_hint>>(lhs)
            | static_cast<std::underlying_type_t<arrnd_hint>>(rhs));
    }

    inline constexpr arrnd_hint& operator|=(arrnd_hint& lhs, const arrnd_hint& rhs) noexcept
    {
        return (lhs = lhs | rhs);
    }

    [[nodiscard]] inline constexpr arrnd_hint operator&(const arrnd_hint& lhs, const arrnd_hint& rhs) noexcept
    {
        return static_cast<arrnd_hint>(static_cast<std::underlying_type_t<arrnd_hint>>(lhs)
            & static_cast<std::underlying_type_t<arrnd_hint>>(rhs));
    }

    inline constexpr arrnd_hint& operator&=(arrnd_hint& lhs, const arrnd_hint& rhs) noexcept
    {
        return (lhs = lhs & rhs);
    }

    [[nodiscard]] inline constexpr arrnd_hint operator~(const arrnd_hint& s) noexcept
    {
        return static_cast<arrnd_hint>(~static_cast<std::underlying_type_t<arrnd_hint>>(s));
    }

    [[nodiscard]] inline constexpr auto to_underlying(const arrnd_hint& s) noexcept
    {
        return static_cast<std::underlying_type_t<arrnd_hint>>(s);
    }

    enum class arrnd_squeeze_type {
        left,
        right,
        trim,
        full,
    };

    template <typename StorageTraits = simple_vector_traits<std::size_t>>
        requires(std::is_same_v<std::size_t, typename StorageTraits::storage_type::value_type>)
    class arrnd_info {
    public:
        using storage_traits_type = StorageTraits;

        using extent_storage_type = typename storage_traits_type::storage_type;
        using extent_type = typename extent_storage_type::value_type;

        using boundary_type = interval<extent_type>;

        constexpr arrnd_info() = default;
        constexpr arrnd_info(const arrnd_info&) = default;
        constexpr arrnd_info& operator=(const arrnd_info&) = default;
        constexpr arrnd_info(arrnd_info&&) = default;
        constexpr arrnd_info& operator=(arrnd_info&&) = default;

        template <iterator_of_type_integral InputIt>
        constexpr arrnd_info(InputIt first_dim, InputIt last_dim)
        {
            if (std::distance(first_dim, last_dim) < 0) {
                throw std::invalid_argument("invalid input iterators distance < 0");
            }

            if (std::distance(first_dim, last_dim) == 0 || std::any_of(first_dim, last_dim, [](auto dim) {
                    return dim == 0;
                })) {
                return;
            }

            if constexpr (std::signed_integral<std::iter_value_t<InputIt>>) {
                if (std::any_of(first_dim, last_dim, [](auto dim) {
                        return dim < 0;
                    })) {
                    throw std::invalid_argument("invalid dims < 0");
                }
            }

            dims_.reserve(std::distance(first_dim, last_dim));
            dims_.insert(dims_.cend(), first_dim, last_dim);

            strides_.resize(std::distance(first_dim, last_dim));
            std::exclusive_scan(dims_.crbegin(), dims_.crend(), strides_.rbegin(), extent_type{1},
                overflow_check_multiplies<extent_type>{});

            if (strides_[0] > std::numeric_limits<extent_type>::max() / dims_[0]) {
                throw std::overflow_error("invalid multiplication");
            }
            indices_boundary_ = boundary_type(extent_type{0}, strides_[0] * dims_[0]);

            hints_ = arrnd_hint::continuous;
        }

        template <iterable_of_type_integral Cont>
        explicit constexpr arrnd_info(Cont&& dims)
            : arrnd_info(std::begin(dims), std::end(dims))
        { }

        explicit constexpr arrnd_info(std::initializer_list<extent_type> dims)
            : arrnd_info(dims.begin(), dims.end())
        { }

        // for maximum flexibility, a constructor that sets the class members and check their validity.
        // this constructor may add hints in addition to the hints argument.
        template <iterator_of_type_integral InputIt1, iterator_of_type_integral InputIt2>
        explicit constexpr arrnd_info(InputIt1 first_dim, InputIt1 last_dim, InputIt2 first_stride,
            InputIt2 last_stride, template_type<interval> auto indices_boundary, arrnd_hint hints)
        {
            if (std::distance(first_dim, last_dim) < 0) {
                throw std::invalid_argument("invalid input iterators distance < 0");
            }

            if (std::distance(first_dim, last_dim) != std::distance(first_stride, last_stride)) {
                throw std::invalid_argument("invalid strides - different number from dimensions");
            }

            if constexpr (std::signed_integral<std::iter_value_t<InputIt1>>) {
                if (std::any_of(first_dim, last_dim, [](auto dim) {
                        return dim < 0;
                    })) {
                    throw std::invalid_argument("invalid dims < 0");
                }
            }

            if constexpr (std::signed_integral<std::iter_value_t<InputIt2>>) {
                if (std::any_of(first_stride, last_stride, [](auto stride) {
                        return stride < 0;
                    })) {
                    throw std::invalid_argument("invalid strides < 0");
                }
            }

            hints_ = arrnd_hint::none;

            if (std::distance(first_dim, last_dim) == 0) {
                if (!empty(indices_boundary)) {
                    throw std::invalid_argument("invalid indices boundary");
                }
                hints_ = hints | arrnd_hint::continuous;
                return;
            }

            if (std::any_of(first_dim, last_dim, [](auto dim) {
                    return dim == 0;
                })) {
                hints_ = hints | arrnd_hint::continuous;
                return;
            }

            extent_type num_elem
                = std::reduce(first_dim, last_dim, extent_type{1}, overflow_check_multiplies<extent_type>{});

            if (indices_boundary.stop() - indices_boundary.start() != num_elem) {
                if (to_underlying(hints & arrnd_hint::continuous)) {
                    throw std::invalid_argument("invalid hint - not continuous according to dims and indices boundary");
                }
                hints_ |= arrnd_hint::sliced;
            } else {
                hints_ |= arrnd_hint::continuous;
            }

            dims_.reserve(std::distance(first_dim, last_dim));
            dims_.insert(std::cend(dims_), first_dim, last_dim);

            strides_.reserve(std::distance(first_stride, last_stride));
            strides_.insert(std::cend(strides_), first_stride, last_stride);

            indices_boundary_ = static_cast<boundary_type>(indices_boundary);

            hints_ |= hints;
        }

        template <iterable_of_type_integral Cont1, iterable_of_type_integral Cont2>
        explicit constexpr arrnd_info(
            Cont1&& dims, Cont2&& strides, template_type<interval> auto indices_boundary, arrnd_hint hints)
            : arrnd_info(
                std::begin(dims), std::end(dims), std::begin(strides), std::end(strides), indices_boundary, hints)
        { }

        explicit constexpr arrnd_info(std::initializer_list<extent_type> dims,
            std::initializer_list<extent_type> strides, template_type<interval> auto indices_boundary, arrnd_hint hints)
            : arrnd_info(dims.begin(), dims.end(), strides.begin(), strides.end(), indices_boundary, hints)
        { }

        // the constructor tries deducing the hints value according to its parameters
        template <iterator_of_type_integral InputIt1, iterator_of_type_integral InputIt2>
        explicit constexpr arrnd_info(InputIt1 first_dim, InputIt1 last_dim, InputIt2 first_stride,
            InputIt2 last_stride, template_type<interval> auto indices_boundary)
            : arrnd_info(first_dim, last_dim, first_stride, last_stride, indices_boundary, arrnd_hint::none)
        { }

        template <iterable_of_type_integral Cont1, iterable_of_type_integral Cont2>
        explicit constexpr arrnd_info(Cont1&& dims, Cont2&& strides, template_type<interval> auto indices_boundary)
            : arrnd_info(std::begin(dims), std::end(dims), std::begin(strides), std::end(strides), indices_boundary)
        { }

        explicit constexpr arrnd_info(std::initializer_list<extent_type> dims,
            std::initializer_list<extent_type> strides, template_type<interval> auto indices_boundary)
            : arrnd_info(dims.begin(), dims.end(), strides.begin(), strides.end(), indices_boundary)
        { }

        // empty arrnd info with specified hints e.g. an empty slice
        explicit constexpr arrnd_info(arrnd_hint hints)
            : hints_(arrnd_hint::continuous | hints)
        { }

        [[nodiscard]] constexpr const extent_storage_type& dims() const noexcept
        {
            return dims_;
        }

        [[nodiscard]] constexpr const extent_storage_type& strides() const noexcept
        {
            return strides_;
        }

        [[nodiscard]] constexpr const boundary_type& indices_boundary() const noexcept
        {
            return indices_boundary_;
        }

        [[nodiscard]] constexpr arrnd_hint hints() const noexcept
        {
            return hints_;
        }

    private:
        extent_storage_type dims_;
        extent_storage_type strides_;
        boundary_type indices_boundary_{0, 0};
        arrnd_hint hints_{arrnd_hint::continuous};
    };

    template <typename StorageTraits, iterator_of_type_interval InputIt>
    [[nodiscard]] inline constexpr arrnd_info<StorageTraits> slice(
        const arrnd_info<StorageTraits>& info, InputIt first_dim_boundary, InputIt last_dim_boundary)
    {
        if (std::distance(first_dim_boundary, last_dim_boundary) < 0) {
            throw std::invalid_argument("invalid input iterators distance < 0");
        }

        if (std::size(info.dims()) == 0 && std::distance(first_dim_boundary, last_dim_boundary) == 0) {
            return arrnd_info<StorageTraits>(info.hints() | arrnd_hint::continuous | arrnd_hint::sliced);
        }

        if (std::size(info.dims()) < std::distance(first_dim_boundary, last_dim_boundary)) {
            throw std::invalid_argument("invalid dim boundaries - different number from dims");
        }

        // input boundries might not be of the same type
        typename arrnd_info<StorageTraits>::storage_traits_type::template replaced_type<
            typename arrnd_info<StorageTraits>::boundary_type>::storage_type
            bounded_dim_boundaries(std::distance(first_dim_boundary, last_dim_boundary));

        std::transform(first_dim_boundary, last_dim_boundary, std::begin(info.dims()),
            std::begin(bounded_dim_boundaries), [](auto boundary, auto dim) {
                return bound(static_cast<typename arrnd_info<StorageTraits>::boundary_type>(boundary), 0, dim);
            });

        if (!std::transform_reduce(std::begin(bounded_dim_boundaries), std::end(bounded_dim_boundaries),
                std::begin(info.dims()), true, std::logical_and<>{}, [](auto boundary, auto dim) {
                    return empty(boundary) || isbetween(boundary, 0, dim);
                })) {
            throw std::out_of_range("invalid dim boundaries - not suitable for dims");
        }

        typename arrnd_info<StorageTraits>::extent_storage_type dims = info.dims();
        std::transform(
            std::begin(bounded_dim_boundaries), std::end(bounded_dim_boundaries), std::begin(dims), [](auto boundary) {
                return absdiff(boundary);
            });

        if (std::any_of(std::begin(dims), std::end(dims), [](auto dim) {
                return dim == 0;
            })) {
            return arrnd_info<StorageTraits>(info.hints() | arrnd_hint::continuous | arrnd_hint::sliced);
        }

        if (std::equal(std::begin(dims), std::end(dims), std::begin(info.dims()), std::end(info.dims()))) {
            return info;
        }

        typename arrnd_info<StorageTraits>::extent_storage_type strides(
            std::begin(info.strides()), std::end(info.strides()));

        std::transform(std::begin(bounded_dim_boundaries), std::end(bounded_dim_boundaries), std::begin(info.strides()),
            std::begin(strides), [](auto boundary, auto stride) {
                if (boundary.step()
                    > std::numeric_limits<typename arrnd_info<StorageTraits>::extent_type>::max() / stride) {
                    throw std::overflow_error("invalid multiplication");
                }
                return boundary.step() * stride;
            });

        typename arrnd_info<StorageTraits>::extent_type offset = info.indices_boundary().start()
            + std::transform_reduce(std::begin(bounded_dim_boundaries), std::end(bounded_dim_boundaries),
                std::begin(info.strides()), typename arrnd_info<StorageTraits>::extent_type{0},
                overflow_check_plus<typename arrnd_info<StorageTraits>::extent_type>{}, [](auto boundary, auto stride) {
                    if (boundary.start()
                        > std::numeric_limits<typename arrnd_info<StorageTraits>::extent_type>::max() / stride) {
                        throw std::overflow_error("invalid multiplication");
                    }
                    return boundary.start() * stride;
                });

        typename arrnd_info<StorageTraits>::extent_type max_index = std::transform_reduce(std::begin(dims),
            std::end(dims), std::begin(strides), typename arrnd_info<StorageTraits>::extent_type{0},
            overflow_check_plus<typename arrnd_info<StorageTraits>::extent_type>{}, [](auto dim, auto stride) {
                if (dim - 1 > std::numeric_limits<typename arrnd_info<StorageTraits>::extent_type>::max() / stride) {
                    throw std::overflow_error("invalid multiplication");
                }
                return (dim - 1) * stride;
            });

        typename arrnd_info<StorageTraits>::boundary_type indices_boundary(offset, offset + max_index + 1);

        typename arrnd_info<StorageTraits>::extent_type num_elem
            = std::reduce(std::begin(dims), std::end(dims), typename arrnd_info<StorageTraits>::extent_type{1},
                overflow_check_multiplies<typename arrnd_info<StorageTraits>::extent_type>{});

        arrnd_hint hints = info.hints() | arrnd_hint::sliced;
        if (max_index + 1 == num_elem) {
            hints |= arrnd_hint::continuous;
        } else {
            hints &= ~arrnd_hint::continuous;
        }

        return arrnd_info<StorageTraits>(dims, strides, indices_boundary, hints);
    }

    template <typename StorageTraits, iterable_of_type_interval Cont>
    [[nodiscard]] inline constexpr arrnd_info<StorageTraits> slice(
        const arrnd_info<StorageTraits>& info, Cont&& dim_boundaries)
    {
        return slice(info, std::begin(dim_boundaries), std::end(dim_boundaries));
    }

    template <typename StorageTraits>
    [[nodiscard]] inline constexpr arrnd_info<StorageTraits> slice(const arrnd_info<StorageTraits>& info,
        std::initializer_list<typename arrnd_info<StorageTraits>::boundary_type> dim_boundaries)
    {
        return slice(info, dim_boundaries.begin(), dim_boundaries.end());
    }

    template <typename StorageTraits>
    [[nodiscard]] inline constexpr arrnd_info<StorageTraits> slice(const arrnd_info<StorageTraits>& info,
        template_type<interval> auto dim_boundary, typename arrnd_info<StorageTraits>::extent_type axis)
    {
        if (std::size(info.dims()) == 0) {
            throw std::invalid_argument("invalid arrnd info");
        }

        if (axis >= std::size(info.dims())) {
            throw std::out_of_range("invalid axis");
        }

        typename arrnd_info<StorageTraits>::storage_traits_type::template replaced_type<
            typename arrnd_info<StorageTraits>::boundary_type>::storage_type dim_boundaries(std::size(info.dims()),
            arrnd_info<StorageTraits>::boundary_type::full());

        dim_boundaries[axis] = static_cast<typename arrnd_info<StorageTraits>::boundary_type>(dim_boundary);

        return slice(info, dim_boundaries);
    }

    template <typename StorageTraits>
    [[nodiscard]] inline constexpr arrnd_info<StorageTraits> squeeze(const arrnd_info<StorageTraits>& info,
        arrnd_squeeze_type squeeze_type,
        typename arrnd_info<StorageTraits>::extent_type max_count
        = std::numeric_limits<typename arrnd_info<StorageTraits>::extent_type>::max())
    {
        if (squeeze_type == arrnd_squeeze_type::full
            && max_count != std::numeric_limits<typename arrnd_info<StorageTraits>::extent_type>::max()) {
            throw std::invalid_argument("invalid squeeze type - no max count support for full squeeze type");
        }

        if (size(info) == 0) {
            if (max_count != std::numeric_limits<typename arrnd_info<StorageTraits>::extent_type>::max()
                && max_count != 0) {
                throw std::invalid_argument("invalid squeeze - empty squeeze support for max count > 0");
            }
            return info;
        }

        if (size(info) == 1) {
            if (max_count != std::numeric_limits<typename arrnd_info<StorageTraits>::extent_type>::max()
                && max_count > 1) {
                throw std::invalid_argument("invalid squeeze - no 1d squeeze support for max count > 1");
            }

            // max_count is valid - no squeeze in case of 1d
            return info;
        }

        typename arrnd_info<StorageTraits>::extent_storage_type dims(info.dims());
        typename arrnd_info<StorageTraits>::extent_storage_type strides(info.strides());

        if (squeeze_type == arrnd_squeeze_type::left || squeeze_type == arrnd_squeeze_type::trim) {
            typename arrnd_info<StorageTraits>::extent_type left_ones_count = 0;
            auto dims_it = std::begin(dims);
            while (*dims_it == 1 && left_ones_count < max_count) {
                ++dims_it;
                ++left_ones_count;
            }
            if (left_ones_count > 0) {
                dims.erase(std::begin(dims), std::next(std::begin(dims), left_ones_count));
                dims.shrink_to_fit();

                strides.erase(std::begin(strides), std::next(std::begin(strides), left_ones_count));
                strides.shrink_to_fit();
            }
        }

        if (squeeze_type == arrnd_squeeze_type::right || squeeze_type == arrnd_squeeze_type::trim) {
            typename arrnd_info<StorageTraits>::extent_type right_ones_count = 0;
            auto dims_it = std::rbegin(dims);
            while (*dims_it == 1 && right_ones_count < max_count) {
                ++dims_it;
                ++right_ones_count;
            }
            if (right_ones_count > 0) {
                dims.erase(std::next(std::begin(dims), std::size(dims) - right_ones_count), std::end(dims));
                dims.shrink_to_fit();

                strides.erase(std::next(std::begin(strides), std::size(strides) - right_ones_count), std::end(strides));
                strides.shrink_to_fit();
            }
        }

        if (squeeze_type == arrnd_squeeze_type::full) {
            if (std::any_of(std::begin(dims), std::end(dims), [](auto dim) {
                    return dim == 1;
                })) {
                typename arrnd_info<StorageTraits>::extent_type not_one_index = 0;
                for (typename arrnd_info<StorageTraits>::extent_type i = 0; i < std::size(dims); ++i) {
                    if (dims[i] != 1) {
                        dims[not_one_index] = dims[i];
                        strides[not_one_index] = strides[i];
                        ++not_one_index;
                    }
                }

                dims.resize(not_one_index);
                dims.shrink_to_fit();

                strides.resize(not_one_index);
                strides.shrink_to_fit();
            }
        }

        return arrnd_info<StorageTraits>(dims, strides, info.indices_boundary(), info.hints());
    }

    template <typename StorageTraits, iterator_of_type_integral InputIt>
    [[nodiscard]] inline constexpr arrnd_info<StorageTraits> transpose(
        const arrnd_info<StorageTraits>& info, InputIt first_axis, InputIt last_axis)
    {
        if (std::distance(first_axis, last_axis) != std::size(info.dims())) {
            throw std::invalid_argument("invalid input iterators distance != number of dims");
        }

        if (empty(info)) {
            return info;
        }

        if (std::size(info.dims()) == 0) {
            if (*first_axis != 0) {
                throw std::out_of_range("invalid axis value");
            }
            return info;
        }

        if (std::any_of(first_axis, last_axis, [&info](std::size_t axis) {
                return axis < 0 || axis >= std::size(info.dims());
            })) {
            throw std::out_of_range("invalid axis value");
        }

        auto is_unique_axes = [](InputIt first, InputIt last) {
            typename arrnd_info<StorageTraits>::extent_storage_type sorted_axes(first, last);
            std::sort(std::begin(sorted_axes), std::end(sorted_axes));
            return std::adjacent_find(std::begin(sorted_axes), std::end(sorted_axes)) == std::end(sorted_axes);
        };

        if (!is_unique_axes(first_axis, last_axis)) {
            throw std::invalid_argument("not unique axes");
        }

        if (std::is_sorted(first_axis, last_axis)) {
            return info;
        }

        typename arrnd_info<StorageTraits>::extent_storage_type dims(std::size(info.dims()));
        typename arrnd_info<StorageTraits>::extent_storage_type strides(std::size(info.strides()));

        typename arrnd_info<StorageTraits>::extent_type i = 0;
        for (auto axes_it = first_axis; axes_it != last_axis; ++axes_it, ++i) {
            dims[i] = info.dims()[*axes_it];
            strides[i] = info.strides()[*axes_it];
        }

        return arrnd_info<StorageTraits>(dims, strides, info.indices_boundary(), info.hints() | arrnd_hint::transposed);
    }

    template <typename StorageTraits, iterable_of_type_integral Cont>
    [[nodiscard]] inline constexpr arrnd_info<StorageTraits> transpose(
        const arrnd_info<StorageTraits>& info, Cont&& axes)
    {
        return transpose(info, std::begin(axes), std::end(axes));
    }

    template <typename StorageTraits>
    [[nodiscard]] inline constexpr arrnd_info<StorageTraits> transpose(const arrnd_info<StorageTraits>& info,
        std::initializer_list<typename arrnd_info<StorageTraits>::extent_type> axes)
    {
        return transpose(info, axes.begin(), axes.end());
    }

    template <typename StorageTraits>
    [[nodiscard]] inline constexpr arrnd_info<StorageTraits> swap(const arrnd_info<StorageTraits>& info,
        typename arrnd_info<StorageTraits>::extent_type first_axis = 0,
        typename arrnd_info<StorageTraits>::extent_type second_axis = 1)
    {

        if (first_axis >= std::size(info.dims()) || second_axis >= std::size(info.dims())) {
            throw std::out_of_range("invalid axis value");
        }

        if (first_axis == second_axis) {
            return info;
        }

        typename arrnd_info<StorageTraits>::extent_storage_type dims(info.dims());
        typename arrnd_info<StorageTraits>::extent_storage_type strides(info.strides());

        std::swap(dims[first_axis], dims[second_axis]);
        std::swap(strides[first_axis], strides[second_axis]);

        return arrnd_info<StorageTraits>(dims, strides, info.indices_boundary(), info.hints() | arrnd_hint::transposed);
    }

    template <typename StorageTraits>
    [[nodiscard]] inline constexpr arrnd_info<StorageTraits> move(const arrnd_info<StorageTraits>& info,
        typename arrnd_info<StorageTraits>::extent_type src_axis = 0,
        typename arrnd_info<StorageTraits>::extent_type dst_axis = 1)
    {

        if (src_axis >= std::size(info.dims()) || dst_axis >= std::size(info.dims())) {
            throw std::out_of_range("invalid axis value");
        }

        if (src_axis == dst_axis) {
            return info;
        }

        typename arrnd_info<StorageTraits>::extent_storage_type dims(info.dims());
        typename arrnd_info<StorageTraits>::extent_storage_type strides(info.strides());

        auto src_dim = dims[src_axis];
        dims.erase(std::next(std::begin(dims), src_axis));
        dims.insert(std::next(std::begin(dims), dst_axis), 1, src_dim);

        auto src_stride = strides[src_axis];
        strides.erase(std::next(std::begin(strides), src_axis));
        strides.insert(std::next(std::begin(strides), dst_axis), 1, src_stride);

        return arrnd_info<StorageTraits>(dims, strides, info.indices_boundary(), info.hints() | arrnd_hint::transposed);
    }

    template <typename StorageTraits>
    [[nodiscard]] inline constexpr arrnd_info<StorageTraits> roll(const arrnd_info<StorageTraits>& info, int offset = 1)
    {
        if (empty(info) || std::size(info.dims()) == 1 || offset == 0) {
            return info;
        }

        typename arrnd_info<StorageTraits>::extent_storage_type dims(info.dims());
        typename arrnd_info<StorageTraits>::extent_storage_type strides(info.strides());

        typename arrnd_info<StorageTraits>::extent_type wrapped_offset
            = static_cast<typename arrnd_info<StorageTraits>::extent_type>(std::abs(offset)) % std::size(info.dims());

        if (offset > 0) {
            std::rotate(std::rbegin(dims), std::next(std::rbegin(dims), wrapped_offset), std::rend(dims));
            std::rotate(std::rbegin(strides), std::next(std::rbegin(strides), wrapped_offset), std::rend(strides));
        } else {
            std::rotate(std::begin(dims), std::next(std::begin(dims), wrapped_offset), std::end(dims));
            std::rotate(std::begin(strides), std::next(std::begin(strides), wrapped_offset), std::end(strides));
        }

        return arrnd_info<StorageTraits>(dims, strides, info.indices_boundary(), info.hints() | arrnd_hint::transposed);
    }

    template <typename StorageTraits>
    [[nodiscard]] inline constexpr typename arrnd_info<StorageTraits>::extent_type total(
        const arrnd_info<StorageTraits>& info)
    {
        if (empty(info)) {
            return 0;
        }

        return std::reduce(std::begin(info.dims()), std::end(info.dims()),
            typename arrnd_info<StorageTraits>::extent_type{1},
            overflow_check_multiplies<typename arrnd_info<StorageTraits>::extent_type>{});
    }

    template <typename StorageTraits>
    [[nodiscard]] inline constexpr typename arrnd_info<StorageTraits>::extent_type size(
        const arrnd_info<StorageTraits>& info)
    {
        return std::size(info.dims());
    }

    template <typename StorageTraits>
    [[nodiscard]] inline constexpr bool empty(const arrnd_info<StorageTraits>& info)
    {
        return size(info) == 0;
    }

    template <typename StorageTraits>
    [[nodiscard]] inline constexpr bool iscontinuous(const arrnd_info<StorageTraits>& info)
    {
        return static_cast<bool>(to_underlying(info.hints() & arrnd_hint::continuous));
    }

    template <typename StorageTraits>
    [[nodiscard]] inline constexpr bool issliced(const arrnd_info<StorageTraits>& info)
    {
        return static_cast<bool>(to_underlying(info.hints() & arrnd_hint::sliced));
    }

    template <typename StorageTraits>
    [[nodiscard]] inline constexpr bool istransposed(const arrnd_info<StorageTraits>& ai)
    {
        return static_cast<bool>(to_underlying(ai.hints() & arrnd_hint::transposed));
    }

    template <typename StorageTraits>
    [[nodiscard]] inline constexpr bool isvector(const arrnd_info<StorageTraits>& info)
    {
        return size(info) == 1;
    }

    template <typename StorageTraits>
    [[nodiscard]] inline constexpr bool ismatrix(const arrnd_info<StorageTraits>& info)
    {
        return size(info) == 2;
    }

    template <typename StorageTraits>
    [[nodiscard]] inline constexpr bool isrow(const arrnd_info<StorageTraits>& info)
    {
        return ismatrix(info) && info.dims().front() == 1;
    }

    template <typename StorageTraits>
    [[nodiscard]] inline constexpr bool iscolumn(const arrnd_info<StorageTraits>& info)
    {
        return ismatrix(info) && info.dims().back() == 1;
    }

    template <typename StorageTraits>
    [[nodiscard]] inline constexpr bool isscalar(const arrnd_info<StorageTraits>& info)
    {
        return total(info) == 1;
    }

    template <typename StorageTraits, iterator_of_type_integral InputIt>
    [[nodiscard]] inline constexpr typename arrnd_info<StorageTraits>::extent_type sub2ind(
        const arrnd_info<StorageTraits>& info, InputIt first_sub, InputIt last_sub)
    {
        if (std::distance(first_sub, last_sub) <= 0) {
            throw std::invalid_argument("invalid input iterators distance <= 0");
        }

        if (std::distance(first_sub, last_sub) > size(info)) {
            throw std::invalid_argument("bad sub iterators distance > ndi.ndims");
        }

        if (empty(info)) {
            throw std::invalid_argument("undefined operation for empty arrnd info");
        }

        if (!std::transform_reduce(first_sub, last_sub,
                std::next(std::begin(info.dims()), std::size(info.dims()) - std::distance(first_sub, last_sub)), true,
                std::logical_and<>{}, [](auto sub, auto dim) {
                    return sub >= 0 && sub < dim;
                })) {
            throw std::out_of_range("invalid subs - not suitable for dims");
        }

        return info.indices_boundary().start()
            + std::transform_reduce(first_sub, last_sub,
                std::next(std::begin(info.strides()), std::size(info.strides()) - std::distance(first_sub, last_sub)),
                typename arrnd_info<StorageTraits>::extent_type{0},
                overflow_check_plus<typename arrnd_info<StorageTraits>::extent_type>{},
                overflow_check_multiplies<typename arrnd_info<StorageTraits>::extent_type>{});
    }

    template <typename StorageTraits, iterable_of_type_integral Cont>
    [[nodiscard]] inline constexpr typename arrnd_info<StorageTraits>::extent_type sub2ind(
        const arrnd_info<StorageTraits>& info, Cont&& subs)
    {
        return sub2ind(info, std::begin(subs), std::end(subs));
    }

    template <typename StorageTraits>
    [[nodiscard]] inline constexpr typename arrnd_info<StorageTraits>::extent_type sub2ind(
        const arrnd_info<StorageTraits>& info,
        std::initializer_list<typename arrnd_info<StorageTraits>::extent_type> subs)
    {
        return sub2ind(info, subs.begin(), subs.end());
    }

    template <typename StorageTraits, iterator_of_type_integral OutputIt>
    inline constexpr void ind2sub(
        const arrnd_info<StorageTraits>& info, typename arrnd_info<StorageTraits>::extent_type ind, OutputIt d_sub)
    {
        if (empty(info)) {
            throw std::invalid_argument("undefined operation for empty arrnd info");
        }

        if (!isbetween(info.indices_boundary(), ind)) {
            throw std::out_of_range("invalid ind - not inside indices boundary");
        }

        for (typename arrnd_info<StorageTraits>::extent_type i = 0; i < std::size(info.dims()); ++i) {
            *d_sub
                = (info.dims()[i] > 1 ? ((ind - info.indices_boundary().start()) / info.strides()[i]) % info.dims()[i]
                                      : 0);
            ++d_sub;
        }
    }

    // convert relative index to abosolute index
    template <typename StorageTraits>
    [[nodiscard]] inline constexpr typename arrnd_info<StorageTraits>::extent_type ind2ind(
        const arrnd_info<StorageTraits>& info, typename arrnd_info<StorageTraits>::extent_type rel_ind)
    {
        if (empty(info)) {
            throw std::invalid_argument("undefined operation for empty arrnd info");
        }

        if (rel_ind >= total(info)) {
            throw std::out_of_range("invalid ind - not inside indices boundary");
        }

        if (!issliced(info)) {
            return rel_ind;
        }

        if (iscontinuous(info)) {
            return info.indices_boundary().start() + rel_ind;
        }

        typename arrnd_info<StorageTraits>::extent_type current_rel_stride
            = std::reduce(std::next(std::begin(info.dims()), 1), std::end(info.dims()),
                typename arrnd_info<StorageTraits>::extent_type{1},
                overflow_check_multiplies<typename arrnd_info<StorageTraits>::extent_type>{});

        typename arrnd_info<StorageTraits>::extent_type abs_ind = info.indices_boundary().start();

        for (typename arrnd_info<StorageTraits>::extent_type i = 0; i < std::size(info.dims()); ++i) {
            auto sub = (info.dims()[i] > 1 ? (rel_ind / current_rel_stride) % info.dims()[i] : 0);
            abs_ind += sub * info.strides()[i];

            current_rel_stride /= (i + 1 >= std::size(info.dims()) ? 1 : info.dims()[i + 1]);
        }

        return abs_ind;
    }

    // return arrnd info suitable for reduced dimension at specified axis.
    // previous strides and hints are being ignored.
    template <typename StorageTraits>
    [[nodiscard]] inline constexpr arrnd_info<StorageTraits> reduce(
        const arrnd_info<StorageTraits>& info, typename arrnd_info<StorageTraits>::extent_type axis)
    {
        if (empty(info)) {
            throw std::invalid_argument("undefined operation for empty arrnd info");
        }

        if (axis >= std::size(info.dims())) {
            throw std::out_of_range("invalid axis value");
        }

        if (std::size(info.dims()) == 1) {
            return arrnd_info<StorageTraits>({1});
        }

        typename arrnd_info<StorageTraits>::extent_storage_type new_dims(info.dims());
        new_dims.erase(std::next(std::begin(new_dims), axis));
        new_dims.shrink_to_fit();

        return arrnd_info<StorageTraits>(new_dims);
    }

    // undo transposed info by rearranging dims accoding to sorted strides.
    template <typename StorageTraits>
    [[nodiscard]] inline constexpr arrnd_info<StorageTraits> unstranspose(const arrnd_info<StorageTraits>& info)
    {
        if (empty(info)) {
            return arrnd_info<StorageTraits>{};
        }

        if (!istransposed(info)) {
            return info;
        }

        typename arrnd_info<StorageTraits>::extent_storage_type new_dims(info.dims());
        typename arrnd_info<StorageTraits>::extent_storage_type new_strides(info.strides());

        auto z = zip(zipped(new_strides), zipped(new_dims));
        std::sort(std::begin(z), std::end(z), [](auto t1, auto t2) {
            return std::get<0>(t1) > std::get<0>(t2);
        });

        return arrnd_info<StorageTraits>(
            new_dims, new_strides, info.indices_boundary(), info.hints() & ~arrnd_hint::transposed);
    }

    // return simple arrnd info by dims only,
    // ignoring hints such as sliced.
    template <typename StorageTraits>
    [[nodiscard]] inline constexpr arrnd_info<StorageTraits> simplify(const arrnd_info<StorageTraits>& info)
    {
        if (empty(info)) {
            return arrnd_info<StorageTraits>{};
        }

        if (info.hints() == arrnd_hint::continuous) {
            return info;
        }

        return arrnd_info<StorageTraits>(info.dims());
    }

    template <typename StorageTraits>
    inline constexpr std::ostream& operator<<(std::ostream& os, const arrnd_info<StorageTraits>& info)
    {
        if (empty(info)) {
            os << "empty";
            return os;
        }

        auto print_vec = [&os](const auto& vec) {
            os << '[';
            std::for_each_n(std::cbegin(vec), std::ssize(vec) - 1, [&os](const auto& e) {
                os << e << ' ';
            });
            os << *std::next(std::cbegin(vec), std::ssize(vec) - 1) << ']';
        };

        os << "total: " << total(info) << '\n';
        os << "dims: ";
        print_vec(info.dims());
        os << '\n';
        os << "strides: ";
        print_vec(info.strides());
        os << '\n';
        os << "indices_boundary: " << info.indices_boundary() << "\n";
        os << "hints (transposed|sliced|continuous): "
           << std::bitset<to_underlying(arrnd_hint::bitscount)>(to_underlying(info.hints())) << "\n";
        os << "props (vector|matrix|row|column|scalar): " << isvector(info) << ismatrix(info) << isrow(info)
           << iscolumn(info) << isscalar(info);

        return os;
    }

}

using details::arrnd_iterator_position;
using details::arrnd_hint;
using details::to_underlying;
using details::arrnd_squeeze_type;
using details::arrnd_info;
using details::slice;
using details::squeeze;
using details::transpose;
using details::swap;
using details::move;
using details::roll;
using details::sub2ind;
using details::ind2sub;
using details::ind2ind;
using details::total;
using details::size;
using details::empty;
using details::iscontinuous;
using details::issliced;
using details::istransposed;
using details::isvector;
using details::ismatrix;
using details::isrow;
using details::iscolumn;
using details::isscalar;
using details::reduce;
using details::unstranspose;
using details::simplify;
}

namespace oc::arrnd {
namespace details {
    template <typename ArrndInfo = arrnd_info<>>
    class arrnd_indexer {
    public:
        using info_type = ArrndInfo;

        using index_type = typename info_type::extent_type;
        using size_type = typename info_type::extent_type;
        using difference_type = typename info_type::extent_storage_type::difference_type;

        using sub_storage_type = typename info_type::extent_storage_type;

        explicit constexpr arrnd_indexer(
            const info_type& info, arrnd_iterator_position start_pos = arrnd_iterator_position::begin)
            : info_(info)
            , subs_(size(info))
            , pos_(empty(info) ? arrnd_iterator_position::end : start_pos)
        {
            if (!empty(info)) {
                if (start_pos == arrnd_iterator_position::begin || start_pos == arrnd_iterator_position::rend) {
                    std::fill(std::begin(subs_), std::end(subs_), index_type{0});
                    curr_abs_ind_ = info.indices_boundary().start();
                    curr_rel_ind_ = index_type{0};
                } else {
                    std::transform(std::begin(info.dims()), std::end(info.dims()), std::begin(subs_), [](auto dim) {
                        return dim - index_type{1};
                    });
                    curr_abs_ind_ = info.indices_boundary().stop() - index_type{1};
                    curr_rel_ind_ = total(info) - index_type{1};
                }
                if (start_pos == arrnd_iterator_position::end) {
                    curr_rel_ind_ = total(info);
                } else if (start_pos == arrnd_iterator_position::rend) {
                    curr_rel_ind_ = index_type{0} - 1;
                }
            }
        }

        constexpr arrnd_indexer() = default;
        constexpr arrnd_indexer(const arrnd_indexer&) = default;
        constexpr arrnd_indexer& operator=(const arrnd_indexer&) = default;
        constexpr arrnd_indexer(arrnd_indexer&&) noexcept = default;
        constexpr arrnd_indexer& operator=(arrnd_indexer&&) noexcept = default;
        constexpr ~arrnd_indexer() = default;

        constexpr arrnd_indexer& operator++() noexcept
        {
            if (pos_ == arrnd_iterator_position::end) {
                return *this;
            }

            if (pos_ == arrnd_iterator_position::rend) {
                ++curr_rel_ind_;
                pos_ = arrnd_iterator_position::begin;
                return *this;
            }

            ++curr_rel_ind_;

            for (index_type i = size(info_); i > 0; --i) {
                ++subs_[i - 1];
                curr_abs_ind_ += info_.strides()[i - 1];
                if (subs_[i - 1] < info_.dims()[i - 1]) {
                    return *this;
                }
                curr_abs_ind_ -= info_.strides()[i - 1] * subs_[i - 1];
                subs_[i - 1] = 0;
            }

            std::transform(std::begin(info_.dims()), std::end(info_.dims()), std::begin(subs_), [](auto dim) {
                return dim - index_type{1};
            });
            curr_abs_ind_ = info_.indices_boundary().stop() - index_type{1};
            curr_rel_ind_ = total(info_);
            pos_ = arrnd_iterator_position::end;

            return *this;
        }

        constexpr arrnd_indexer operator++(int) noexcept
        {
            arrnd_indexer<info_type> temp{*this};
            ++(*this);
            return temp;
        }

        constexpr arrnd_indexer& operator+=(size_type count) noexcept
        {
            for (size_type i = 0; i < count; ++i) {
                ++(*this);
            }
            return *this;
        }

        arrnd_indexer operator+(size_type count) const noexcept
        {
            arrnd_indexer<info_type> temp{*this};
            temp += count;
            return temp;
        }

        constexpr arrnd_indexer& operator--() noexcept
        {
            if (pos_ == arrnd_iterator_position::rend) {
                return *this;
            }

            if (pos_ == arrnd_iterator_position::end) {
                --curr_rel_ind_;
                pos_ = arrnd_iterator_position::rbegin;
                return *this;
            }

            --curr_rel_ind_;

            for (index_type i = size(info_); i > 0; --i) {
                if (subs_[i - 1] >= 1) {
                    --subs_[i - 1];
                    curr_abs_ind_ -= info_.strides()[i - 1];
                    return *this;
                }
                subs_[i - 1] = info_.dims()[i - 1] - index_type{1};
                curr_abs_ind_ += info_.strides()[i - 1] * subs_[i - 1];
            }

            std::fill(std::begin(subs_), std::end(subs_), index_type{0});
            curr_abs_ind_ = info_.indices_boundary().start();
            curr_rel_ind_ = index_type{0} - 1;
            pos_ = arrnd_iterator_position::rend;

            return *this;
        }

        constexpr arrnd_indexer operator--(int) noexcept
        {
            arrnd_indexer<info_type> temp{*this};
            --(*this);
            return temp;
        }

        constexpr arrnd_indexer& operator-=(size_type count) noexcept
        {
            for (size_type i = 0; i < count; ++i) {
                --(*this);
            }
            return *this;
        }

        constexpr arrnd_indexer operator-(size_type count) const noexcept
        {
            arrnd_indexer<info_type> temp{*this};
            temp -= count;
            return temp;
        }

        [[nodiscard]] explicit constexpr operator bool() const noexcept
        {
            return !(pos_ == arrnd_iterator_position::end || pos_ == arrnd_iterator_position::rend);
        }

        [[nodiscard]] constexpr index_type operator*() const noexcept
        {
            return curr_abs_ind_;
        }

        [[nodiscard]] constexpr const sub_storage_type& subs() const noexcept
        {
            return subs_;
        }

        [[nodiscard]] constexpr arrnd_indexer operator[](index_type index) const noexcept
        {
            assert(index >= 0 && index < total(info_));

            if (index > curr_rel_ind_) {
                return *this + (index - curr_rel_ind_);
            }

            if (index < curr_rel_ind_) {
                return *this - (curr_rel_ind_ - index);
            }

            return *this;
        }

        [[nodiscard]] constexpr bool operator==(const arrnd_indexer& other) const noexcept
        {
            return curr_rel_ind_ + 1 == other.curr_rel_ind_ + 1;
        }

        [[nodiscard]] constexpr bool operator<(const arrnd_indexer& other) const noexcept
        {
            return curr_rel_ind_ + 1 < other.curr_rel_ind_ + 1;
        }

        [[nodiscard]] constexpr bool operator<=(const arrnd_indexer& other) const noexcept
        {
            return curr_rel_ind_ + 1 <= other.curr_rel_ind_ + 1;
        }

        [[nodiscard]] constexpr difference_type operator-(const arrnd_indexer& other) const noexcept
        {
            return static_cast<difference_type>(curr_rel_ind_ + 1)
                - static_cast<difference_type>(other.curr_rel_ind_ + 1);
        }

    private:
        info_type info_;

        sub_storage_type subs_;

        index_type curr_abs_ind_{0};
        index_type curr_rel_ind_{0};

        arrnd_iterator_position pos_{arrnd_iterator_position::end};
    };

    enum class arrnd_window_type {
        complete,
        partial,
    };

    template <typename Interval>
        requires(std::signed_integral<typename Interval::size_type>)
    struct arrnd_window {
        using interval_type = Interval;

        explicit constexpr arrnd_window(interval_type nival, arrnd_window_type ntype = arrnd_window_type::complete)
            : ival(nival)
            , type(ntype)
        { }

        constexpr arrnd_window() = default;
        constexpr arrnd_window(const arrnd_window&) = default;
        constexpr arrnd_window& operator=(const arrnd_window&) = default;
        constexpr arrnd_window(arrnd_window&&) noexcept = default;
        constexpr arrnd_window& operator=(arrnd_window&&) noexcept = default;
        constexpr ~arrnd_window() = default;

        interval_type ival;
        arrnd_window_type type{arrnd_window_type::complete};
    };

    template <typename ArrndInfo = arrnd_info<>>
    class arrnd_windows_slider {
    public:
        using info_type = ArrndInfo;

        using index_type = typename info_type::extent_type;
        using size_type = typename info_type::extent_type;

        using boundary_storage_type = typename info_type::storage_traits_type::template replaced_type<
            typename info_type::boundary_type>::storage_type;

        using window_type = arrnd_window<interval<std::make_signed_t<typename info_type::extent_type>>>;

        template <typename InputIt>
            requires(template_type<iterator_value_t<InputIt>, arrnd_window>)
        explicit constexpr arrnd_windows_slider(const info_type& info, InputIt first_window, InputIt last_window,
            arrnd_iterator_position start_pos = arrnd_iterator_position::begin)
            : info_(info)
            , windows_(size(info))
            , curr_boundaries_(size(info))
        {
            if (size(info) < std::distance(first_window, last_window)) {
                throw std::invalid_argument("invalid window size - bigger than number of dims");
            }

            if (std::any_of(first_window, last_window, [](auto window) {
                    return isunbound(window.ival) && (isleftbound(window.ival) || isrightbound(window.ival));
                })) {
                throw std::invalid_argument("invalid window interval - half bounded intervals currently not supported");
            }

            if (!std::transform_reduce(std::begin(info.dims()),
                    std::next(std::begin(info.dims()), std::distance(first_window, last_window)), first_window, true,
                    std::logical_and<>{}, [](auto dim, auto window) {
                        auto ival = window.ival;
                        auto type = window.type;
                        return (type == arrnd_window_type::complete
                                   && (isunbound(ival)
                                       || (ival.start() <= 0 && ival.stop() >= 0 && absdiff(ival) <= dim)))
                            || type == arrnd_window_type::partial;
                    })) {
                throw std::invalid_argument("invalid window interval");
            }

            if constexpr (std::is_same_v<std::iter_value_t<InputIt>, typename window_type::interval_type>) {
                std::copy(first_window, last_window, std::begin(windows_));
            } else {
                std::transform(first_window, last_window, std::begin(windows_), [](auto window) {
                    return window_type(static_cast<typename window_type::interval_type>(window.ival), window.type);
                });
            }
            if (size(info) > std::distance(first_window, last_window)) {
                std::transform(std::next(std::begin(info.dims()), std::distance(first_window, last_window)),
                    std::end(info.dims()), std::next(std::begin(windows_), std::distance(first_window, last_window)),
                    [](auto dim) {
                        return window_type(typename window_type::interval_type(0, static_cast<size_type>(dim)),
                            arrnd_window_type::complete);
                    });
            }

            typename info_type::extent_storage_type indexer_dims(size(info));
            std::transform(std::begin(info.dims()), std::end(info.dims()), std::begin(windows_),
                std::begin(indexer_dims), [](auto dim, auto window) {
                    auto ival = window.ival;
                    if (isunbound(window.ival)) {
                        return index_type{1};
                    }
                    return window.type == arrnd_window_type::complete ? dim - absdiff(ival) + 1 : dim;
                });

            indexer_ = arrnd_indexer(info_type(indexer_dims), start_pos);

            for (size_type i = 0; i < size(info); ++i) {
                curr_boundaries_[i] = window2boundary(windows_[i], indexer_.subs()[i], info.dims()[i]);
            }
        }

        template <iterable_type Cont>
            requires(template_type<iterable_value_t<Cont>, arrnd_window>)
        explicit constexpr arrnd_windows_slider(
            const info_type& info, Cont&& windows, arrnd_iterator_position start_pos = arrnd_iterator_position::begin)
            : arrnd_windows_slider(info, std::begin(windows), std::end(windows), start_pos)
        { }

        explicit constexpr arrnd_windows_slider(const info_type& info, std::initializer_list<window_type> windows,
            arrnd_iterator_position start_pos = arrnd_iterator_position::begin)
            : arrnd_windows_slider(info, windows.begin(), windows.end(), start_pos)
        { }

        explicit constexpr arrnd_windows_slider(const info_type& info, index_type axis,
            const window_type& window = window_type(typename window_type::interval_type(0, 1)),
            arrnd_iterator_position start_pos = arrnd_iterator_position::begin)
        {
            if (axis >= size(info)) {
                throw std::out_of_range("invalid axis");
            }

            typename info_type::storage_traits_type::template replaced_type<window_type>::storage_type windows(
                size(info));

            std::transform(std::begin(info.dims()), std::end(info.dims()), std::begin(windows), [](auto dim) {
                return window_type(
                    typename window_type::interval_type(0, static_cast<size_type>(dim)), arrnd_window_type::complete);
            });

            windows[axis] = window;

            *this = arrnd_windows_slider(info, windows, start_pos);
        }

        constexpr arrnd_windows_slider() = default;
        constexpr arrnd_windows_slider(const arrnd_windows_slider&) = default;
        constexpr arrnd_windows_slider& operator=(const arrnd_windows_slider&) = default;
        constexpr arrnd_windows_slider(arrnd_windows_slider&&) noexcept = default;
        constexpr arrnd_windows_slider& operator=(arrnd_windows_slider&&) noexcept = default;
        constexpr ~arrnd_windows_slider() = default;

        constexpr arrnd_windows_slider& operator++() noexcept
        {
            ++indexer_;

            for (size_type i = 0; i < size(info_); ++i) {
                curr_boundaries_[i] = window2boundary(windows_[i], indexer_.subs()[i], info_.dims()[i]);
            }

            return *this;
        }

        constexpr arrnd_windows_slider operator++(int) noexcept
        {
            arrnd_windows_slider<info_type> temp{*this};
            ++(*this);
            return temp;
        }

        constexpr arrnd_windows_slider& operator+=(size_type count) noexcept
        {
            for (size_type i = 0; i < count; ++i) {
                ++(*this);
            }
            return *this;
        }

        arrnd_windows_slider operator+(size_type count) const noexcept
        {
            arrnd_windows_slider<info_type> temp{*this};
            temp += count;
            return temp;
        }

        constexpr arrnd_windows_slider& operator--() noexcept
        {
            --indexer_;

            for (size_type i = 0; i < size(info_); ++i) {
                curr_boundaries_[i] = window2boundary(windows_[i], indexer_.subs()[i], info_.dims()[i]);
            }

            return *this;
        }

        constexpr arrnd_windows_slider operator--(int) noexcept
        {
            arrnd_windows_slider<info_type> temp{*this};
            --(*this);
            return temp;
        }

        constexpr arrnd_windows_slider& operator-=(size_type count) noexcept
        {
            for (size_type i = 0; i < count; ++i) {
                --(*this);
            }
            return *this;
        }

        constexpr arrnd_windows_slider operator-(size_type count) const noexcept
        {
            arrnd_windows_slider<info_type> temp{*this};
            temp -= count;
            return temp;
        }

        [[nodiscard]] explicit constexpr operator bool() const noexcept
        {
            return static_cast<bool>(indexer_);
        }

        [[nodiscard]] constexpr const boundary_storage_type& operator*() const noexcept
        {
            return curr_boundaries_;
        }

        [[nodiscard]] constexpr arrnd_windows_slider operator[](index_type index) const noexcept
        {
            auto subs = indexer_[index].subs();

            arrnd_windows_slider<info_type> temp{*this};

            for (size_type i = 0; i < size(info_); ++i) {
                temp.curr_boundaries_[i] = window2boundary(windows_[i], subs[i], info_.dims()[i]);
            }

            return temp;
        }

        [[nodiscard]] constexpr bool operator==(const arrnd_windows_slider& other) const noexcept
        {
            return indexer_ == other.indexer_;
        }

        [[nodiscard]] constexpr bool operator<(const arrnd_windows_slider& other) const noexcept
        {
            return indexer_ < other.indexer_;
        }

        [[nodiscard]] constexpr bool operator<=(const arrnd_windows_slider& other) const noexcept
        {
            return indexer_ <= other.indexer_;
        }

        [[nodiscard]] constexpr auto operator-(const arrnd_windows_slider& other) const noexcept
        {
            return indexer_ - other.indexer_;
        }

        // not so safe function, and should be used with consideration of the internal indexer.
        constexpr void modify_window(index_type axis, window_type window)
        {
            if (axis >= size(info_)) {
                throw std::out_of_range("invalid axis");
            }

            const auto& ival = window.ival;

            if (isunbound(ival) && (isleftbound(ival) || isrightbound(ival))) {
                throw std::invalid_argument("invalid window interval - half bounded intervals currently not supported");
            }

            if (!(isunbound(ival) || (ival.start() <= 0 && ival.stop() >= 0 && absdiff(ival) <= info_.dims()[axis]))) {
                throw std::invalid_argument("invalid window interval");
            }

            windows_[axis] = window;

            curr_boundaries_[axis] = window2boundary(windows_[axis], indexer_.subs()[axis], info_.dims()[axis]);
        }

    private:
        [[nodiscard]] typename info_type::boundary_type window2boundary(
            window_type window, index_type sub, size_type dim) const
        {
            const auto& ival = window.ival;
            auto type = window.type;

            if (isunbound(ival)) {
                // unsupported half bound intervals checked in class constructor.
                return typename info_type::boundary_type(
                    0, static_cast<index_type>(dim), static_cast<index_type>(ival.step()));
            }

            if (type == arrnd_window_type::complete) {
                return typename info_type::boundary_type(
                    sub, sub + static_cast<index_type>(absdiff(ival)), static_cast<index_type>(ival.step()));
            }

            auto before = static_cast<index_type>(std::abs(ival.start()));
            auto nstart = (sub < before ? 0 : sub - before);

            auto after = static_cast<index_type>(ival.stop());
            auto nstop = (sub + after > dim ? dim : sub + after);

            return typename info_type::boundary_type(nstart, nstop, static_cast<index_type>(ival.step()));
        }

        info_type info_;

        typename info_type::storage_traits_type::template replaced_type<window_type>::storage_type windows_;
        arrnd_indexer<info_type> indexer_;

        boundary_storage_type curr_boundaries_;
    };
}

using details::arrnd_indexer;
using details::arrnd_window_type;
using details::arrnd_window;
using details::arrnd_windows_slider;
}

// arrnd iterator types
namespace oc::arrnd {
namespace details {
    struct arrnd_returned_element_iterator_tag { };
    struct arrnd_returned_slice_iterator_tag { };

    template <typename Arrnd>
    class arrnd_const_iterator;

    template <typename Arrnd>
    class arrnd_iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = typename Arrnd::difference_type;
        using value_type = typename Arrnd::value_type;
        using pointer = typename Arrnd::value_type*;
        using reference = typename Arrnd::value_type&;

        using indexer_type = typename Arrnd::indexer_type;

        friend class arrnd_const_iterator<Arrnd>;

        explicit constexpr arrnd_iterator(pointer data, const indexer_type& indexer)
            : indexer_(indexer)
            , data_(data)
        { }

        constexpr arrnd_iterator() = default;

        constexpr arrnd_iterator(const arrnd_iterator& other) = default;
        constexpr arrnd_iterator& operator=(const arrnd_iterator& other) = default;

        constexpr arrnd_iterator(arrnd_iterator&& other) noexcept = default;
        constexpr arrnd_iterator& operator=(arrnd_iterator&& other) noexcept = default;

        constexpr ~arrnd_iterator() = default;

        constexpr arrnd_iterator& operator++() noexcept
        {
            ++indexer_;
            return *this;
        }

        constexpr arrnd_iterator operator++(int) noexcept
        {
            arrnd_iterator temp{*this};
            ++(*this);
            return temp;
        }

        constexpr arrnd_iterator& operator+=(difference_type count) noexcept
        {
            indexer_ += count;
            return *this;
        }

        constexpr arrnd_iterator operator+(difference_type count) const noexcept
        {
            arrnd_iterator temp{*this};
            temp += count;
            return temp;
        }

        constexpr arrnd_iterator& operator--() noexcept
        {
            --indexer_;
            return *this;
        }

        constexpr arrnd_iterator operator--(int) noexcept
        {
            arrnd_iterator temp{*this};
            --(*this);
            return temp;
        }

        constexpr arrnd_iterator& operator-=(difference_type count) noexcept
        {
            indexer_ -= count;
            return *this;
        }

        constexpr arrnd_iterator operator-(difference_type count) const noexcept
        {
            arrnd_iterator temp{*this};
            temp -= count;
            return temp;
        }

        [[nodiscard]] constexpr reference operator*() const noexcept
        {
            return data_[*indexer_];
        }

        [[nodiscard]] constexpr bool operator==(const arrnd_iterator& iter) const noexcept
        {
            return indexer_ == iter.indexer_;
        }

        [[nodiscard]] constexpr bool operator<(const arrnd_iterator& iter) const noexcept
        {
            return indexer_ < iter.indexer_;
        }

        [[nodiscard]] constexpr bool operator<=(const arrnd_iterator& iter) const noexcept
        {
            return indexer_ <= iter.indexer_;
        }

        [[nodiscard]] constexpr reference operator[](difference_type index) const noexcept
        {
            return data_[indexer_[index]];
        }

        [[nodiscard]] constexpr difference_type operator-(const arrnd_iterator& other) const noexcept
        {
            return indexer_ - other.indexer_;
        }

    private:
        indexer_type indexer_;
        pointer data_ = nullptr;
    };

    template <typename Arrnd>
    class arrnd_const_iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = typename Arrnd::difference_type;
        using value_type = typename Arrnd::value_type;
        using pointer = typename Arrnd::value_type*;
        using reference = typename Arrnd::value_type&;

        using indexer_type = typename Arrnd::indexer_type;

        explicit constexpr arrnd_const_iterator(pointer data, const indexer_type& indexer)
            : indexer_(indexer)
            , data_(data)
        { }

        constexpr arrnd_const_iterator() = default;

        constexpr arrnd_const_iterator(const arrnd_const_iterator& other) = default;
        constexpr arrnd_const_iterator& operator=(const arrnd_const_iterator& other) = default;

        constexpr arrnd_const_iterator(arrnd_const_iterator&& other) noexcept = default;
        constexpr arrnd_const_iterator& operator=(arrnd_const_iterator&& other) noexcept = default;

        constexpr ~arrnd_const_iterator() = default;

        constexpr arrnd_const_iterator(const arrnd_iterator<Arrnd>& other)
            : indexer_(other.indexer_)
            , data_(other.data_)
        { }
        constexpr arrnd_const_iterator& operator=(const arrnd_iterator<Arrnd>& other)
        {
            indexer_ = other.indexer_;
            data_ = other.data_;
            return *this;
        }

        constexpr arrnd_const_iterator(arrnd_iterator<Arrnd>&& other)
            : indexer_(std::move(other.indexer_))
            , data_(std::move(other.data_))
        { }
        constexpr arrnd_const_iterator& operator=(arrnd_iterator<Arrnd>&& other)
        {
            indexer_ = std::move(other.indexer_);
            data_ = std::move(other.data_);
            return *this;
        }

        constexpr arrnd_const_iterator& operator++() noexcept
        {
            ++indexer_;
            return *this;
        }

        constexpr arrnd_const_iterator operator++(int) noexcept
        {
            arrnd_const_iterator temp{*this};
            ++(*this);
            return temp;
        }

        constexpr arrnd_const_iterator& operator+=(difference_type count) noexcept
        {
            indexer_ += count;
            return *this;
        }

        constexpr arrnd_const_iterator operator+(difference_type count) const noexcept
        {
            arrnd_const_iterator temp{*this};
            temp += count;
            return temp;
        }

        constexpr arrnd_const_iterator& operator--() noexcept
        {
            --indexer_;
            return *this;
        }

        constexpr arrnd_const_iterator operator--(int) noexcept
        {
            arrnd_const_iterator temp{*this};
            --(*this);
            return temp;
        }

        constexpr arrnd_const_iterator& operator-=(difference_type count) noexcept
        {
            indexer_ -= count;
            return *this;
        }

        constexpr arrnd_const_iterator operator-(difference_type count) const noexcept
        {
            arrnd_const_iterator temp{*this};
            temp -= count;
            return temp;
        }

        [[nodiscard]] constexpr const reference operator*() const noexcept
        {
            return data_[*indexer_];
        }

        [[nodiscard]] constexpr bool operator==(const arrnd_const_iterator& iter) const noexcept
        {
            return indexer_ == iter.indexer_;
        }

        [[nodiscard]] constexpr bool operator<(const arrnd_const_iterator& iter) const noexcept
        {
            return indexer_ < iter.indexer_;
        }

        [[nodiscard]] constexpr bool operator<=(const arrnd_const_iterator& iter) const noexcept
        {
            return indexer_ <= iter.indexer_;
        }

        [[nodiscard]] constexpr const reference operator[](difference_type index) const noexcept
        {
            return data_[indexer_[index]];
        }

        [[nodiscard]] constexpr difference_type operator-(const arrnd_const_iterator& other) const noexcept
        {
            return indexer_ - other.indexer_;
        }

    private:
        indexer_type indexer_;
        pointer data_ = nullptr;
    };

    template <typename Arrnd>
    class arrnd_const_reverse_iterator;

    template <typename Arrnd>
    class arrnd_reverse_iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = typename Arrnd::difference_type;
        using value_type = typename Arrnd::value_type;
        using pointer = typename Arrnd::value_type*;
        using reference = typename Arrnd::value_type&;

        using indexer_type = typename Arrnd::indexer_type;

        friend arrnd_const_reverse_iterator<Arrnd>;

        explicit constexpr arrnd_reverse_iterator(pointer data, const indexer_type& indexer)
            : indexer_(indexer)
            , data_(data)
        { }

        constexpr arrnd_reverse_iterator() = default;

        constexpr arrnd_reverse_iterator(const arrnd_reverse_iterator& other) = default;
        constexpr arrnd_reverse_iterator& operator=(const arrnd_reverse_iterator& other) = default;

        constexpr arrnd_reverse_iterator(arrnd_reverse_iterator&& other) noexcept = default;
        constexpr arrnd_reverse_iterator& operator=(arrnd_reverse_iterator&& other) noexcept = default;

        constexpr ~arrnd_reverse_iterator() = default;

        constexpr arrnd_reverse_iterator& operator++() noexcept
        {
            --indexer_;
            return *this;
        }

        constexpr arrnd_reverse_iterator operator++(int) noexcept
        {
            arrnd_reverse_iterator temp{*this};
            --(*this);
            return temp;
        }

        constexpr arrnd_reverse_iterator& operator+=(difference_type count) noexcept
        {
            indexer_ -= count;
            return *this;
        }

        constexpr arrnd_reverse_iterator operator+(difference_type count) const noexcept
        {
            arrnd_reverse_iterator temp{*this};
            temp += count;
            return temp;
        }

        constexpr arrnd_reverse_iterator& operator--() noexcept
        {
            ++indexer_;
            return *this;
        }

        constexpr arrnd_reverse_iterator operator--(int) noexcept
        {
            arrnd_reverse_iterator temp{*this};
            ++(*this);
            return temp;
        }

        constexpr arrnd_reverse_iterator& operator-=(difference_type count) noexcept
        {
            indexer_ += count;
            return *this;
        }

        constexpr arrnd_reverse_iterator operator-(difference_type count) const noexcept
        {
            arrnd_reverse_iterator temp{*this};
            temp -= count;
            return temp;
        }

        [[nodiscard]] constexpr reference operator*() const noexcept
        {
            return data_[*indexer_];
        }

        [[nodiscard]] constexpr bool operator==(const arrnd_reverse_iterator& iter) const noexcept
        {
            return indexer_ == iter.indexer_;
        }

        [[nodiscard]] constexpr bool operator<(const arrnd_reverse_iterator& iter) const noexcept
        {
            return iter.indexer_ < indexer_;
        }

        [[nodiscard]] constexpr bool operator<=(const arrnd_reverse_iterator& iter) const noexcept
        {
            return iter.indexer_ <= indexer_;
        }

        [[nodiscard]] constexpr reference operator[](difference_type index) const noexcept
        {
            return data_[indexer_[index]];
        }

        [[nodiscard]] constexpr difference_type operator-(const arrnd_reverse_iterator& other) const noexcept
        {
            return other.indexer_ - indexer_;
        }

    private:
        indexer_type indexer_;
        pointer data_ = nullptr;
    };

    template <typename Arrnd>
    class arrnd_const_reverse_iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = typename Arrnd::difference_type;
        using value_type = typename Arrnd::value_type;
        using pointer = typename Arrnd::value_type*;
        using reference = typename Arrnd::value_type&;

        using indexer_type = typename Arrnd::indexer_type;

        explicit constexpr arrnd_const_reverse_iterator(pointer data, const indexer_type& indexer)
            : indexer_(indexer)
            , data_(data)
        { }

        constexpr arrnd_const_reverse_iterator() = default;

        constexpr arrnd_const_reverse_iterator(const arrnd_const_reverse_iterator& other) = default;
        constexpr arrnd_const_reverse_iterator& operator=(const arrnd_const_reverse_iterator& other) = default;

        constexpr arrnd_const_reverse_iterator(arrnd_const_reverse_iterator&& other) noexcept = default;
        constexpr arrnd_const_reverse_iterator& operator=(arrnd_const_reverse_iterator&& other) noexcept = default;

        constexpr ~arrnd_const_reverse_iterator() = default;

        constexpr arrnd_const_reverse_iterator(const arrnd_reverse_iterator<Arrnd>& other)
            : indexer_(other.indexer_)
            , data_(other.data_)
        { }
        constexpr arrnd_const_reverse_iterator& operator=(const arrnd_reverse_iterator<Arrnd>& other)
        {
            indexer_ = other.indexer_;
            data_ = other.data_;
            return *this;
        }

        constexpr arrnd_const_reverse_iterator(arrnd_reverse_iterator<Arrnd>&& other)
            : indexer_(std::move(other.indexer_))
            , data_(std::move(other.data_))
        { }
        constexpr arrnd_const_reverse_iterator& operator=(arrnd_reverse_iterator<Arrnd>&& other)
        {
            indexer_ = std::move(other.indexer_);
            data_ = std::move(other.data_);
            return *this;
        }

        constexpr arrnd_const_reverse_iterator& operator++() noexcept
        {
            --indexer_;
            return *this;
        }

        constexpr arrnd_const_reverse_iterator operator++(int) noexcept
        {
            arrnd_const_reverse_iterator temp{*this};
            --(*this);
            return temp;
        }

        constexpr arrnd_const_reverse_iterator& operator+=(difference_type count) noexcept
        {
            indexer_ -= count;
            return *this;
        }

        constexpr arrnd_const_reverse_iterator operator+(difference_type count) const noexcept
        {
            arrnd_const_reverse_iterator temp{*this};
            temp += count;
            return temp;
        }

        constexpr arrnd_const_reverse_iterator& operator--() noexcept
        {
            ++indexer_;
            return *this;
        }

        constexpr arrnd_const_reverse_iterator operator--(int) noexcept
        {
            arrnd_const_reverse_iterator temp{*this};
            ++(*this);
            return temp;
        }

        constexpr arrnd_const_reverse_iterator& operator-=(difference_type count) noexcept
        {
            indexer_ += count;
            return *this;
        }

        constexpr arrnd_const_reverse_iterator operator-(difference_type count) const noexcept
        {
            arrnd_const_reverse_iterator temp{*this};
            temp -= count;
            return temp;
        }

        [[nodiscard]] constexpr const reference operator*() const noexcept
        {
            return data_[*indexer_];
        }

        [[nodiscard]] constexpr bool operator==(const arrnd_const_reverse_iterator& iter) const noexcept
        {
            return indexer_ == iter.indexer_;
        }

        [[nodiscard]] constexpr bool operator<(const arrnd_const_reverse_iterator& iter) const noexcept
        {
            return iter.indexer_ < indexer_;
        }

        [[nodiscard]] constexpr bool operator<=(const arrnd_const_reverse_iterator& iter) const noexcept
        {
            return iter.indexer_ <= indexer_;
        }

        [[nodiscard]] constexpr const reference operator[](difference_type index) const noexcept
        {
            return data_[indexer_[index]];
        }

        [[nodiscard]] constexpr difference_type operator-(const arrnd_const_reverse_iterator& other) const noexcept
        {
            return other.indexer_ - indexer_;
        }

    private:
        indexer_type indexer_;
        pointer data_ = nullptr;
    };

    template <typename Arrnd>
    class arrnd_slice_const_iterator;

    template <typename Arrnd>
    class arrnd_slice_iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = typename Arrnd::difference_type;
        using value_type = Arrnd;
        using reference = Arrnd&;

        using windows_slider_type = typename Arrnd::windows_slider_type;

        friend arrnd_slice_const_iterator<Arrnd>;

        explicit constexpr arrnd_slice_iterator(const value_type& arrnd_ref, const windows_slider_type& far)
            : arrnd_ref_(arrnd_ref)
            , far_(far)
        {
            if (far) {
                slice_ = arrnd_ref[std::make_pair((*far_).cbegin(), (*far_).cend())];
            }
        }

        constexpr arrnd_slice_iterator() = default;

        constexpr arrnd_slice_iterator(const arrnd_slice_iterator& other) = default;
        constexpr arrnd_slice_iterator& operator=(const arrnd_slice_iterator& other) = default;

        constexpr arrnd_slice_iterator(arrnd_slice_iterator&& other) noexcept = default;
        constexpr arrnd_slice_iterator& operator=(arrnd_slice_iterator&& other) noexcept = default;

        constexpr ~arrnd_slice_iterator() = default;

        constexpr arrnd_slice_iterator& operator++() noexcept
        {
            ++far_;
            return *this;
        }

        constexpr arrnd_slice_iterator operator++(int) noexcept
        {
            arrnd_slice_iterator temp{*this};
            ++(*this);
            return temp;
        }

        constexpr arrnd_slice_iterator& operator+=(difference_type count) noexcept
        {
            far_ += count;
            return *this;
        }

        constexpr arrnd_slice_iterator operator+(difference_type count) const
        {
            arrnd_slice_iterator temp{*this};
            temp += count;
            return temp;
        }

        constexpr arrnd_slice_iterator& operator--() noexcept
        {
            --far_;
            return *this;
        }

        constexpr arrnd_slice_iterator operator--(int) noexcept
        {
            arrnd_slice_iterator temp{*this};
            --(*this);
            return temp;
        }

        constexpr arrnd_slice_iterator& operator-=(difference_type count) noexcept
        {
            far_ -= count;
            return *this;
        }

        constexpr arrnd_slice_iterator operator-(difference_type count) const noexcept
        {
            arrnd_slice_iterator temp{*this};
            temp -= count;
            return temp;
        }

        [[nodiscard]] constexpr reference operator*() const noexcept
        {
            slice_ = arrnd_ref_[std::make_pair((*far_).cbegin(), (*far_).cend())];
            return slice_;
        }

        [[nodiscard]] constexpr bool operator==(const arrnd_slice_iterator& iter) const noexcept
        {
            return far_ == iter.far_;
        }

        [[nodiscard]] constexpr bool operator<(const arrnd_slice_iterator& iter) const noexcept
        {
            return far_ < iter.far_;
        }

        [[nodiscard]] constexpr bool operator<=(const arrnd_slice_iterator& iter) const noexcept
        {
            return far_ <= iter.far_;
        }

        [[nodiscard]] constexpr reference operator[](difference_type index) const noexcept
        {
            auto ranges = far_[index];
            return arrnd_ref_[std::make_pair(ranges.cbegin(), ranges.cend())];
        }

        [[nodiscard]] constexpr difference_type operator-(const arrnd_slice_iterator& other) const noexcept
        {
            return far_ - other.far_;
        }

    private:
        value_type arrnd_ref_;
        windows_slider_type far_;

        mutable value_type slice_;
    };

    template <typename Arrnd>
    class arrnd_slice_const_iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = typename Arrnd::difference_type;
        using value_type = Arrnd;
        using const_reference = const Arrnd&;

        using windows_slider_type = typename Arrnd::windows_slider_type;

        explicit constexpr arrnd_slice_const_iterator(const value_type& arrnd_ref, const windows_slider_type& far)
            : arrnd_ref_(arrnd_ref)
            , far_(far)
        {
            if (far) {
                slice_ = arrnd_ref[std::make_pair((*far_).cbegin(), (*far_).cend())];
            }
        }

        constexpr arrnd_slice_const_iterator() = default;

        constexpr arrnd_slice_const_iterator(const arrnd_slice_const_iterator& other) = default;
        constexpr arrnd_slice_const_iterator& operator=(const arrnd_slice_const_iterator& other) = default;

        constexpr arrnd_slice_const_iterator(arrnd_slice_const_iterator&& other) noexcept = default;
        constexpr arrnd_slice_const_iterator& operator=(arrnd_slice_const_iterator&& other) noexcept = default;

        constexpr ~arrnd_slice_const_iterator() = default;

        constexpr arrnd_slice_const_iterator(const arrnd_slice_iterator<Arrnd>& other)
            : arrnd_ref_(other.arrnd_ref_)
            , far_(other.far_)
            , slice_(other.slice_)
        { }
        constexpr arrnd_slice_const_iterator& operator=(const arrnd_slice_iterator<Arrnd>& other)
        {
            arrnd_ref_ = other.arrnd_ref_;
            far_ = other.far_;
            slice_ = other.slice_;
            return *this;
        }

        constexpr arrnd_slice_const_iterator(arrnd_slice_iterator<Arrnd>&& other)
            : arrnd_ref_(std::move(other.arrnd_ref_))
            , far_(std::move(other.far_))
            , slice_(std::move(other.slice_))
        { }
        constexpr arrnd_slice_const_iterator& operator=(arrnd_slice_iterator<Arrnd>&& other)
        {
            arrnd_ref_ = std::move(other.arrnd_ref_);
            far_ = std::move(other.far_);
            slice_ = std::move(other.slice_);
            return *this;
        }

        constexpr arrnd_slice_const_iterator& operator++() noexcept
        {
            ++far_;
            return *this;
        }

        constexpr arrnd_slice_const_iterator operator++(int) noexcept
        {
            arrnd_slice_const_iterator temp{*this};
            ++(*this);
            return temp;
        }

        constexpr arrnd_slice_const_iterator& operator+=(difference_type count) noexcept
        {
            far_ += count;
            return *this;
        }

        constexpr arrnd_slice_const_iterator operator+(difference_type count) const noexcept
        {
            arrnd_slice_const_iterator temp{*this};
            temp += count;
            return temp;
        }

        constexpr arrnd_slice_const_iterator& operator--() noexcept
        {
            --far_;
            return *this;
        }

        constexpr arrnd_slice_const_iterator operator--(int) noexcept
        {
            arrnd_slice_const_iterator temp{*this};
            --(*this);
            return temp;
        }

        constexpr arrnd_slice_const_iterator& operator-=(difference_type count) noexcept
        {
            far_ -= count;
            return *this;
        }

        constexpr arrnd_slice_const_iterator operator-(difference_type count) const noexcept
        {
            arrnd_slice_const_iterator temp{*this};
            temp -= count;
            return temp;
        }

        [[nodiscard]] constexpr const_reference operator*() const noexcept
        {
            slice_ = arrnd_ref_[std::make_pair((*far_).cbegin(), (*far_).cend())];
            return slice_;
        }

        [[nodiscard]] constexpr bool operator==(const arrnd_slice_const_iterator& iter) const noexcept
        {
            return far_ == iter.far_;
        }

        [[nodiscard]] constexpr bool operator<(const arrnd_slice_const_iterator& iter) const noexcept
        {
            return far_ < iter.far_;
        }

        [[nodiscard]] constexpr bool operator<=(const arrnd_slice_const_iterator& iter) const noexcept
        {
            return far_ <= iter.far_;
        }

        [[nodiscard]] constexpr const_reference operator[](difference_type index) const noexcept
        {
            auto ranges = far_[index];
            return arrnd_ref_[std::make_pair(ranges.cbegin(), ranges.cend())];
        }

        [[nodiscard]] constexpr difference_type operator-(const arrnd_slice_const_iterator& other) const noexcept
        {
            return far_ - other.far_;
        }

    private:
        value_type arrnd_ref_;
        windows_slider_type far_;

        mutable value_type slice_;
    };

    template <typename Arrnd>
    class arrnd_slice_reverse_const_iterator;

    template <typename Arrnd>
    class arrnd_slice_reverse_iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = typename Arrnd::difference_type;
        using value_type = Arrnd;
        using reference = Arrnd&;

        using windows_slider_type = typename Arrnd::windows_slider_type;

        friend arrnd_slice_reverse_const_iterator<Arrnd>;

        explicit constexpr arrnd_slice_reverse_iterator(const value_type& arrnd_ref, const windows_slider_type& far)
            : arrnd_ref_(arrnd_ref)
            , far_(far)
        {
            if (far) {
                slice_ = arrnd_ref[std::make_pair((*far_).cbegin(), (*far_).cend())];
            }
        }

        constexpr arrnd_slice_reverse_iterator() = default;

        constexpr arrnd_slice_reverse_iterator(const arrnd_slice_reverse_iterator& other) = default;
        constexpr arrnd_slice_reverse_iterator& operator=(const arrnd_slice_reverse_iterator& other) = default;

        constexpr arrnd_slice_reverse_iterator(arrnd_slice_reverse_iterator&& other) noexcept = default;
        constexpr arrnd_slice_reverse_iterator& operator=(arrnd_slice_reverse_iterator&& other) noexcept = default;

        constexpr ~arrnd_slice_reverse_iterator() = default;

        constexpr arrnd_slice_reverse_iterator& operator--() noexcept
        {
            ++far_;
            return *this;
        }

        constexpr arrnd_slice_reverse_iterator operator--(int) noexcept
        {
            arrnd_slice_reverse_iterator temp{*this};
            ++(*this);
            return temp;
        }

        constexpr arrnd_slice_reverse_iterator& operator-=(difference_type count) noexcept
        {
            far_ += count;
            return *this;
        }

        constexpr arrnd_slice_reverse_iterator operator-(difference_type count) const noexcept
        {
            arrnd_slice_reverse_iterator temp{*this};
            temp += count;
            return temp;
        }

        constexpr arrnd_slice_reverse_iterator& operator++() noexcept
        {
            --far_;
            return *this;
        }

        constexpr arrnd_slice_reverse_iterator operator++(int) noexcept
        {
            arrnd_slice_reverse_iterator temp{*this};
            --(*this);
            return temp;
        }

        constexpr arrnd_slice_reverse_iterator& operator+=(difference_type count) noexcept
        {
            far_ -= count;
            return *this;
        }

        constexpr arrnd_slice_reverse_iterator operator+(difference_type count) const noexcept
        {
            arrnd_slice_reverse_iterator temp{*this};
            temp -= count;
            return temp;
        }

        [[nodiscard]] constexpr reference operator*() const noexcept
        {
            slice_ = arrnd_ref_[std::make_pair((*far_).cbegin(), (*far_).cend())];
            return slice_;
        }

        [[nodiscard]] constexpr bool operator==(const arrnd_slice_reverse_iterator& iter) const noexcept
        {
            return far_ == iter.far_;
        }

        [[nodiscard]] constexpr bool operator<(const arrnd_slice_reverse_iterator& iter) const noexcept
        {
            return far_ > iter.far_;
        }

        [[nodiscard]] constexpr bool operator<=(const arrnd_slice_reverse_iterator& iter) const noexcept
        {
            return far_ >= iter.far_;
        }

        [[nodiscard]] constexpr reference operator[](difference_type index) const noexcept
        {
            auto ranges = far_[index];
            return arrnd_ref_[std::make_pair(ranges.cbegin(), ranges.cend())];
        }

        [[nodiscard]] constexpr difference_type operator-(const arrnd_slice_reverse_iterator& other) const noexcept
        {
            return far_ - other.far_;
        }

    private:
        value_type arrnd_ref_;
        windows_slider_type far_;

        mutable value_type slice_;
    };

    template <typename Arrnd>
    class arrnd_slice_reverse_const_iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = typename Arrnd::difference_type;
        using value_type = Arrnd;
        using const_reference = const Arrnd&;

        using windows_slider_type = typename Arrnd::windows_slider_type;

        explicit constexpr arrnd_slice_reverse_const_iterator(
            const value_type& arrnd_ref, const windows_slider_type& far)
            : arrnd_ref_(arrnd_ref)
            , far_(far)
        {
            if (far) {
                slice_ = arrnd_ref[std::make_pair((*far_).cbegin(), (*far_).cend())];
            }
        }

        constexpr arrnd_slice_reverse_const_iterator() = default;

        constexpr arrnd_slice_reverse_const_iterator(const arrnd_slice_reverse_const_iterator& other) = default;
        constexpr arrnd_slice_reverse_const_iterator& operator=(const arrnd_slice_reverse_const_iterator& other)
            = default;

        constexpr arrnd_slice_reverse_const_iterator(arrnd_slice_reverse_const_iterator&& other) noexcept = default;
        constexpr arrnd_slice_reverse_const_iterator& operator=(arrnd_slice_reverse_const_iterator&& other) noexcept
            = default;

        constexpr ~arrnd_slice_reverse_const_iterator() = default;

        constexpr arrnd_slice_reverse_const_iterator(const arrnd_slice_reverse_iterator<Arrnd>& other)
            : arrnd_ref_(other.arrnd_ref_)
            , far_(other.far_)
            , slice_(other.slice_)
        { }
        constexpr arrnd_slice_reverse_const_iterator& operator=(const arrnd_slice_reverse_iterator<Arrnd>& other)
        {
            arrnd_ref_ = other.arrnd_ref_;
            far_ = other.far_;
            slice_ = other.slice_;
            return *this;
        }

        constexpr arrnd_slice_reverse_const_iterator(arrnd_slice_reverse_iterator<Arrnd>&& other)
            : arrnd_ref_(std::move(other.arrnd_ref_))
            , far_(std::move(other.far_))
            , slice_(std::move(other.slice_))
        { }
        constexpr arrnd_slice_reverse_const_iterator& operator=(arrnd_slice_reverse_iterator<Arrnd>&& other)
        {
            arrnd_ref_ = std::move(other.arrnd_ref_);
            far_ = std::move(other.far_);
            slice_ = std::move(other.slice_);
            return *this;
        }

        constexpr arrnd_slice_reverse_const_iterator& operator--() noexcept
        {
            ++far_;
            return *this;
        }

        constexpr arrnd_slice_reverse_const_iterator operator--(int) noexcept
        {
            arrnd_slice_reverse_const_iterator temp{*this};
            ++(*this);
            return temp;
        }

        constexpr arrnd_slice_reverse_const_iterator& operator-=(difference_type count) noexcept
        {
            far_ += count;
            return *this;
        }

        constexpr arrnd_slice_reverse_const_iterator operator-(difference_type count) const noexcept
        {
            arrnd_slice_reverse_const_iterator temp{*this};
            temp += count;
            return temp;
        }

        constexpr arrnd_slice_reverse_const_iterator& operator++() noexcept
        {
            --far_;
            return *this;
        }

        constexpr arrnd_slice_reverse_const_iterator operator++(int) noexcept
        {
            arrnd_slice_reverse_const_iterator temp{*this};
            --(*this);
            return temp;
        }

        constexpr arrnd_slice_reverse_const_iterator& operator+=(difference_type count) noexcept
        {
            far_ -= count;
            return *this;
        }

        constexpr arrnd_slice_reverse_const_iterator operator+(difference_type count) const noexcept
        {
            arrnd_slice_reverse_const_iterator temp{*this};
            temp -= count;
            return temp;
        }

        [[nodiscard]] constexpr const_reference operator*() const noexcept
        {
            slice_ = arrnd_ref_[std::make_pair((*far_).cbegin(), (*far_).cend())];
            return slice_;
        }

        [[nodiscard]] constexpr bool operator==(const arrnd_slice_reverse_const_iterator& iter) const noexcept
        {
            return far_ == iter.far_;
        }

        [[nodiscard]] constexpr bool operator<(const arrnd_slice_reverse_const_iterator& iter) const noexcept
        {
            return far_ > iter.far_;
        }

        [[nodiscard]] constexpr bool operator<=(const arrnd_slice_reverse_const_iterator& iter) const noexcept
        {
            return far_ >= iter.far_;
        }

        [[nodiscard]] constexpr const_reference operator[](difference_type index) const noexcept
        {
            auto ranges = far_[index];
            return arrnd_ref_[std::make_pair(ranges.cbegin(), ranges.cend())];
        }

        [[nodiscard]] constexpr difference_type operator-(
            const arrnd_slice_reverse_const_iterator& other) const noexcept
        {
            return far_ - other.far_;
        }

    private:
        value_type arrnd_ref_;
        windows_slider_type far_;

        mutable value_type slice_;
    };

    template <typename Arrnd>
    class arrnd_back_insert_iterator {
    public:
        using iterator_category = std::output_iterator_tag;

        constexpr arrnd_back_insert_iterator() noexcept = default;

        explicit arrnd_back_insert_iterator(Arrnd& cont) noexcept
            : cont_(std::addressof(cont))
        { }

        arrnd_back_insert_iterator& operator=(const Arrnd& cont)
        {
            cont_->push_back(cont);
            return *this;
        }

        arrnd_back_insert_iterator& operator=(Arrnd&& cont)
        {
            cont_->push_back(std::move(cont));
            return *this;
        }

        arrnd_back_insert_iterator& operator=(const typename Arrnd::value_type& value)
        {
            cont_->push_back(Arrnd({1}, value));
            return *this;
        }

        arrnd_back_insert_iterator& operator=(typename Arrnd::value_type&& value)
        {
            cont_->push_back(std::move(Arrnd({1}, value)));
            return *this;
        }

        [[nodiscard]] arrnd_back_insert_iterator& operator*() noexcept
        {
            return *this;
        }

        arrnd_back_insert_iterator& operator++() noexcept
        {
            return *this;
        }

        arrnd_back_insert_iterator operator++(int) noexcept
        {
            return *this;
        }

    protected:
        Arrnd* cont_ = nullptr;
    };

    template <typename Arrnd>
    [[nodiscard]] inline constexpr arrnd_back_insert_iterator<Arrnd> arrnd_back_inserter(Arrnd& cont) noexcept
    {
        return arrnd_back_insert_iterator<Arrnd>(cont);
    }

    template <typename Arrnd>
    class arrnd_front_insert_iterator {
    public:
        using iterator_category = std::output_iterator_tag;

        constexpr arrnd_front_insert_iterator() noexcept = default;

        explicit arrnd_front_insert_iterator(Arrnd& cont)
            : cont_(std::addressof(cont))
        { }

        arrnd_front_insert_iterator& operator=(const Arrnd& cont)
        {
            cont_->push_front(cont);
            return *this;
        }

        arrnd_front_insert_iterator& operator=(Arrnd&& cont)
        {
            cont_->push_front(std::move(cont));
            return *this;
        }

        arrnd_front_insert_iterator& operator=(const typename Arrnd::value_type& value)
        {
            cont_->push_front(Arrnd({1}, value));
            return *this;
        }

        arrnd_front_insert_iterator& operator=(typename Arrnd::value_type&& value)
        {
            cont_->push_front(std::move(Arrnd({1}, value)));
            return *this;
        }

        [[nodiscard]] arrnd_front_insert_iterator& operator*()
        {
            return *this;
        }

        arrnd_front_insert_iterator& operator++()
        {
            return *this;
        }

        arrnd_front_insert_iterator operator++(int)
        {
            return *this;
        }

    protected:
        Arrnd* cont_ = nullptr;
    };

    template <typename Arrnd>
    [[nodiscard]] inline constexpr arrnd_front_insert_iterator<Arrnd> arrnd_front_inserter(Arrnd& cont)
    {
        return arrnd_front_insert_iterator<Arrnd>(cont);
    }

    template <typename Arrnd>
    class arrnd_insert_iterator {
    public:
        using iterator_category = std::output_iterator_tag;
        using size_type = typename Arrnd::size_type;

        constexpr arrnd_insert_iterator() noexcept = default;

        explicit arrnd_insert_iterator(Arrnd& cont, size_type ind = 0)
            : cont_(std::addressof(cont))
            , ind_(ind)
        { }

        arrnd_insert_iterator& operator=(const Arrnd& cont)
        {
            cont_->insert(cont, ind_);
            ind_ += cont.info().numel();
            return *this;
        }

        arrnd_insert_iterator& operator=(Arrnd&& cont)
        {
            cont_->insert(std::move(cont), ind_);
            ind_ += cont.info().numel();
            return *this;
        }

        arrnd_insert_iterator& operator=(const typename Arrnd::value_type& value)
        {
            cont_->insert(Arrnd({1}, value), ind_);
            ind_ += 1;
            return *this;
        }

        arrnd_insert_iterator& operator=(typename Arrnd::value_type&& value)
        {
            cont_->insert(std::move(Arrnd({1}, value)), ind_);
            ind_ += 1;
            return *this;
        }

        [[nodiscard]] arrnd_insert_iterator& operator*()
        {
            return *this;
        }

        arrnd_insert_iterator& operator++()
        {
            return *this;
        }

        arrnd_insert_iterator operator++(int)
        {
            return *this;
        }

    protected:
        Arrnd* cont_ = nullptr;
        size_type ind_;
    };

    template <typename Arrnd>
    [[nodiscard]] inline constexpr arrnd_insert_iterator<Arrnd> arrnd_inserter(
        Arrnd& cont, typename Arrnd::size_type ind = 0)
    {
        return arrnd_insert_iterator<Arrnd>(cont, ind);
    }

    template <typename Arrnd>
    class arrnd_slice_back_insert_iterator {
    public:
        using iterator_category = std::output_iterator_tag;
        using size_type = typename Arrnd::size_type;

        constexpr arrnd_slice_back_insert_iterator() noexcept = default;

        explicit arrnd_slice_back_insert_iterator(Arrnd& cont, size_type axis = 0) noexcept
            : cont_(std::addressof(cont))
            , axis_(axis)
        { }

        arrnd_slice_back_insert_iterator& operator=(const Arrnd& cont)
        {
            cont_->push_back(cont, axis_);
            return *this;
        }

        arrnd_slice_back_insert_iterator& operator=(Arrnd&& cont)
        {
            cont_->push_back(std::move(cont), axis_);
            return *this;
        }

        [[nodiscard]] arrnd_slice_back_insert_iterator& operator*() noexcept
        {
            return *this;
        }

        arrnd_slice_back_insert_iterator& operator++() noexcept
        {
            return *this;
        }

        arrnd_slice_back_insert_iterator operator++(int) noexcept
        {
            return *this;
        }

    protected:
        Arrnd* cont_ = nullptr;
        size_type axis_;
    };

    template <typename Arrnd>
    [[nodiscard]] inline constexpr arrnd_slice_back_insert_iterator<Arrnd> arrnd_slice_back_inserter(
        Arrnd& cont, typename Arrnd::size_type axis = 0) noexcept
    {
        return arrnd_slice_back_insert_iterator<Arrnd>(cont, axis);
    }

    template <typename Arrnd>
    class arrnd_slice_front_insert_iterator {
    public:
        using iterator_category = std::output_iterator_tag;
        using size_type = typename Arrnd::size_type;

        constexpr arrnd_slice_front_insert_iterator() noexcept = default;

        explicit arrnd_slice_front_insert_iterator(Arrnd& cont, size_type axis = 0)
            : cont_(std::addressof(cont))
            , axis_(axis)
        { }

        arrnd_slice_front_insert_iterator& operator=(const Arrnd& cont)
        {
            cont_->insert(cont, 0, axis_);
            return *this;
        }

        arrnd_slice_front_insert_iterator& operator=(Arrnd&& cont)
        {
            cont_->insert(std::move(cont), 0, axis_);
            return *this;
        }

        [[nodiscard]] arrnd_slice_front_insert_iterator& operator*()
        {
            return *this;
        }

        arrnd_slice_front_insert_iterator& operator++()
        {
            return *this;
        }

        arrnd_slice_front_insert_iterator operator++(int)
        {
            return *this;
        }

    protected:
        Arrnd* cont_ = nullptr;
        size_type axis_;
    };

    template <typename Arrnd>
    [[nodiscard]] inline constexpr arrnd_slice_front_insert_iterator<Arrnd> arrnd_slice_front_inserter(
        Arrnd& cont, typename Arrnd::size_type axis = 0)
    {
        return arrnd_slice_front_insert_iterator<Arrnd>(cont, axis);
    }

    template <typename Arrnd>
    class arrnd_slice_insert_iterator {
    public:
        using iterator_category = std::output_iterator_tag;
        using size_type = typename Arrnd::size_type;

        constexpr arrnd_slice_insert_iterator() noexcept = default;

        explicit arrnd_slice_insert_iterator(Arrnd& cont, size_type ind = 0, size_type axis = 0)
            : cont_(std::addressof(cont))
            , ind_(ind)
            , axis_(axis)
        { }

        arrnd_slice_insert_iterator& operator=(const Arrnd& cont)
        {
            cont_->insert(cont, ind_, axis_);
            ind_ += cont.info().dims()[axis_];
            return *this;
        }

        arrnd_slice_insert_iterator& operator=(Arrnd&& cont)
        {
            cont_->insert(std::move(cont), ind_, axis_);
            ind_ += cont.info().dims()[axis_];
            return *this;
        }

        [[nodiscard]] arrnd_slice_insert_iterator& operator*()
        {
            return *this;
        }

        arrnd_slice_insert_iterator& operator++()
        {
            return *this;
        }

        arrnd_slice_insert_iterator operator++(int)
        {
            return *this;
        }

    protected:
        Arrnd* cont_ = nullptr;
        size_type ind_;
        size_type axis_;
    };

    template <typename Arrnd>
    [[nodiscard]] inline constexpr arrnd_slice_insert_iterator<Arrnd> arrnd_slice_inserter(
        Arrnd& cont, typename Arrnd::size_type ind = 0, typename Arrnd::size_type axis = 0)
    {
        return arrnd_slice_insert_iterator<Arrnd>(cont, ind, axis);
    }
}

using details::arrnd_returned_element_iterator_tag;
using details::arrnd_returned_slice_iterator_tag;

using details::arrnd_iterator;
using details::arrnd_const_iterator;
using details::arrnd_reverse_iterator;
using details::arrnd_const_reverse_iterator;

using details::arrnd_slice_iterator;
using details::arrnd_slice_const_iterator;
using details::arrnd_slice_reverse_iterator;
using details::arrnd_slice_reverse_const_iterator;

using details::arrnd_back_inserter;
using details::arrnd_front_inserter;
using details::arrnd_inserter;

using details::arrnd_slice_back_inserter;
using details::arrnd_slice_front_inserter;
using details::arrnd_slice_inserter;
}

namespace oc::arrnd {
namespace details {
    struct arrnd_tag { };
    template <typename T>
    concept arrnd_type = std::is_same_v<typename std::remove_cvref_t<T>::tag, arrnd_tag>;

    template <arrnd_type T>
    [[nodiscard]] inline constexpr std::size_t arrnd_depth()
    {
        return T::depth + 1;
    }
    template <typename T>
    [[nodiscard]] inline constexpr std::size_t arrnd_depth()
    {
        return 0;
    }

    template <typename Arrnd1, typename Arrnd2>
    concept arrnd_depths_match = arrnd_type<Arrnd1> && arrnd_type<Arrnd2> && (Arrnd1::depth == Arrnd2::depth);

    template <typename T, std::int64_t Depth>
        requires(Depth >= 0 && Depth <= T::depth)
    struct arrnd_inner {
        using type = arrnd_inner<typename T::value_type, Depth - 1>::type;
    };
    template <typename T>
    struct arrnd_inner<T, 0> {
        using type = T;
    };
    template <typename Arrnd, std::int64_t Level = Arrnd::depth>
        requires std::is_same_v<typename Arrnd::tag, arrnd_tag>
    using arrnd_inner_t = arrnd_inner<Arrnd, Level>::type;

    template <typename T>
    struct typed {
        using type = T;
    };

    // Replace type in nested arrnd at specific depth

    template <typename T, typename R, std::size_t Depth>
    struct arrnd_last_replaced_type_nested_tuple {
        template <typename UT>
        struct typed {
            using type = UT;
        };

        template <typename UT, typename UR, std::size_t UDepth>
        struct impl {
            using type = std::tuple<UT, typename impl<typename UT::value_type, UR, UDepth - 1>::type>;
        };

        template <typename UT, typename UR>
        struct impl<UT, UR, 0> {
            using type = typename UT::template replaced_type<UR>;
        };

        using type
            = std::conditional_t<Depth == 0, std::tuple<typename T::template replaced_type<R>>, typename impl<T, R, Depth>::type>;
    };

    template <typename T, typename R, std::size_t Depth>
    using arrnd_last_replaced_type_nested_tuple_t = typename arrnd_last_replaced_type_nested_tuple<T, R, Depth>::type;

    template <typename T>
    struct flat_tuple {
        using type = std::tuple<T>;
    };

    template <typename... Args>
    struct flat_tuple<std::tuple<Args...>> {
        using type = decltype(std::tuple_cat(typename flat_tuple<Args>::type{}...));
    };

    template <typename Tuple>
    using flat_tuple_t = typename flat_tuple<Tuple>::type;

    template <typename Tuple, std::size_t Depth>
    struct arrnd_fold_tuple {
        static constexpr std::size_t idx = std::tuple_size_v<Tuple> - 1;
        using type = typename std::tuple_element_t<idx - Depth,
            Tuple>::template replaced_type<typename arrnd_fold_tuple<Tuple, Depth - 1>::type>;
    };

    template <typename Tuple>
    struct arrnd_fold_tuple<Tuple, 1> {
        static constexpr std::size_t idx = std::tuple_size_v<Tuple> - 1;
        using type =
            typename std::tuple_element_t<idx - 1, Tuple>::template replaced_type<std::tuple_element_t<idx - 0, Tuple>>;
    };

    template <typename Tuple>
    struct arrnd_fold_tuple<Tuple, 0> {
        static constexpr int idx = std::tuple_size_v<Tuple> - 1;
        using type = std::tuple_element_t<idx - 0, Tuple>;
    };

    template <typename Tuple>
    using arrnd_fold_tuple_t = typename arrnd_fold_tuple<Tuple, std::tuple_size_v<Tuple> - 1>::type;

    template <typename T, typename R, std::size_t Depth>
    struct arrnd_replaced_inner_type {
        using type = arrnd_fold_tuple_t<flat_tuple_t<arrnd_last_replaced_type_nested_tuple_t<T, R, Depth>>>;
    };

    template <typename T, typename R, std::size_t Depth>
    using arrnd_replaced_inner_type_t = arrnd_replaced_inner_type<T, R, Depth>::type;

    // ----------------------------------------------

    template <typename Arrnd, std::size_t Depth>
        requires(Depth >= 0)
    struct arrnd_nested {
        using type = typename Arrnd::template replaced_type<typename arrnd_nested<Arrnd, Depth - 1>::type>;
    };

    template <typename Arrnd>
    struct arrnd_nested<Arrnd, 0> {
        using type = Arrnd;
    };

    template <typename Arrnd, std::size_t Depth>
    using arrnd_nested_t = arrnd_nested<Arrnd, Depth>::type;

    template <typename Arrnd1, typename Arrnd2, std::size_t Depth = std::min(Arrnd1::depth, Arrnd2::depth)>
    using arrnd_tol_type = decltype(typename std::remove_cvref_t<Arrnd1>::template inner_type<Depth>::value_type{} -
        typename std::remove_cvref_t<Arrnd2>::template inner_type<Depth>::value_type{});

    template <typename Arrnd1, typename T, std::size_t Depth = Arrnd1::depth>
    using arrnd_lhs_tol_type
        = decltype(typename std::remove_cvref_t<Arrnd1>::template inner_type<Depth>::value_type{} - std::remove_cvref_t<T>{});

    template <typename T, typename Arrnd2, std::size_t Depth = Arrnd2::depth>
    using arrnd_rhs_tol_type
        = decltype(std::remove_cvref_t<T>{} - typename std::remove_cvref_t<Arrnd2>::template inner_type<Depth>::value_type{});

    enum class arrnd_common_shape { vector, row, column };

    template <arrnd_type Arrnd, typename Constraint>
    class arrnd_lazy_filter {
    public:
        constexpr arrnd_lazy_filter() = delete;

        explicit constexpr arrnd_lazy_filter(Arrnd arr_ref, Constraint constraint)
            : arr_ref_(arr_ref)
            , constraint_(constraint)
        { }

        [[nodiscard]] constexpr operator Arrnd() const
        {
            return arr_ref_.filter(constraint_);
        }

        [[nodiscard]] constexpr Arrnd operator()() const
        {
            return arr_ref_.filter(constraint_);
        }

        constexpr arrnd_lazy_filter(arrnd_lazy_filter&& other) = default;
        template <arrnd_type OtherArrnd, typename OtherConstraint>
        constexpr arrnd_lazy_filter& operator=(arrnd_lazy_filter<OtherArrnd, OtherConstraint>&& other)
        {
            // copy array elements from one proxy to another according to mask

            if (arr_ref_.empty()) {
                return *this;
            }

            // the user is responsible that the number of elements in both constraints is the same

            auto other_filtered = static_cast<OtherArrnd>(other);

            arr_ref_.copy_from(other_filtered, constraint_);

            (void)arrnd_lazy_filter<OtherArrnd, OtherConstraint>(std::move(other));

            return *this;
        }

        constexpr arrnd_lazy_filter(const arrnd_lazy_filter& other) = delete;
        constexpr arrnd_lazy_filter& operator=(const arrnd_lazy_filter& other) = delete;

        template <arrnd_type Arrnd>
        constexpr arrnd_lazy_filter& operator=(const Arrnd& other) &&
        {
            arr_ref_.copy_from(other, constraint_);
            return *this;
        }

        template <arrnd_type Arrnd>
        constexpr arrnd_lazy_filter& operator+=(const Arrnd& other) &&
        {
            arr_ref_.copy_from(arr_ref_.filter(constraint_) + other, constraint_);
            return *this;
        }

        template <arrnd_type Arrnd>
        constexpr arrnd_lazy_filter& operator-=(const Arrnd& other) &&
        {
            arr_ref_.copy_from(arr_ref_.filter(constraint_) - other, constraint_);
            return *this;
        }

        template <arrnd_type Arrnd>
        constexpr arrnd_lazy_filter& operator*=(const Arrnd& other) &&
        {
            arr_ref_.copy_from(arr_ref_.filter(constraint_) * other, constraint_);
            return *this;
        }

        template <arrnd_type Arrnd>
        constexpr arrnd_lazy_filter& operator/=(const Arrnd& other) &&
        {
            arr_ref_.copy_from(arr_ref_.filter(constraint_) / other, constraint_);
            return *this;
        }

        template <arrnd_type Arrnd>
        constexpr arrnd_lazy_filter& operator%=(const Arrnd& other) &&
        {
            arr_ref_.copy_from(arr_ref_.filter(constraint_) % other, constraint_);
            return *this;
        }

        template <arrnd_type Arrnd>
        constexpr arrnd_lazy_filter& operator^=(const Arrnd& other) &&
        {
            arr_ref_.copy_from(arr_ref_.filter(constraint_) ^ other, constraint_);
            return *this;
        }

        template <arrnd_type Arrnd>
        constexpr arrnd_lazy_filter& operator&=(const Arrnd& other) &&
        {
            arr_ref_.copy_from(arr_ref_.filter(constraint_) & other, constraint_);
            return *this;
        }

        template <arrnd_type Arrnd>
        constexpr arrnd_lazy_filter& operator|=(const Arrnd& other) &&
        {
            arr_ref_.copy_from(arr_ref_.filter(constraint_) | other, constraint_);
            return *this;
        }

        template <arrnd_type Arrnd>
        constexpr arrnd_lazy_filter& operator<<=(const Arrnd& other) &&
        {
            arr_ref_.copy_from(arr_ref_.filter(constraint_) << other, constraint_);
            return *this;
        }

        template <arrnd_type Arrnd>
        constexpr arrnd_lazy_filter& operator>>=(const Arrnd& other) &&
        {
            arr_ref_.copy_from(arr_ref_.filter(constraint_) >> other, constraint_);
            return *this;
        }

        template <typename U>
            requires(!arrnd_type<U>)
        constexpr arrnd_lazy_filter& operator+=(const U& value) &&
        {
            *this = arrnd_lazy_filter(arr_ref_ + value, constraint_);
            return *this;
        }

        template <typename U>
            requires(!arrnd_type<U>)
        constexpr arrnd_lazy_filter& operator-=(const U& value) &&
        {
            *this = arrnd_lazy_filter(arr_ref_ - value, constraint_);
            return *this;
        }

        template <typename U>
            requires(!arrnd_type<U>)
        constexpr arrnd_lazy_filter& operator*=(const U& value) &&
        {
            *this = arrnd_lazy_filter(arr_ref_ * value, constraint_);
            return *this;
        }

        template <typename U>
            requires(!arrnd_type<U>)
        constexpr arrnd_lazy_filter& operator/=(const U& value) &&
        {
            *this = arrnd_lazy_filter(arr_ref_ / value, constraint_);
            return *this;
        }

        template <typename U>
            requires(!arrnd_type<U>)
        constexpr arrnd_lazy_filter& operator%=(const U& value) &&
        {
            *this = arrnd_lazy_filter(arr_ref_ % value, constraint_);
            return *this;
        }

        template <typename U>
            requires(!arrnd_type<U>)
        constexpr arrnd_lazy_filter& operator^=(const U& value) &&
        {
            *this = arrnd_lazy_filter(arr_ref_ ^ value, constraint_);
            return *this;
        }

        template <typename U>
            requires(!arrnd_type<U>)
        constexpr arrnd_lazy_filter& operator&=(const U& value) &&
        {
            *this = arrnd_lazy_filter(arr_ref_ & value, constraint_);
            return *this;
        }

        template <typename U>
            requires(!arrnd_type<U>)
        constexpr arrnd_lazy_filter& operator|=(const U& value) &&
        {
            *this = arrnd_lazy_filter(arr_ref_ | value, constraint_);
            return *this;
        }

        template <typename U>
            requires(!arrnd_type<U>)
        constexpr arrnd_lazy_filter& operator<<=(const U& value) &&
        {
            *this = arrnd_lazy_filter(arr_ref_ << value, constraint_);
            return *this;
        }

        template <typename U>
            requires(!arrnd_type<U>)
        constexpr arrnd_lazy_filter& operator>>=(const U& value) &&
        {
            *this = arrnd_lazy_filter(arr_ref_ >> value, constraint_);
            return *this;
        }

        template <arrnd_type Arrnd>
        constexpr arrnd_lazy_filter& operator++() &&
        {
            auto c = arr_ref_.clone();
            *this = arrnd_lazy_filter(++c, constraint_);
            return *this;
        }

        template <arrnd_type Arrnd>
        constexpr arrnd_lazy_filter& operator--() &&
        {
            auto c = arr_ref_.clone();
            *this = arrnd_lazy_filter(--c, constraint_);
            return *this;
        }

        template <typename U>
            requires(!arrnd_type<U>)
        constexpr arrnd_lazy_filter& operator=(const U& value) &&
        {
            if (arr_ref_.empty()) {
                return *this;
            }

            if constexpr (arrnd_type<Constraint>) {
                // constraint is a mask
                if constexpr (std::is_same_v<bool, typename Constraint::value_type>) {
                    if (!std::equal(std::begin(arr_ref_.info().dims()), std::end(arr_ref_.info().dims()),
                            std::begin(constraint_.info().dims()), std::end(constraint_.info().dims()))) {
                        throw std::invalid_argument("invalid mask constraint");
                    }

                    for (auto t : zip(zipped(arr_ref_), zipped(constraint_))) {
                        if (std::get<1>(t)) {
                            std::get<0>(t) = value;
                        }
                    }
                }
                // constraint is indices
                else if constexpr (arrnd_type<Constraint>) {
                    for (auto index : constraint_) {
                        arr_ref_[index] = value;
                    }
                }
            }
            // constraint might be predicator
            else {
                for (auto& element : arr_ref_) {
                    if (constraint_(element)) {
                        element = value;
                    }
                }
            }

            return *this;
        }

        virtual constexpr ~arrnd_lazy_filter() = default;

    private:
        Arrnd arr_ref_;
        Constraint constraint_;
    };

    template <arrnd_type Arrnd, iterator_of_type_integral DimsIt>
    [[nodiscard]] inline constexpr auto zeros(DimsIt first_dim, DimsIt last_dim)
    {
        return Arrnd(first_dim, last_dim, 0);
    }

    template <arrnd_type Arrnd, iterable_of_type_integral Cont>
    [[nodiscard]] inline constexpr auto zeros(const Cont& dims)
    {
        return zeros<Arrnd>(std::begin(dims), std::end(dims));
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto zeros(std::initializer_list<typename Arrnd::size_type> dims)
    {
        return zeros<Arrnd>(dims.begin(), dims.end());
    }

    template <arrnd_type Arrnd, iterator_of_type_integral DimsIt>
    [[nodiscard]] inline constexpr auto eye(DimsIt first_dim, DimsIt last_dim)
    {
        auto ndims = std::distance(first_dim, last_dim);

        if (ndims < 2) {
            throw std::invalid_argument("invalid dims - should be at least matrix");
        }

        auto eye_impl = [](typename Arrnd::size_type r, typename Arrnd::size_type c) {
            if (r == 0 || c == 0) {
                return Arrnd{};
            }
            Arrnd res({r, c}, typename Arrnd::value_type{0});

            auto n = std::min(r, c);

            typename Arrnd::size_type one_ind = 0;

            for (typename Arrnd::size_type i = 0; i < n; ++i) {
                res[one_ind] = typename Arrnd::value_type{1};
                one_ind += c + 1;
            }

            return res;
        };

        if (ndims == 2) {
            return eye_impl(*first_dim, *std::next(first_dim, 1));
        }

        Arrnd res(first_dim, last_dim, typename Arrnd::value_type{0});
        return res.browse(2, [eye_impl](auto page) {
            return eye_impl(page.info().dims().front(), page.info().dims().back());
        });
    }

    template <arrnd_type Arrnd, iterable_of_type_integral Cont>
    [[nodiscard]] inline constexpr auto eye(const Cont& dims)
    {
        return eye<Arrnd>(std::begin(dims), std::end(dims));
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto eye(std::initializer_list<typename Arrnd::size_type> dims)
    {
        return eye<Arrnd>(dims.begin(), dims.end());
    }

    enum class arrnd_traversal_type { dfs, bfs };
    enum class arrnd_traversal_result { apply, transform };
    enum class arrnd_traversal_container { carry, propagate };

    template <typename T, typename DataStorageTraits = simple_vector_traits<T>, typename ArrndInfo = arrnd_info<>>
    class arrnd {
        static_assert(std::is_same_v<T, typename DataStorageTraits::storage_type::value_type>);

    public:
        using tag = arrnd_tag;

        using data_storage_traits_type = DataStorageTraits;

        using storage_type = typename data_storage_traits_type::storage_type;

        template <typename U>
        using allocator_template_type = typename data_storage_traits_type::template allocator_template_type<U>;

        using value_type = typename storage_type::value_type;
        using size_type = typename storage_type::size_type;
        using difference_type = typename storage_type::difference_type;
        using reference = typename storage_type::reference;
        using const_reference = typename storage_type::const_reference;
        using pointer = typename storage_type::pointer;
        using const_pointer = typename storage_type::const_pointer;

        using info_type = ArrndInfo;

        using indexer_type = arrnd_indexer<info_type>;
        using windows_slider_type = arrnd_windows_slider<info_type>;

        using boundary_type = typename info_type::boundary_type;
        using window_type = typename windows_slider_type::window_type;

        using this_type = arrnd<T, data_storage_traits_type, info_type>;
        template <typename U>
        using replaced_type = arrnd<U, typename data_storage_traits_type::template replaced_type<U>, info_type>;

        template <std::size_t Depth>
        using inner_type = arrnd_inner_t<this_type, Depth>;
        template <typename U, std::size_t Depth>
        using replaced_inner_type = arrnd_replaced_inner_type_t<this_type, U, Depth>;

        using iterator = arrnd_iterator<this_type>;
        using const_iterator = arrnd_const_iterator<this_type>;
        using reverse_iterator = arrnd_reverse_iterator<this_type>;
        using const_reverse_iterator = arrnd_const_reverse_iterator<this_type>;

        using slice_iterator = arrnd_slice_iterator<this_type>;
        using const_slice_iterator = arrnd_slice_const_iterator<this_type>;
        using reverse_slice_iterator = arrnd_slice_reverse_iterator<this_type>;
        using const_reverse_slice_iterator = arrnd_slice_reverse_const_iterator<this_type>;

        constexpr static std::size_t depth = arrnd_depth<T>();
        constexpr static bool is_flat = (depth == 0);

        template <std::size_t Depth>
        using nested_t = arrnd_nested_t<this_type, Depth>;

        constexpr arrnd() = default;

        constexpr arrnd(arrnd&& other) = default;
        template <arrnd_type Arrnd>
            requires arrnd_depths_match<arrnd, Arrnd>
        constexpr arrnd(Arrnd&& other)
        {
            *this = other.clone<this_type>();
            (void)Arrnd(std::move(other));
        }
        constexpr arrnd& operator=(arrnd&& other) & = default;
        constexpr arrnd& operator=(arrnd&& other) &&
        {
            if (&other == this) {
                return *this;
            }

            copy_from(other);
            (void)arrnd(std::move(other));
            return *this;
        }
        template <arrnd_type Arrnd>
            requires arrnd_depths_match<arrnd, Arrnd>
        constexpr arrnd& operator=(Arrnd&& other) &
        {
            *this = other.clone<this_type>();
            (void)Arrnd(std::move(other));
            return *this;
        }
        template <arrnd_type Arrnd>
            requires arrnd_depths_match<arrnd, Arrnd>
        constexpr arrnd& operator=(Arrnd&& other) &&
        {
            copy_from(other);
            (void)Arrnd(std::move(other));
            return *this;
        }

        constexpr arrnd(const arrnd& other) = default;
        template <arrnd_type Arrnd>
            requires arrnd_depths_match<arrnd, Arrnd>
        constexpr arrnd(const Arrnd& other)
        {
            *this = other.clone<this_type>();
        }
        constexpr arrnd& operator=(const arrnd& other) & = default;
        constexpr arrnd& operator=(const arrnd& other) &&
        {
            if (&other == this) {
                return *this;
            }

            copy_from(other);
            return *this;
        }
        template <arrnd_type Arrnd>
            requires arrnd_depths_match<arrnd, Arrnd>
        constexpr arrnd& operator=(const Arrnd& other) &
        {
            *this = other.clone<this_type>();
            return *this;
        }
        template <arrnd_type Arrnd>
            requires arrnd_depths_match<arrnd, Arrnd>
        constexpr arrnd& operator=(const Arrnd& other) &&
        {
            copy_from(other);
            return *this;
        }

        template <typename U>
            requires(!arrnd_type<U>)
        constexpr arrnd& operator=(const U& value)
        {
            if (empty()) {
                return *this;
            }

            std::fill(begin(), end(), value);

            return *this;
        }

        virtual constexpr ~arrnd() = default;

        explicit constexpr arrnd(const info_type& info, std::shared_ptr<storage_type> shared_storage)
            : info_(info)
            , shared_storage_(shared_storage)
        {
            // check if the arrnd type invariant is legal.
            if (!oc::arrnd::empty(info)) {
                if (!shared_storage || std::empty(*shared_storage)) {
                    throw std::invalid_argument("invalid null or empty shared storage");
                }
                if (!(info.indices_boundary().start() < shared_storage->size()
                        && info.indices_boundary().stop() <= shared_storage->size())) {
                    throw std::invalid_argument("invalid shared storage size not in indices boundaries");
                }
            }
        }

        template <iterator_of_type_integral InputDimsIt, iterator_type InputDataIt>
        explicit constexpr arrnd(
            InputDimsIt first_dim, InputDimsIt last_dim, InputDataIt first_data, InputDataIt last_data)
            : info_(first_dim, last_dim)
            , shared_storage_(oc::arrnd::empty(info_)
                      ? nullptr
                      : std::allocate_shared<storage_type>(
                          allocator_template_type<storage_type>(), first_data, last_data))
        {
            // in case that data buffer allocated, check the number of data elements is valid
            if (shared_storage_ && std::distance(first_data, last_data) != oc::arrnd::total(info_)) {
                throw std::invalid_argument("number of elements by dims is not match to the number of data elements");
            }
        }

        template <typename U>
        explicit constexpr arrnd(std::initializer_list<size_type> dims, std::initializer_list<U> data)
            : arrnd(dims.begin(), dims.end(), data.begin(), data.end())
        { }

        template <iterable_of_type_integral Cont, iterable_type DataCont>
            requires(!template_type<DataCont, std::initializer_list>)
        explicit constexpr arrnd(const Cont& dims, const DataCont& data)
            : arrnd(std::begin(dims), std::end(dims), std::begin(data), std::end(data))
        { }

        template <iterator_of_type_integral InputDimsIt>
        explicit constexpr arrnd(const InputDimsIt& first_dim, const InputDimsIt& last_dim)
            : info_(first_dim, last_dim)
            , shared_storage_(oc::arrnd::empty(info_)
                      ? nullptr
                      : std::allocate_shared<storage_type>(allocator_template_type<storage_type>(), total(info_)))
        { }
        template <iterable_of_type_integral Cont>
        explicit constexpr arrnd(const Cont& dims)
            : arrnd(std::begin(dims), std::end(dims))
        { }
        explicit constexpr arrnd(std::initializer_list<size_type> dims)
            : arrnd(dims.begin(), dims.end())
        { }

        template <iterator_of_type_integral InputDimsIt>
        explicit constexpr arrnd(InputDimsIt first_dim, InputDimsIt last_dim, const_reference value)
            : info_(first_dim, last_dim)
            , shared_storage_(oc::arrnd::empty(info_)
                      ? nullptr
                      : std::allocate_shared<storage_type>(allocator_template_type<storage_type>(), total(info_)))
        {
            if (shared_storage_) {
                std::fill(shared_storage_->begin(), shared_storage_->end(), value);
            }
        }
        template <iterable_of_type_integral Cont>
        explicit constexpr arrnd(const Cont& dims, const_reference value)
            : arrnd(std::begin(dims), std::end(dims), value)
        { }
        explicit constexpr arrnd(std::initializer_list<size_type> dims, const_reference value)
            : arrnd(dims.begin(), dims.end(), value)
        { }

        template <iterator_of_type_integral InputDimsIt, typename U>
        explicit constexpr arrnd(InputDimsIt first_dim, InputDimsIt last_dim, const U& value)
            : info_(first_dim, last_dim)
            , shared_storage_(oc::arrnd::empty(info_)
                      ? nullptr
                      : std::allocate_shared<storage_type>(allocator_template_type<storage_type>(), total(info_)))
        {
            if (shared_storage_) {
                std::fill(shared_storage_->begin(), shared_storage_->end(), value);
            }
        }
        template <iterable_of_type_integral Cont, typename U>
        explicit constexpr arrnd(const Cont& dims, const U& value)
            : arrnd(std::begin(dims), std::end(dims), value)
        { }
        template <typename U>
        explicit constexpr arrnd(std::initializer_list<size_type> dims, const U& value)
            : arrnd(dims.begin(), dims.end(), value)
        { }

        template <iterator_of_type_integral InputDimsIt, typename Func>
            requires(!arrnd_type<Func> && std::is_invocable_v<Func>)
        explicit constexpr arrnd(InputDimsIt first_dim, InputDimsIt last_dim, Func&& func)
            : info_(first_dim, last_dim)
            , shared_storage_(oc::arrnd::empty(info_)
                      ? nullptr
                      : std::allocate_shared<storage_type>(allocator_template_type<storage_type>(), total(info_)))
        {
            if (shared_storage_) {
                std::for_each(shared_storage_->begin(), shared_storage_->end(), [&func](auto& value) {
                    value = static_cast<value_type>(func());
                });
            }
        }
        template <iterable_of_type_integral Cont, typename Func>
            requires(!arrnd_type<Func> && std::is_invocable_v<Func>)
        explicit constexpr arrnd(const Cont& dims, Func&& func)
            : arrnd(std::begin(dims), std::end(dims), std::forward<Func>(func))
        { }
        template <typename Func>
            requires(!arrnd_type<Func> && std::is_invocable_v<Func>)
        explicit constexpr arrnd(std::initializer_list<size_type> dims, Func&& func)
            : arrnd(dims.begin(), dims.end(), std::forward<Func>(func))
        { }

        [[nodiscard]] constexpr const info_type& info() const noexcept
        {
            return info_;
        }

        [[nodiscard]] constexpr info_type& info() noexcept
        {
            return info_;
        }

        [[nodiscard]] constexpr const auto& shared_storage() const noexcept
        {
            return shared_storage_;
        }

        [[nodiscard]] constexpr auto& shared_storage() noexcept
        {
            return shared_storage_;
        }

        [[nodiscard]] constexpr const this_type* creator() const noexcept
        {
            return creators_.is_creator_valid.expired() ? nullptr : creators_.latest_creator;
        }

        [[nodiscard]] explicit constexpr operator value_type() const
        {
            if (!oc::arrnd::isscalar(info_)) {
                throw std::exception("info is not scalar");
            }
            return (*this)[info_.indices_boundary().start()];
        }

        [[nodiscard]] constexpr const_reference operator[](size_type index) const noexcept
        {
            assert(shared_storage_);
            assert(index >= info_.indices_boundary().start() && index < info_.indices_boundary().stop());
            return shared_storage_->data()[index];
        }
        [[nodiscard]] constexpr reference operator[](size_type index) noexcept
        {
            assert(shared_storage_);
            assert(index >= info_.indices_boundary().start() && index < info_.indices_boundary().stop());
            return shared_storage_->data()[index];
        }

        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr const_reference operator[](std::pair<InputIt, InputIt> subs) const noexcept
        {
            assert(shared_storage_);
            return shared_storage_->data()[oc::arrnd::sub2ind(info_, subs.first, subs.second)];
        }
        template <iterable_of_type_integral Cont>
        [[nodiscard]] constexpr const_reference operator[](const Cont& subs) const noexcept
        {
            return (*this)[std::make_pair(std::begin(subs), std::end(subs))];
        }
        [[nodiscard]] constexpr const_reference operator[](std::initializer_list<size_type> subs) const noexcept
        {
            return (*this)[std::make_pair(subs.begin(), subs.end())];
        }

        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr reference operator[](std::pair<InputIt, InputIt> subs) noexcept
        {
            assert(shared_storage_);
            return shared_storage_->data()[oc::arrnd::sub2ind(info_, subs.first, subs.second)];
        }
        template <iterable_of_type_integral Cont>
        [[nodiscard]] constexpr reference operator[](const Cont& subs) noexcept
        {
            return (*this)[std::make_pair(std::begin(subs), std::end(subs))];
        }
        [[nodiscard]] constexpr reference operator[](std::initializer_list<size_type> subs) noexcept
        {
            return (*this)[std::make_pair(subs.begin(), subs.end())];
        }

        template <iterator_of_type_interval InputIt>
        [[nodiscard]] constexpr this_type operator[](std::pair<InputIt, InputIt> boundaries) const&
        {
            this_type slice(oc::arrnd::slice(info_, boundaries.first, boundaries.second), shared_storage_);
            slice.creators_.is_creator_valid = creators_.has_original_creator;
            slice.creators_.latest_creator = this;
            return slice;
        }
        template <iterator_of_type_interval InputIt>
        [[nodiscard]] constexpr this_type operator[](std::pair<InputIt, InputIt> boundaries) const&&
        {
            return this_type(oc::arrnd::slice(info_, boundaries.first, boundaries.second), shared_storage_);
        }
        template <iterable_of_type_interval Cont>
        [[nodiscard]] constexpr this_type operator[](const Cont& boundaries) const&
        {
            return (*this)[std::make_pair(std::cbegin(boundaries), std::cend(boundaries))];
        }
        template <iterable_of_type_interval Cont>
        [[nodiscard]] constexpr this_type operator[](const Cont& boundaries) const&&
        {
            return std::move(*this)[std::make_pair(std::begin(boundaries), std::end(boundaries))];
        }
        [[nodiscard]] constexpr this_type operator[](std::initializer_list<boundary_type> boundaries) const&
        {
            return (*this)[std::make_pair(boundaries.begin(), boundaries.end())];
        }
        [[nodiscard]] constexpr this_type operator[](
            std::initializer_list<boundary_type> boundaries) const&&
        {
            return std::move(*this)[std::make_pair(boundaries.begin(), boundaries.end())];
        }

        [[nodiscard]] constexpr this_type operator[](boundary_type boundary) const&
        {
            this_type slice(
                oc::arrnd::squeeze(oc::arrnd::slice(info_, boundary, 0), arrnd_squeeze_type::left, 1), shared_storage_);
            slice.creators_.is_creator_valid = creators_.has_original_creator;
            slice.creators_.latest_creator = this;
            return slice;
        }
        [[nodiscard]] constexpr this_type operator[](boundary_type boundary) const&&
        {
            return this_type(
                oc::arrnd::squeeze(oc::arrnd::slice(info_, boundary, 0), arrnd_squeeze_type::left, 1), shared_storage_);
        }

        [[nodiscard]] constexpr this_type operator()(boundary_type boundary, size_type axis) const&
        {
            this_type slice(oc::arrnd::slice(info_, boundary, axis), shared_storage_);
            slice.creators_.is_creator_valid = creators_.has_original_creator;
            slice.creators_.latest_creator = this;
            return slice;
        }
        [[nodiscard]] constexpr this_type operator()(boundary_type boundary, size_type axis) const&&
        {
            return this_type(oc::arrnd::slice(info_, boundary, axis), shared_storage_);
        }

        // access relative array indices, might be slow for slices
        [[nodiscard]] constexpr const_reference operator()(size_type index) const noexcept
        {
            assert(index >= 0 && index <= total(info_));
            return issliced(info_) ? shared_storage_->data()[oc::arrnd::ind2ind(info_, index)]
                                   : shared_storage_->data()[index];
        }
        [[nodiscard]] constexpr reference operator()(size_type index) noexcept
        {
            assert(index >= 0 && index <= total(info_));
            return issliced(info_) ? shared_storage_->data()[oc::arrnd::ind2ind(info_, index)]
                                   : shared_storage_->data()[index];
        }

        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto operator()(std::pair<InputIt, InputIt> indices) const
        {
            return std::move(*this)(replaced_type<size_type>(
                {std::distance(indices.first, indices.second)}, indices.first, indices.second));
        }
        template <iterable_of_type_integral Cont>
            requires(!arrnd_type<Cont>)
        [[nodiscard]] constexpr auto operator()(const Cont& indices) const
        {
            return (*this)(replaced_type<size_type>({std::ssize(indices)}, std::begin(indices), std::end(indices)));
        }
        // more strict function than filter. in case of logical type arrnd, its being treated as mask
        template <arrnd_type Arrnd>
            requires(std::integral<typename Arrnd::value_type>)
        [[nodiscard]] constexpr auto operator()(const Arrnd& selector) const
        {
            return arrnd_lazy_filter(*this, selector);
        }
        [[nodiscard]] constexpr auto operator()(std::initializer_list<size_type> indices) const
        {
            std::initializer_list<size_type> dims{std::size(indices)};
            return (*this)(replaced_type<size_type>(dims.begin(), dims.end(), indices.begin(), indices.end()));
        }

        [[nodiscard]] constexpr auto operator()(arrnd_common_shape shape) const
        {
            return reshape(shape);
        }

        template <typename Pred>
            requires(!arrnd_type<Pred> && std::is_invocable_v<Pred, value_type>)
        [[nodiscard]] constexpr auto operator()(Pred&& pred) const
        {
            auto selector = [&pred](const value_type& value) {
                return pred(value);
            };

            return arrnd_lazy_filter(*this, selector);
        }

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return oc::arrnd::empty(info_);
        }

        template <iterator_type InputIt>
        constexpr this_type& copy_from(InputIt first_data, InputIt last_data)
        {
            if (empty() || std::distance(first_data, last_data) <= 0) {
                return *this;
            }

            for (auto t : zip(zipped(*this), zipped(first_data, last_data))) {
                if constexpr (arrnd_type<value_type> && arrnd_type<decltype(std::get<1>(t))>) {
                    std::get<0>(t).copy_from(std::begin(std::get<1>(t)), std::end(std::get<1>(t)));
                } else {
                    std::get<0>(t) = std::get<1>(t);
                }
            }

            return *this;
        }

        template <iterable_type Cont>
        constexpr this_type& copy_from(const Cont& data)
        {
            return copy_from(std::begin(data), std::end(data));
        }

        template <typename U>
        constexpr this_type& copy_from(std::initializer_list<U> data)
        {
            return copy_from(data.begin(), data.end());
        }

        template <iterator_type InputIt1, iterator_of_type_integral InputIt2>
        constexpr this_type& copy_from(
            InputIt1 first_data, InputIt1 last_data, InputIt2 first_index, InputIt2 last_index)
        {
            if (empty() || std::distance(first_data, last_data) <= 0 || std::distance(first_index, last_index) <= 0) {
                return *this;
            }

            if (std::any_of(first_index, last_index, [&](auto index) {
                    return index < info_.indices_boundary().start() || index >= info_.indices_boundary().stop();
                })) {
                throw std::invalid_argument("invalid input indices");
            }

            for (auto t : zip(zipped(first_data, last_data), zipped(first_index, last_index))) {
                if constexpr (arrnd_type<value_type> && arrnd_type<decltype(std::get<0>(t))>) {
                    (*this)[std::get<1>(t)].copy_from(std::get<0>(t));
                } else {
                    (*this)[std::get<1>(t)] = std::get<0>(t);
                }
            }

            return *this;
        }

        template <iterable_type Cont1, iterable_of_type_integral Cont2>
            requires(!std::is_same_v<bool, iterable_value_t<Cont2>>) // to prevent ambiguity with mask
        constexpr this_type& copy_from(const Cont1& data, const Cont2& indices)
        {
            return copy_from(std::begin(data), std::end(data), std::begin(indices), std::end(indices));
        }

        template <typename U>
        constexpr this_type& copy_from(std::initializer_list<U> data, std::initializer_list<size_type> indices)
        {
            return copy_from(data.begin(), data.end(), indices.begin(), indices.end());
        }

        template <arrnd_type Arrnd1, arrnd_type Arrnd2>
            requires(std::is_same_v<bool, typename Arrnd2::value_type>)
        constexpr this_type& copy_from(const Arrnd1& data, const Arrnd2& mask)
        {
            if (empty() || data.empty() || mask.empty()) {
                return *this;
            }

            if (!std::equal(std::begin(info_.dims()), std::end(info_.dims()), std::begin(mask.info().dims()),
                    std::end(mask.info().dims()))) {
                throw std::invalid_argument("invalid mask dims");
            }

            auto dst_it = begin();
            auto src_it = data.begin();
            auto msk_it = mask.begin();

            for (; dst_it != end() && src_it != data.end() && msk_it != mask.end(); ++dst_it, ++msk_it) {
                if (*msk_it) {
                    if constexpr (arrnd_type<value_type> && arrnd_type<decltype(*src_it)>) {
                        (*dst_it).copy_from(*src_it);
                    } else {
                        *dst_it = *src_it;
                    }
                    ++src_it;
                }
            }
        }

        template <iterator_type InputIt, typename Pred>
            requires(std::is_invocable_v<Pred, value_type> && !arrnd_type<Pred>)
        constexpr this_type& copy_from(InputIt first_data, InputIt last_data, Pred pred)
        {
            if (empty() || std::distance(first_data, last_data) <= 0) {
                return *this;
            }

            if (total(info_) < std::distance(first_data, last_data)) {
                throw std::invalid_argument("invalid input data - number of input data might be small for array");
            }

            auto dst_it = begin();
            auto src_it = first_data;

            for (; dst_it != end() && src_it != last_data; ++dst_it) {
                if (pred(*dst_it)) {
                    if constexpr (arrnd_type<value_type> && arrnd_type<decltype(*src_it)>) {
                        (*dst_it).copy_from(*src_it);
                    } else {
                        *dst_it = *src_it;
                    }
                    ++src_it;
                }
            }

            return *this;
        }

        template <iterable_type Cont, typename Pred>
            requires(std::is_invocable_v<Pred, value_type> && !arrnd_type<Pred>)
        constexpr this_type& copy_from(const Cont& data, Pred&& pred)
        {
            return copy_from(std::begin(data), std::end(data), std::forward<Pred>(pred));
        }

        template <typename U, typename Pred>
            requires(std::is_invocable_v<Pred, value_type> && !arrnd_type<Pred>)
        constexpr this_type& copy_from(std::initializer_list<U> data, Pred&& pred)
        {
            return copy_from(data.begin(), data.end(), std::forward<Pred>(pred));
        }

        template <iterator_type InputIt1, iterator_of_type_interval InputIt2>
        constexpr this_type& copy_from(
            InputIt1 first_data, InputIt1 last_data, InputIt2 first_boundary, InputIt2 last_boundary)
        {
            (*this)[std::make_pair(first_boundary, last_boundary)].copy_from(first_data, last_data);
            return *this;
        }

        template <iterable_type Cont1, iterable_of_type_interval Cont2>
        constexpr this_type& copy_from(const Cont1& data, const Cont2& boundaries)
        {
            return copy_from(std::begin(data), std::end(data), std::begin(boundaries), std::end(boundaries));
        }

        template <typename U, arrnd_type Arrnd>
        constexpr this_type& copy_from(std::initializer_list<U> data, std::initializer_list<boundary_type> boundaries)
        {
            return copy_from(data.begin(), data.end(), boundaries.begin(), boundaries.end());
        }

        // Modify array by simplifying info and by squeeze internal buffer
        // data in the front of it for efficient internal buffer usage.
        constexpr this_type& refresh()
        {
            // continuous array, which is not sliced or transposed
            if (empty() || info_.hints() == arrnd_hint::continuous) {
                return *this;
            }

            // in order to perform the rearranging of the array elements
            // without using temporary buffer, a unstranspose of the array
            // info is required.
            this_type tmp(unstranspose(info_), shared_storage_);

            std::move(tmp.begin(), tmp.end(), std::begin(*shared_storage_));
            tmp.shared_storage_->resize(total(tmp.info_));

            *this = this_type(simplify(info_), shared_storage_);

            return *this;
        }

        // Ensures a full copy of this array and its inner arrays.
        template <arrnd_type Arrnd = this_type>
        [[nodiscard]] constexpr Arrnd clone() const
        {
            if (empty()) {
                return Arrnd();
            }

            Arrnd c{};

            c.info() =
                typename Arrnd::info_type(info_.dims(), info_.strides(), info_.indices_boundary(), info_.hints());
            if constexpr (this_type::is_flat) {
                c.shared_storage() = std::allocate_shared<typename Arrnd::storage_type>(
                    typename Arrnd::template allocator_template_type<typename Arrnd::storage_type>(), *shared_storage_);
                c.shared_storage()->reserve(shared_storage_->capacity());
            } else {
                c.shared_storage() = std::allocate_shared<typename Arrnd::storage_type>(
                    typename Arrnd::template allocator_template_type<typename Arrnd::storage_type>(),
                    shared_storage_->size());
                c.shared_storage()->reserve(shared_storage_->capacity());

                for (auto t : zip(zipped(*this), zipped(c))) {
                    std::get<1>(t) = std::get<0>(t).template clone<typename Arrnd::value_type>();
                }
            }

            return c;
        }

        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr this_type reshape(InputIt first_dim, InputIt last_dim) const
        {
            if (total(info_) != total(info_type(first_dim, last_dim))) {
                throw std::invalid_argument("invalid input dims - different total size from array");
            }

            // no reshape
            if (std::equal(std::begin(info_.dims()), std::end(info_.dims()), first_dim, last_dim)) {
                return *this;
            }

            if (info_.hints() != arrnd_hint::continuous) {
                throw std::invalid_argument("invalid reshape operation for non-standard array (might be "
                                            "sliced/transposed) - try to use refresh()");
            }

            return this_type(info_type(first_dim, last_dim), shared_storage_);
        }

        template <iterable_of_type_integral Cont>
        [[nodiscard]] constexpr this_type reshape(const Cont& dims) const
        {
            return reshape(std::begin(dims), std::end(dims));
        }

        [[nodiscard]] constexpr this_type reshape(std::initializer_list<size_type> dims) const
        {
            return reshape(dims.begin(), dims.end());
        }

        [[nodiscard]] constexpr this_type reshape(arrnd_common_shape shape) const
        {
            if (empty()) {
                return *this;
            }

            switch (shape) {
            case arrnd_common_shape::vector:
                return reshape({total(info_)});
            case arrnd_common_shape::row:
                return reshape({size_type{1}, total(info_)});
            case arrnd_common_shape::column:
                return reshape({total(info_), size_type{1}});
            default:
                assert(false && "unknown arrnd_common_shape value");
                return *this;
            }
        }

        // Similar to std::vector, resize inner buffer to suitable input dims
        // and arrange array elements according to it.
        template <iterator_of_type_integral InputIt>
        constexpr this_type& resize(InputIt first_dim, InputIt last_dim)
        {
            if (empty()) {
                *this = this_type(first_dim, last_dim);
                return *this;
            }

            info_type new_info(first_dim, last_dim);

            // in case of same number of elements and standard array, resize can be done by reshape operation
            if (total(info_) == total(new_info) && info_.hints() == arrnd_hint::continuous) {
                *this = reshape(first_dim, last_dim);
                return *this;
            }

            // check if there's enough space to arrange elements, and resize internal buffer if not
            bool is_post_resize_required = total(new_info) <= shared_storage_->size();
            if (!is_post_resize_required) {
                shared_storage_->resize(total(new_info));
            }

            this_type res(new_info, shared_storage_);

            size_type min_num_dims = std::min(size(info_), size(new_info));

            typename info_type::storage_traits_type::template replaced_type<boundary_type>::storage_type boundaries(
                size(info_), boundary_type::full());

            typename info_type::storage_traits_type::template replaced_type<boundary_type>::storage_type new_boundaries(
                size(new_info), boundary_type::full());

            auto z = zip(zipped(boundaries), zipped(new_boundaries), zipped(info_.dims()), zipped(new_info.dims()));

            std::for_each(std::rbegin(z), std::next(std::rbegin(z), min_num_dims), [](auto t) {
                auto dim = std::get<2>(t);
                auto new_dim = std::get<3>(t);

                auto& boundary = std::get<0>(t);
                boundary = boundary_type::to(std::min(dim, new_dim));

                auto& new_boundary = std::get<1>(t);
                new_boundary = boundary_type::to(std::min(dim, new_dim));
            });

            res[new_boundaries] = (*this)[boundaries];

            if (is_post_resize_required) {
                shared_storage_->resize(total(new_info));
            }

            *this = std::move(res);

            return *this;
        }

        template <iterable_of_type_integral Cont>
        constexpr this_type& resize(const Cont& dims)
        {
            return resize(std::begin(dims), std::end(dims));
        }

        constexpr this_type& resize(std::initializer_list<size_type> dims)
        {
            return resize(dims.begin(), dims.end());
        }

        template <arrnd_type Arrnd>
        constexpr this_type& push_back(const Arrnd& arr, size_type axis = 0)
        {
            size_type index = empty() ? 0 : info_.dims()[axis];
            return insert<Arrnd>(arr, index, axis);
        }

        template <arrnd_type Arrnd>
        constexpr this_type& push_front(const Arrnd& arr, size_type axis = 0)
        {
            return insert<Arrnd>(arr, 0, axis);
        }

        /*
        * Insert arr as set of values at index/axis, such that 0<=index<=dims[axis] and 0<=axis<size(dims)
        * Algorithm:
        * - if array and arr are empty:
        *   - index should be valid and axis is irrelevant
        *   - return *this
        * - if arr is empty:
        *   - index and axis are irrelevant
        *   - return *this
        * - if array is empty:
        *   - index should be valid and axis is irrelevant
        *   - return clone(arr)
        * - else (both array and arr contain values):
        *   - check arr size validity (should be the same as array except at axis)
        *   - refresh array for space efficiency
        *   - calculate new info, and new total
        *   - append capacity according to (total(new) - total(prev))
        *   - build ref with new info
        *   - copy values
        */
        template <arrnd_type Arrnd>
        constexpr this_type& insert(const Arrnd& arr, size_type index, size_type axis = 0)
        {
            if (empty() && arr.empty()) {
                if (index != 0) {
                    throw std::invalid_argument("invalid index for empty arrays");
                }

                return *this;
            }

            if (arr.empty()) {
                if (axis < 0 || axis >= size(info_)) {
                    throw std::invalid_argument("invalid axis for empty input");
                }
                if (index < 0 || index > info_.dims()[axis]) {
                    throw std::invalid_argument("invalid index for empty input");
                }

                return *this;
            }

            if (empty()) {
                if (index != 0) {
                    throw std::invalid_argument("invalid index for empty array");
                }

                *this = arr.template clone<this_type>();
                return *this;
            }

            if (axis < 0 || axis >= size(info_)) {
                throw std::invalid_argument("invalid axis");
            }
            if (index < 0 || index > info_.dims()[axis]) {
                throw std::invalid_argument("invalid index");
            }

            if (size(info_) != size(arr.info())) {
                throw std::invalid_argument("invalid input - different number of dims");
            }

            auto dims = info_.dims();
            dims[axis] = arr.info().dims()[axis];
            if (!std::equal(
                    std::begin(arr.info().dims()), std::end(arr.info().dims()), std::begin(dims), std::end(dims))) {
                throw std::invalid_argument("invalid input - dims should be the same except at axis");
            }

            refresh();

            auto& new_dims = dims;
            new_dims[axis] += info_.dims()[axis];
            info_type new_info(new_dims);

            auto total_diff = total(new_info) - total(info_);
            shared_storage_->append(total_diff);

            this_type res(new_info, shared_storage_);

            // if push_back at axis zero, no need to move current data in storage
            if (index == info_.dims()[axis] && axis == 0) {
                std::copy(arr.cbegin(axis, arrnd_returned_element_iterator_tag{}),
                    arr.cend(axis, arrnd_returned_element_iterator_tag{}),
                    std::next(res.begin(axis, arrnd_returned_element_iterator_tag{}), total(info_)));
            }
            // if there's enough capacity in data storage, use if for temp data
            else if (shared_storage_->capacity() - shared_storage_->size() >= total(info_)) {
                auto current_shared_storage_size = shared_storage_->size();

                // temporarily resize data storage to its capacity size
                shared_storage_->resize(shared_storage_->capacity());

                auto tmp_it = std::next(std::begin(*shared_storage_), current_shared_storage_size);
                std::move(cbegin(axis, arrnd_returned_element_iterator_tag{}),
                    cend(axis, arrnd_returned_element_iterator_tag{}), tmp_it);

                auto num_pre_input_copies = index * (total(res.info_) / res.info_.dims()[axis]);

                std::move(tmp_it, std::next(tmp_it, num_pre_input_copies),
                    res.begin(axis, arrnd_returned_element_iterator_tag{}));

                std::copy(arr.cbegin(axis, arrnd_returned_element_iterator_tag{}),
                    arr.cend(axis, arrnd_returned_element_iterator_tag{}),
                    std::next(res.begin(axis, arrnd_returned_element_iterator_tag{}), num_pre_input_copies));

                std::move(std::next(tmp_it, num_pre_input_copies), std::next(tmp_it, total(info_)),
                    std::next(res.begin(axis, arrnd_returned_element_iterator_tag{}),
                        num_pre_input_copies + total(arr.info())));

                shared_storage_->resize(current_shared_storage_size);
            }
            // else create a temporary data storage
            else {
                storage_type tmp(total(info_));
                std::move(cbegin(axis, arrnd_returned_element_iterator_tag{}),
                    cend(axis, arrnd_returned_element_iterator_tag{}), std::begin(tmp));

                auto num_pre_input_copies = index * (total(res.info_) / res.info_.dims()[axis]);

                std::move(std::begin(tmp), std::next(std::begin(tmp), num_pre_input_copies),
                    res.begin(axis, arrnd_returned_element_iterator_tag{}));

                std::copy(arr.cbegin(axis, arrnd_returned_element_iterator_tag{}),
                    arr.cend(axis, arrnd_returned_element_iterator_tag{}),
                    std::next(res.begin(axis, arrnd_returned_element_iterator_tag{}), num_pre_input_copies));

                std::move(std::next(std::begin(tmp), num_pre_input_copies), std::end(tmp),
                    std::next(res.begin(axis, arrnd_returned_element_iterator_tag{}),
                        num_pre_input_copies + total(arr.info())));
            }

            *this = std::move(res);
            return *this;
        }

        template <iterator_of_template_type<std::tuple> InputIt>
        constexpr this_type& repeat(InputIt first_tuple, InputIt last_tuple)
        {
            if (std::distance(first_tuple, last_tuple) <= 0) {
                return *this;
            }

            auto res = *this;
            auto mid = res.clone();

            for (auto t_it = first_tuple; t_it != last_tuple; ++t_it) {
                const auto& t = *t_it;
                for (size_type i = 0; i < std::get<0>(t) - 1; ++i) {
                    res.push_back(mid, std::get<1>(t));
                }
                mid = res.clone();
            }

            *this = std::move(res);

            return *this;
        }

        template <template_type<std::tuple> Tuple>
        constexpr this_type& repeat(std::initializer_list<Tuple> count_axis_tuples)
        {
            return repeat(count_axis_tuples.begin(), count_axis_tuples.end());
        }

        template <iterable_of_template_type<std::tuple> Cont>
        constexpr this_type& repeat(const Cont& count_axis_tuples)
        {
            return repeat(std::begin(count_axis_tuples), std::end(count_axis_tuples));
        }

        template <iterator_of_type_integral InputIt>
        constexpr this_type& repeat(InputIt first_rep, InputIt last_rep)
        {
            if (std::distance(first_rep, last_rep) > info_.dims().size()) {
                throw std::invalid_argument("invalid number of input reps");
            }

            auto res = *this;
            auto mid = res.clone();

            size_type curr_axis = 0;

            for (auto r_it = first_rep; r_it != last_rep; ++r_it) {
                for (size_type i = 0; i < *r_it - 1; ++i) {
                    res.push_back(mid, curr_axis);
                }
                ++curr_axis;
                mid = res.clone();
            }

            *this = std::move(res);

            return *this;
        }

        constexpr this_type& repeat(std::initializer_list<size_type> reps)
        {
            return repeat(reps.begin(), reps.end());
        }

        template <iterable_of_type_integral Cont>
        constexpr this_type& repeat(const Cont& reps)
        {
            return repeat(std::begin(reps), std::end(reps));
        }

        constexpr this_type& erase(size_type count, size_type index, size_type axis = 0)
        {
            if (empty()) {
                if (index != 0) {
                    throw std::invalid_argument("invalid index for empty array");
                }
                if (count != 0) {
                    throw std::invalid_argument("invalid count for empty array");
                }

                return *this;
            }

            if (axis < 0 || axis >= size(info_)) {
                throw std::invalid_argument("invalid axis");
            }
            if (index < 0 || index >= info_.dims()[axis]) {
                throw std::invalid_argument("invalid index");
            }
            if (index + count > info_.dims()[axis]) {
                throw std::invalid_argument("invalid count");
            }

            // check if resulted array is empty
            auto new_dims = info_.dims();
            new_dims[axis] -= count;
            info_type new_info(new_dims);

            if (oc::arrnd::empty(new_info)) {
                *this = this_type{};
                return *this;
            }

            refresh();

            this_type res(new_info, shared_storage_);

            size_type num_pre_removals = index * (total(res.info_) / res.info_.dims()[axis]);
            size_type removals = total(info_) - total(res.info_);

            if (shared_storage_->capacity() - shared_storage_->size() >= total(res.info())) {
                auto current_shared_storage_size = shared_storage_->size();

                // temporarily resize data storage to its capacity size
                shared_storage_->resize(shared_storage_->capacity());

                auto tmp_it = std::next(std::begin(*shared_storage_), current_shared_storage_size);
                std::move(cbegin(axis, arrnd_returned_element_iterator_tag{}),
                    std::next(cbegin(axis, arrnd_returned_element_iterator_tag{}), num_pre_removals), tmp_it);
                std::move(std::next(cbegin(axis, arrnd_returned_element_iterator_tag{}), num_pre_removals + removals),
                    cend(axis, arrnd_returned_element_iterator_tag{}), std::next(tmp_it, num_pre_removals));

                std::move(tmp_it, std::next(tmp_it, total(res.info())), std::begin(*shared_storage_));

                shared_storage_->resize(current_shared_storage_size);
            } else {
                storage_type tmp(total(info_));
                std::move(cbegin(axis, arrnd_returned_element_iterator_tag{}),
                    std::next(cbegin(axis, arrnd_returned_element_iterator_tag{}), num_pre_removals), std::begin(tmp));
                std::move(std::next(cbegin(axis, arrnd_returned_element_iterator_tag{}), num_pre_removals + removals),
                    cend(axis, arrnd_returned_element_iterator_tag{}), std::next(std::begin(tmp), num_pre_removals));

                std::move(std::begin(tmp), std::end(tmp), std::begin(*shared_storage_));
            }

            shared_storage_->resize(total(new_info));

            *this = std::move(res);
            return *this;
        }

        constexpr this_type& pop_front(size_type count = 1, size_type axis = 0)
        {
            return erase(empty() ? 0 : count, 0, axis);
        }

        constexpr this_type& pop_back(size_type count = 1, size_type axis = 0)
        {
            size_type fixed_count = empty() ? 0 : count;
            size_type ind = empty() ? size_type{0} : *std::next(info_.dims().cbegin(), axis) - fixed_count;
            return erase(fixed_count, ind, axis);
        }

        template <std::size_t FromDepth, std::size_t ToDepth, arrnd_traversal_type TraversalType,
            arrnd_traversal_result TraversalResult, typename UnaryOp, std::size_t CurrDepth = 0>
            requires(FromDepth <= ToDepth && TraversalType == arrnd_traversal_type::dfs
                && TraversalResult == arrnd_traversal_result::apply)
        constexpr auto& traverse(UnaryOp op)
        {
            // Traverse and perform operation on leaves.
            if constexpr (arrnd_type<value_type>) {
                for (auto& inner : *this) {
                    inner = inner.template traverse<FromDepth, ToDepth, TraversalType, TraversalResult, UnaryOp,
                        CurrDepth + 1>(op);
                }
            } else if constexpr (CurrDepth + 1 >= FromDepth && CurrDepth + 1 <= ToDepth) {
                for (auto& value : *this) {
                    if constexpr (std::is_void_v<decltype(op(value))>) {
                        op(value);
                    } else {
                        value = op(value);
                    }
                }
            }

            // Apply operation on array if depth is relevant
            if constexpr (CurrDepth >= FromDepth && CurrDepth <= ToDepth) {
                if constexpr (std::is_void_v<decltype(op(*this))>) {
                    op(*this);
                } else {
                    *this = op(*this);
                }
            }

            return *this;
        }

        template <std::size_t FromDepth, std::size_t ToDepth, arrnd_traversal_type TraversalType,
            arrnd_traversal_result TraversalResult, typename UnaryOp, std::size_t CurrDepth = 0>
            requires(FromDepth <= ToDepth && TraversalType == arrnd_traversal_type::bfs
                && TraversalResult == arrnd_traversal_result::apply)
        constexpr auto& traverse(UnaryOp op)
        {
            // Apply operation on array if depth is relevant
            if constexpr (CurrDepth >= FromDepth && CurrDepth <= ToDepth) {
                if constexpr (std::is_void_v<decltype(op(*this))>) {
                    op(*this);
                } else {
                    *this = op(*this);
                }
            }

            // Traverse and perform operation on leaves.
            if constexpr (arrnd_type<value_type>) {
                for (auto& inner : *this) {
                    inner = inner.template traverse<FromDepth, ToDepth, TraversalType, TraversalResult, UnaryOp,
                        CurrDepth + 1>(op);
                }
            } else if constexpr (CurrDepth + 1 >= FromDepth && CurrDepth + 1 <= ToDepth) {
                for (auto& value : *this) {
                    if constexpr (std::is_void_v<decltype(op(value))>) {
                        op(value);
                    } else {
                        value = op(value);
                    }
                }
            }

            return *this;
        }

        template <std::size_t FromDepth, std::size_t ToDepth, arrnd_traversal_type TraversalType,
            arrnd_traversal_result TraversalResult, arrnd_traversal_container TraversalCont, iterable_type Cont,
            typename Op, std::size_t CurrDepth = 0>
            requires(FromDepth <= ToDepth && TraversalType == arrnd_traversal_type::dfs
                && TraversalResult == arrnd_traversal_result::apply)
        constexpr auto& traverse(const Cont& cont, Op op)
        {
            // Traverse and perform operation on leaves.
            if constexpr (arrnd_type<value_type>) {
                if constexpr (TraversalCont == arrnd_traversal_container::carry) {
                    for (auto& inner : *this) {
                        inner = inner.template traverse<FromDepth, ToDepth, TraversalType, TraversalResult,
                            TraversalCont, Cont, Op, CurrDepth + 1>(cont, op);
                    }
                } else {
                    for (auto inners : zip(zipped(*this), zipped(cont))) {
                        std::get<0>(inners)
                            = std::get<0>(inners)
                                  .template traverse<FromDepth, ToDepth, TraversalType, TraversalResult, TraversalCont,
                                      std::remove_reference_t<decltype(std::get<1>(inners))>, Op, CurrDepth + 1>(
                                      std::get<1>(inners), op);
                    }
                }
            } else if constexpr (CurrDepth + 1 >= FromDepth && CurrDepth + 1 <= ToDepth) {
                if constexpr (TraversalCont == arrnd_traversal_container::carry) {
                    for (auto& value : *this) {
                        if constexpr (std::is_void_v<decltype(op(value, cont))>) {
                            op(value, cont);
                        } else {
                            value = op(value, cont);
                        }
                    }
                } else {
                    for (auto values : zip(zipped(*this), zipped(cont))) {
                        if constexpr (std::is_void_v<decltype(op(std::get<0>(values), std::get<1>(values)))>) {
                            op(std::get<0>(values), std::get<1>(values));
                        } else {
                            std::get<0>(values) = op(std::get<0>(values), std::get<1>(values));
                        }
                    }
                }
            }

            // Apply operation on array if depth is relevant
            if constexpr (CurrDepth >= FromDepth && CurrDepth <= ToDepth) {
                if constexpr (std::is_void_v<decltype(op(*this, cont))>) {
                    op(*this, cont);
                } else {
                    *this = op(*this, cont);
                }
            }

            return *this;
        }

        template <std::size_t FromDepth, std::size_t ToDepth, arrnd_traversal_type TraversalType,
            arrnd_traversal_result TraversalResult, arrnd_traversal_container TraversalCont, iterable_type Cont,
            typename Op, std::size_t CurrDepth = 0>
            requires(FromDepth <= ToDepth && TraversalType == arrnd_traversal_type::bfs
                && TraversalResult == arrnd_traversal_result::apply)
        constexpr auto& traverse(const Cont& cont, Op op)
        {
            // Apply operation on array if depth is relevant
            if constexpr (CurrDepth >= FromDepth && CurrDepth <= ToDepth) {
                if constexpr (std::is_void_v<decltype(op(*this, cont))>) {
                    op(*this, cont);
                } else {
                    *this = op(*this, cont);
                }
            }

            // Traverse and perform operation on leaves.
            if constexpr (arrnd_type<value_type>) {
                if constexpr (TraversalCont == arrnd_traversal_container::carry) {
                    for (auto& inner : *this) {
                        inner = inner.template traverse<FromDepth, ToDepth, TraversalType, TraversalResult,
                            TraversalCont, Cont, Op, CurrDepth + 1>(cont, op);
                    }
                } else {
                    for (auto inners : zip(zipped(*this), zipped(cont))) {
                        std::get<0>(inners)
                            = std::get<0>(inners)
                                  .template traverse<FromDepth, ToDepth, TraversalType, TraversalResult, TraversalCont,
                                      std::remove_reference_t<decltype(std::get<1>(inners))>, Op, CurrDepth + 1>(
                                      std::get<1>(inners), op);
                    }
                }
            } else if constexpr (CurrDepth + 1 >= FromDepth && CurrDepth + 1 <= ToDepth) {
                if constexpr (TraversalCont == arrnd_traversal_container::carry) {
                    for (auto& value : *this) {
                        if constexpr (std::is_void_v<decltype(op(value, cont))>) {
                            op(value, cont);
                        } else {
                            value = op(value, cont);
                        }
                    }
                } else {
                    for (auto values : zip(zipped(*this), zipped(cont))) {
                        if constexpr (std::is_void_v<decltype(op(std::get<0>(values), std::get<1>(values)))>) {
                            op(std::get<0>(values), std::get<1>(values));
                        } else {
                            std::get<0>(values) = op(std::get<0>(values), std::get<1>(values));
                        }
                    }
                }
            }

            return *this;
        }

        template <std::size_t FromDepth, std::size_t ToDepth, arrnd_traversal_type TraversalType,
            arrnd_traversal_result TraversalResult, typename UnaryOp, std::size_t CurrDepth = 0>
            requires(FromDepth <= ToDepth && TraversalType == arrnd_traversal_type::dfs
                && TraversalResult == arrnd_traversal_result::transform)
        [[nodiscard]] constexpr auto traverse(UnaryOp op) const
        {
            // The main thing done in this function is the returned type deduction.

            // Main compilation branches:
            // - If arrnd_type<value_type> and relevant(CurrentDepth)
            // - Else If arrnd_type<value_type>: Do not transform, just recursive call
            // - Else If !arrnd_type<value_type> and relevant(CurrentDepth) && relevant(CurrentDepth + 1)
            // - Else If !arrnd_type<value_type> and relevant(CurrentDepth) && !relevant(CurrentDepth + 1)
            // - Else If !arrnd_type<value_type> and !relevant(CurrentDepth) && relevant(CurrentDepth + 1)
            // - Else If !arrnd_type<value_type>: Do not transform, no recursive call

            constexpr bool is_current_depth_relevant = CurrDepth >= FromDepth && CurrDepth <= ToDepth;
            constexpr bool is_next_depth_relevant = CurrDepth + 1 >= FromDepth && CurrDepth + 1 <= ToDepth;

            if constexpr (arrnd_type<value_type> && is_current_depth_relevant) {
                using transform_t = replaced_type<decltype(std::declval<value_type>()
                                                               .template traverse<FromDepth, ToDepth, TraversalType,
                                                                   TraversalResult, UnaryOp, CurrDepth + 1>(op))>;

                transform_t res;
                res.info()
                    = transform_t::info_type(info_.dims(), info_.strides(), info_.indices_boundary(), info_.hints());
                res.shared_storage() = shared_storage_
                    ? std::allocate_shared<typename transform_t::storage_type>(
                        typename transform_t::template allocator_template_type<typename transform_t::storage_type>(),
                        shared_storage_->size())
                    : nullptr;
                if (shared_storage_) {
                    res.shared_storage()->reserve(shared_storage_->capacity());
                }

                for (auto t : zip(zipped(*this), zipped(res))) {
                    std::get<1>(t) = std::get<0>(t)
                                         .template traverse<FromDepth, ToDepth, TraversalType, TraversalResult, UnaryOp,
                                             CurrDepth + 1>(op);
                }

                if constexpr (std::is_void_v<decltype(op(res))>) {
                    op(res);
                    return res;
                } else {
                    return op(res);
                }
            } else if constexpr (arrnd_type<value_type>) {
                using transform_t = replaced_type<decltype(std::declval<value_type>()
                                                               .template traverse<FromDepth, ToDepth, TraversalType,
                                                                   TraversalResult, UnaryOp, CurrDepth + 1>(op))>;

                transform_t res;
                res.info()
                    = transform_t::info_type(info_.dims(), info_.strides(), info_.indices_boundary(), info_.hints());
                res.shared_storage() = shared_storage_
                    ? std::allocate_shared<typename transform_t::storage_type>(
                        typename transform_t::template allocator_template_type<typename transform_t::storage_type>(),
                        shared_storage_->size())
                    : nullptr;
                if (shared_storage_) {
                    res.shared_storage()->reserve(shared_storage_->capacity());
                }

                for (auto t : zip(zipped(*this), zipped(res))) {
                    std::get<1>(t) = std::get<0>(t)
                                         .template traverse<FromDepth, ToDepth, TraversalType, TraversalResult, UnaryOp,
                                             CurrDepth + 1>(op);
                }

                return res;
            } else if constexpr (!arrnd_type<value_type> && is_current_depth_relevant && !is_next_depth_relevant) {
                using transform_t
                    = std::conditional_t<std::is_void_v<decltype(op(*this))>, this_type, decltype(op(*this))>;

                transform_t res;

                if constexpr (std::is_void_v<decltype(op(*this))>) {
                    res = *this;
                    op(res);
                    return res;
                } else {
                    this_type mid = *this;
                    return op(mid);
                }
            } else if constexpr (!arrnd_type<value_type> && is_current_depth_relevant && is_next_depth_relevant) {
                using transform_t = replaced_type<std::conditional_t<std::is_void_v<decltype(op(*std::begin(*this)))>,
                    value_type, decltype(op(*std::begin(*this)))>>;

                transform_t res;
                res.info()
                    = transform_t::info_type(info_.dims(), info_.strides(), info_.indices_boundary(), info_.hints());
                res.shared_storage() = shared_storage_
                    ? std::allocate_shared<typename transform_t::storage_type>(
                        typename transform_t::template allocator_template_type<typename transform_t::storage_type>(),
                        shared_storage_->size())
                    : nullptr;
                if (shared_storage_) {
                    res.shared_storage()->reserve(shared_storage_->capacity());
                }

                for (auto t : zip(zipped(*this), zipped(res))) {
                    if constexpr (std::is_void_v<decltype(op(std::get<0>(t)))>) {
                        std::get<1>(t) = std::get<0>(t);
                        op(std::get<1>(t));
                    } else {
                        std::get<1>(t) = op(std::get<0>(t));
                    }
                }

                if constexpr (std::is_void_v<decltype(op(res))>) {
                    op(res);
                    return res;
                } else {
                    return op(res);
                }
            } else if constexpr (!arrnd_type<value_type> && !is_current_depth_relevant && is_next_depth_relevant) {
                using transform_t = replaced_type<std::conditional_t<std::is_void_v<decltype(op(*std::begin(*this)))>,
                    value_type, decltype(op(*std::begin(*this)))>>;

                transform_t res;
                res.info()
                    = transform_t::info_type(info_.dims(), info_.strides(), info_.indices_boundary(), info_.hints());
                res.shared_storage() = shared_storage_
                    ? std::allocate_shared<typename transform_t::storage_type>(
                        typename transform_t::template allocator_template_type<typename transform_t::storage_type>(),
                        shared_storage_->size())
                    : nullptr;
                if (shared_storage_) {
                    res.shared_storage()->reserve(shared_storage_->capacity());
                }

                for (auto t : zip(zipped(*this), zipped(res))) {
                    if constexpr (std::is_void_v<decltype(op(std::get<0>(t)))>) {
                        std::get<1>(t) = std::get<0>(t);
                        op(std::get<1>(t));
                    } else {
                        std::get<1>(t) = op(std::get<0>(t));
                    }
                }

                return res;
            } else {
                // No transformations
                return *this;
            }
        }

        template <std::size_t FromDepth, std::size_t ToDepth, arrnd_traversal_type TraversalType,
            arrnd_traversal_result TraversalResult, typename UnaryOp, std::size_t CurrDepth = 0>
            requires(FromDepth <= ToDepth && TraversalType == arrnd_traversal_type::bfs
                && TraversalResult == arrnd_traversal_result::transform)
        [[nodiscard]] constexpr auto traverse(UnaryOp op) const
        {
            // The main thing done in this function is the returned type deduction.

            // Main compilation branches:
            // - If arrnd_type<value_type> and relevant(CurrentDepth)
            // - Else If arrnd_type<value_type>: Do not transform, just recursive call
            // - Else If !arrnd_type<value_type> and relevant(CurrentDepth) && relevant(CurrentDepth + 1)
            // - Else If !arrnd_type<value_type> and relevant(CurrentDepth) && !relevant(CurrentDepth + 1)
            // - Else If !arrnd_type<value_type> and !relevant(CurrentDepth) && relevant(CurrentDepth + 1)
            // - Else If !arrnd_type<value_type>: Do not transform, no recursive call

            constexpr bool is_current_depth_relevant = CurrDepth >= FromDepth && CurrDepth <= ToDepth;
            constexpr bool is_next_depth_relevant = CurrDepth + 1 >= FromDepth && CurrDepth + 1 <= ToDepth;

            if constexpr (arrnd_type<value_type> && is_current_depth_relevant) {
                auto invoke = [&op](auto arr) {
                    if constexpr (std::is_void_v<decltype(op(arr))>) {
                        op(arr);
                        return arr;
                    } else {
                        return op(arr);
                    }
                };
                auto mid = invoke(*this);

                using transform_t = replaced_type<decltype((*std::begin(mid))
                                                               .template traverse<FromDepth, ToDepth, TraversalType,
                                                                   TraversalResult, UnaryOp, CurrDepth + 1>(op))>;

                transform_t res;
                res.info()
                    = transform_t::info_type(info_.dims(), info_.strides(), info_.indices_boundary(), info_.hints());
                res.shared_storage() = shared_storage_
                    ? std::allocate_shared<typename transform_t::storage_type>(
                        typename transform_t::template allocator_template_type<typename transform_t::storage_type>(),
                        shared_storage_->size())
                    : nullptr;
                if (shared_storage_) {
                    res.shared_storage()->reserve(shared_storage_->capacity());
                }

                for (auto t : zip(zipped(mid), zipped(res))) {
                    std::get<1>(t) = std::get<0>(t)
                                         .template traverse<FromDepth, ToDepth, TraversalType, TraversalResult, UnaryOp,
                                             CurrDepth + 1>(op);
                }

                return res;
            } else if constexpr (arrnd_type<value_type>) {
                using transform_t = replaced_type<decltype(std::declval<value_type>()
                                                               .template traverse<FromDepth, ToDepth, TraversalType,
                                                                   TraversalResult, UnaryOp, CurrDepth + 1>(op))>;

                transform_t res;
                res.info()
                    = transform_t::info_type(info_.dims(), info_.strides(), info_.indices_boundary(), info_.hints());
                res.shared_storage() = shared_storage_
                    ? std::allocate_shared<typename transform_t::storage_type>(
                        typename transform_t::template allocator_template_type<typename transform_t::storage_type>(),
                        shared_storage_->size())
                    : nullptr;
                if (shared_storage_) {
                    res.shared_storage()->reserve(shared_storage_->capacity());
                }

                for (auto t : zip(zipped(*this), zipped(res))) {
                    std::get<1>(t) = std::get<0>(t)
                                         .template traverse<FromDepth, ToDepth, TraversalType, TraversalResult, UnaryOp,
                                             CurrDepth + 1>(op);
                }

                return res;
            } else if constexpr (!arrnd_type<value_type> && is_current_depth_relevant && !is_next_depth_relevant) {
                using transform_t
                    = std::conditional_t<std::is_void_v<decltype(op(*this))>, this_type, decltype(op(*this))>;

                transform_t res;

                if constexpr (std::is_void_v<decltype(op(*this))>) {
                    res = *this;
                    op(res);
                    return res;
                } else {
                    this_type mid = *this;
                    return op(mid);
                }
            } else if constexpr (!arrnd_type<value_type> && is_current_depth_relevant && is_next_depth_relevant) {
                auto invoke = [&op](auto arr) {
                    if constexpr (std::is_void_v<decltype(op(arr))>) {
                        op(arr);
                        return arr;
                    } else {
                        return op(arr);
                    }
                };
                auto mid = invoke(*this);

                using transform_t = replaced_type<std::conditional_t<std::is_void_v<decltype(op(*std::begin(mid)))>,
                    value_type, decltype(op(*std::begin(mid)))>>;

                transform_t res;
                res.info()
                    = transform_t::info_type(info_.dims(), info_.strides(), info_.indices_boundary(), info_.hints());
                res.shared_storage() = shared_storage_
                    ? std::allocate_shared<typename transform_t::storage_type>(
                        typename transform_t::template allocator_template_type<typename transform_t::storage_type>(),
                        shared_storage_->size())
                    : nullptr;
                if (shared_storage_) {
                    res.shared_storage()->reserve(shared_storage_->capacity());
                }

                for (auto t : zip(zipped(mid), zipped(res))) {
                    if constexpr (std::is_void_v<decltype(op(std::get<0>(t)))>) {
                        std::get<1>(t) = std::get<0>(t);
                        op(std::get<1>(t));
                    } else {
                        std::get<1>(t) = op(std::get<0>(t));
                    }
                }

                return res;
            } else if constexpr (!arrnd_type<value_type> && !is_current_depth_relevant && is_next_depth_relevant) {
                using transform_t = replaced_type<std::conditional_t<std::is_void_v<decltype(op(*std::begin(*this)))>,
                    value_type, decltype(op(*std::begin(*this)))>>;

                transform_t res;
                res.info()
                    = transform_t::info_type(info_.dims(), info_.strides(), info_.indices_boundary(), info_.hints());
                res.shared_storage() = shared_storage_
                    ? std::allocate_shared<typename transform_t::storage_type>(
                        typename transform_t::template allocator_template_type<typename transform_t::storage_type>(),
                        shared_storage_->size())
                    : nullptr;
                if (shared_storage_) {
                    res.shared_storage()->reserve(shared_storage_->capacity());
                }

                for (auto t : zip(zipped(*this), zipped(res))) {
                    if constexpr (std::is_void_v<decltype(op(std::get<0>(t)))>) {
                        std::get<1>(t) = std::get<0>(t);
                        op(std::get<1>(t));
                    } else {
                        std::get<1>(t) = op(std::get<0>(t));
                    }
                }

                return res;
            } else {
                // No transformations
                return *this;
            }
        }

        template <std::size_t FromDepth, std::size_t ToDepth, arrnd_traversal_type TraversalType,
            arrnd_traversal_result TraversalResult, arrnd_traversal_container TraversalCont, iterable_type Cont,
            typename Op, std::size_t CurrDepth = 0>
            requires(FromDepth <= ToDepth && TraversalType == arrnd_traversal_type::dfs
                && TraversalResult == arrnd_traversal_result::transform)
        [[nodiscard]] constexpr auto traverse(const Cont& cont, Op op) const
        {
            // The main thing done in this function is the returned type deduction.

            // Main compilation branches:
            // - If arrnd_type<value_type> and relevant(CurrentDepth)
            // - Else If arrnd_type<value_type>: Do not transform, just recursive call
            // - Else If !arrnd_type<value_type> and relevant(CurrentDepth) && relevant(CurrentDepth + 1)
            // - Else If !arrnd_type<value_type> and relevant(CurrentDepth) && !relevant(CurrentDepth + 1)
            // - Else If !arrnd_type<value_type> and !relevant(CurrentDepth) && relevant(CurrentDepth + 1)
            // - Else If !arrnd_type<value_type>: Do not transform, no recursive call

            constexpr bool is_current_depth_relevant = CurrDepth >= FromDepth && CurrDepth <= ToDepth;
            constexpr bool is_next_depth_relevant = CurrDepth + 1 >= FromDepth && CurrDepth + 1 <= ToDepth;

            if constexpr (arrnd_type<value_type> && is_current_depth_relevant) {
                if constexpr (TraversalCont == arrnd_traversal_container::carry) {
                    using transform_t
                        = replaced_type<decltype(std::declval<value_type>()
                                                     .template traverse<FromDepth, ToDepth, TraversalType,
                                                         TraversalResult, TraversalCont, Cont, Op, CurrDepth + 1>(
                                                         cont, op))>;

                    transform_t res;
                    res.info() = transform_t::info_type(
                        info_.dims(), info_.strides(), info_.indices_boundary(), info_.hints());
                    res.shared_storage() = shared_storage_ ? std::allocate_shared<typename transform_t::storage_type>(
                                               typename transform_t::template allocator_template_type<
                                                   typename transform_t::storage_type>(),
                                               shared_storage_->size())
                                                           : nullptr;
                    if (shared_storage_) {
                        res.shared_storage()->reserve(shared_storage_->capacity());
                    }

                    for (auto t : zip(zipped(*this), zipped(res))) {
                        std::get<1>(t) = std::get<0>(t)
                                             .template traverse<FromDepth, ToDepth, TraversalType, TraversalResult,
                                                 TraversalCont, Cont, Op, CurrDepth + 1>(cont, op);
                    }

                    if constexpr (std::is_void_v<decltype(op(res, cont))>) {
                        op(res, cont);
                        return res;
                    } else {
                        return op(res, cont);
                    }
                } else {
                    using transform_t = replaced_type<
                        decltype(std::declval<value_type>()
                                     .template traverse<FromDepth, ToDepth, TraversalType, TraversalResult,
                                         TraversalCont, std::remove_reference_t<decltype(*std::begin(cont))>, Op,
                                         CurrDepth + 1>(*std::begin(cont), op))>;

                    transform_t res;
                    res.info() = transform_t::info_type(
                        info_.dims(), info_.strides(), info_.indices_boundary(), info_.hints());
                    res.shared_storage() = shared_storage_ ? std::allocate_shared<typename transform_t::storage_type>(
                                               typename transform_t::template allocator_template_type<
                                                   typename transform_t::storage_type>(),
                                               shared_storage_->size())
                                                           : nullptr;
                    if (shared_storage_) {
                        res.shared_storage()->reserve(shared_storage_->capacity());
                    }

                    for (auto t : zip(zipped(*this), zipped(res), zipped(cont))) {
                        std::get<1>(t)
                            = std::get<0>(t)
                                  .template traverse<FromDepth, ToDepth, TraversalType, TraversalResult, TraversalCont,
                                      std::remove_reference_t<decltype(std::get<2>(t))>, Op, CurrDepth + 1>(
                                      std::get<2>(t), op);
                    }

                    if constexpr (std::is_void_v<decltype(op(res, cont))>) {
                        op(res, cont);
                        return res;
                    } else {
                        return op(res, cont);
                    }
                }
            } else if constexpr (arrnd_type<value_type>) {
                if constexpr (TraversalCont == arrnd_traversal_container::carry) {
                    using transform_t
                        = replaced_type<decltype(std::declval<value_type>()
                                                     .template traverse<FromDepth, ToDepth, TraversalType,
                                                         TraversalResult, TraversalCont, Cont, Op, CurrDepth + 1>(
                                                         cont, op))>;

                    transform_t res;
                    res.info() = transform_t::info_type(
                        info_.dims(), info_.strides(), info_.indices_boundary(), info_.hints());
                    res.shared_storage() = shared_storage_ ? std::allocate_shared<typename transform_t::storage_type>(
                                               typename transform_t::template allocator_template_type<
                                                   typename transform_t::storage_type>(),
                                               shared_storage_->size())
                                                           : nullptr;
                    if (shared_storage_) {
                        res.shared_storage()->reserve(shared_storage_->capacity());
                    }

                    for (auto t : zip(zipped(*this), zipped(res))) {
                        std::get<1>(t) = std::get<0>(t)
                                             .template traverse<FromDepth, ToDepth, TraversalType, TraversalResult,
                                                 TraversalCont, Cont, Op, CurrDepth + 1>(cont, op);
                    }

                    return res;
                } else {
                    using transform_t = replaced_type<
                        decltype(std::declval<value_type>()
                                     .template traverse<FromDepth, ToDepth, TraversalType, TraversalResult,
                                         TraversalCont, std::remove_reference_t<decltype(*std::begin(cont))>, Op,
                                         CurrDepth + 1>(*std::begin(cont), op))>;

                    transform_t res;
                    res.info() = transform_t::info_type(
                        info_.dims(), info_.strides(), info_.indices_boundary(), info_.hints());
                    res.shared_storage() = shared_storage_ ? std::allocate_shared<typename transform_t::storage_type>(
                                               typename transform_t::template allocator_template_type<
                                                   typename transform_t::storage_type>(),
                                               shared_storage_->size())
                                                           : nullptr;
                    if (shared_storage_) {
                        res.shared_storage()->reserve(shared_storage_->capacity());
                    }

                    for (auto t : zip(zipped(*this), zipped(res), zipped(cont))) {
                        std::get<1>(t)
                            = std::get<0>(t)
                                  .template traverse<FromDepth, ToDepth, TraversalType, TraversalResult, TraversalCont,
                                      std::remove_reference_t<decltype(std::get<2>(t))>, Op, CurrDepth + 1>(
                                      std::get<2>(t), op);
                    }

                    return res;
                }
            } else if constexpr (!arrnd_type<value_type> && is_current_depth_relevant && !is_next_depth_relevant) {
                using transform_t = std::conditional_t<std::is_void_v<decltype(op(*this, cont))>, this_type,
                    decltype(op(*this, cont))>;

                transform_t res;

                if constexpr (std::is_void_v<decltype(op(*this, cont))>) {
                    res = *this;
                    op(res, cont);
                    return res;
                } else {
                    this_type mid = *this;
                    return op(mid, cont);
                }
            } else if constexpr (!arrnd_type<value_type> && is_current_depth_relevant && is_next_depth_relevant) {
                if constexpr (TraversalCont == arrnd_traversal_container::carry) {
                    using transform_t
                        = replaced_type<std::conditional_t<std::is_void_v<decltype(op(*std::begin(*this), cont))>,
                            value_type, decltype(op(*std::begin(*this), cont))>>;

                    transform_t res;
                    res.info() = transform_t::info_type(
                        info_.dims(), info_.strides(), info_.indices_boundary(), info_.hints());
                    res.shared_storage() = shared_storage_ ? std::allocate_shared<typename transform_t::storage_type>(
                                               typename transform_t::template allocator_template_type<
                                                   typename transform_t::storage_type>(),
                                               shared_storage_->size())
                                                           : nullptr;
                    if (shared_storage_) {
                        res.shared_storage()->reserve(shared_storage_->capacity());
                    }

                    for (auto t : zip(zipped(*this), zipped(res))) {
                        if constexpr (std::is_void_v<decltype(op(std::get<0>(t), cont))>) {
                            std::get<1>(t) = std::get<0>(t);
                            op(std::get<1>(t), cont);
                        } else {
                            std::get<1>(t) = op(std::get<0>(t), cont);
                        }
                    }

                    if constexpr (std::is_void_v<decltype(op(res, cont))>) {
                        op(res, cont);
                        return res;
                    } else {
                        return op(res, cont);
                    }
                } else {
                    using transform_t = replaced_type<
                        std::conditional_t<std::is_void_v<decltype(op(*std::begin(*this), *std::begin(cont)))>,
                            value_type, decltype(op(*std::begin(*this), *std::begin(cont)))>>;

                    transform_t res;
                    res.info() = transform_t::info_type(
                        info_.dims(), info_.strides(), info_.indices_boundary(), info_.hints());
                    res.shared_storage() = shared_storage_ ? std::allocate_shared<typename transform_t::storage_type>(
                                               typename transform_t::template allocator_template_type<
                                                   typename transform_t::storage_type>(),
                                               shared_storage_->size())
                                                           : nullptr;
                    if (shared_storage_) {
                        res.shared_storage()->reserve(shared_storage_->capacity());
                    }

                    for (auto t : zip(zipped(*this), zipped(res), zipped(cont))) {
                        if constexpr (std::is_void_v<decltype(op(std::get<0>(t), std::get<2>(t)))>) {
                            std::get<1>(t) = std::get<0>(t);
                            op(std::get<1>(t), std::get<2>(t));
                        } else {
                            std::get<1>(t) = op(std::get<0>(t), std::get<2>(t));
                        }
                    }

                    if constexpr (std::is_void_v<decltype(op(res, cont))>) {
                        op(res, cont);
                        return res;
                    } else {
                        return op(res, cont);
                    }
                }
            } else if constexpr (!arrnd_type<value_type> && !is_current_depth_relevant && is_next_depth_relevant) {
                if constexpr (TraversalCont == arrnd_traversal_container::carry) {
                    using transform_t
                        = replaced_type<std::conditional_t<std::is_void_v<decltype(op(*std::begin(*this), cont))>,
                            value_type, decltype(op(*std::begin(*this), cont))>>;

                    transform_t res;
                    res.info() = transform_t::info_type(
                        info_.dims(), info_.strides(), info_.indices_boundary(), info_.hints());
                    res.shared_storage() = shared_storage_ ? std::allocate_shared<typename transform_t::storage_type>(
                                               typename transform_t::template allocator_template_type<
                                                   typename transform_t::storage_type>(),
                                               shared_storage_->size())
                                                           : nullptr;
                    if (shared_storage_) {
                        res.shared_storage()->reserve(shared_storage_->capacity());
                    }

                    for (auto t : zip(zipped(*this), zipped(res))) {
                        if constexpr (std::is_void_v<decltype(op(std::get<0>(t), cont))>) {
                            std::get<1>(t) = std::get<0>(t);
                            op(std::get<1>(t), cont);
                        } else {
                            std::get<1>(t) = op(std::get<0>(t), cont);
                        }
                    }

                    return res;
                } else {
                    using transform_t = replaced_type<
                        std::conditional_t<std::is_void_v<decltype(op(*std::begin(*this), *std::begin(cont)))>,
                            value_type, decltype(op(*std::begin(*this), *std::begin(cont)))>>;

                    transform_t res;
                    res.info() = transform_t::info_type(
                        info_.dims(), info_.strides(), info_.indices_boundary(), info_.hints());
                    res.shared_storage() = shared_storage_ ? std::allocate_shared<typename transform_t::storage_type>(
                                               typename transform_t::template allocator_template_type<
                                                   typename transform_t::storage_type>(),
                                               shared_storage_->size())
                                                           : nullptr;
                    if (shared_storage_) {
                        res.shared_storage()->reserve(shared_storage_->capacity());
                    }

                    for (auto t : zip(zipped(*this), zipped(res), zipped(cont))) {
                        if constexpr (std::is_void_v<decltype(op(std::get<0>(t), std::get<2>(t)))>) {
                            std::get<1>(t) = std::get<0>(t);
                            op(std::get<1>(t), std::get<2>(t));
                        } else {
                            std::get<1>(t) = op(std::get<0>(t), std::get<2>(t));
                        }
                    }

                    return res;
                }
            } else {
                // No transformations
                return *this;
            }
        }

        template <std::size_t FromDepth, std::size_t ToDepth, arrnd_traversal_type TraversalType,
            arrnd_traversal_result TraversalResult, arrnd_traversal_container TraversalCont, iterable_type Cont,
            typename Op, std::size_t CurrDepth = 0>
            requires(FromDepth <= ToDepth && TraversalType == arrnd_traversal_type::bfs
                && TraversalResult == arrnd_traversal_result::transform)
        [[nodiscard]] constexpr auto traverse(const Cont& cont, Op op) const
        {
            // The main thing done in this function is the returned type deduction.

            // Main compilation branches:
            // - If arrnd_type<value_type> and relevant(CurrentDepth)
            // - Else If arrnd_type<value_type>: Do not transform, just recursive call
            // - Else If !arrnd_type<value_type> and relevant(CurrentDepth) && relevant(CurrentDepth + 1)
            // - Else If !arrnd_type<value_type> and relevant(CurrentDepth) && !relevant(CurrentDepth + 1)
            // - Else If !arrnd_type<value_type> and !relevant(CurrentDepth) && relevant(CurrentDepth + 1)
            // - Else If !arrnd_type<value_type>: Do not transform, no recursive call

            constexpr bool is_current_depth_relevant = CurrDepth >= FromDepth && CurrDepth <= ToDepth;
            constexpr bool is_next_depth_relevant = CurrDepth + 1 >= FromDepth && CurrDepth + 1 <= ToDepth;

            if constexpr (arrnd_type<value_type> && is_current_depth_relevant) {
                auto invoke = [&op, &cont](auto arr) {
                    if constexpr (std::is_void_v<decltype(op(arr, cont))>) {
                        op(arr, cont);
                        return arr;
                    } else {
                        return op(arr, cont);
                    }
                };
                auto mid = invoke(*this);

                if constexpr (TraversalCont == arrnd_traversal_container::carry) {
                    using transform_t
                        = replaced_type<decltype((*std::begin(mid))
                                                     .template traverse<FromDepth, ToDepth, TraversalType,
                                                         TraversalResult, TraversalCont, Cont, Op, CurrDepth + 1>(
                                                         cont, op))>;

                    transform_t res;
                    res.info() = transform_t::info_type(
                        info_.dims(), info_.strides(), info_.indices_boundary(), info_.hints());
                    res.shared_storage() = shared_storage_ ? std::allocate_shared<typename transform_t::storage_type>(
                                               typename transform_t::template allocator_template_type<
                                                   typename transform_t::storage_type>(),
                                               shared_storage_->size())
                                                           : nullptr;
                    if (shared_storage_) {
                        res.shared_storage()->reserve(shared_storage_->capacity());
                    }

                    for (auto t : zip(zipped(mid), zipped(res))) {
                        std::get<1>(t) = std::get<0>(t)
                                             .template traverse<FromDepth, ToDepth, TraversalType, TraversalResult,
                                                 TraversalCont, Cont, Op, CurrDepth + 1>(cont, op);
                    }

                    return res;
                } else {
                    using transform_t = replaced_type<
                        decltype((*std::begin(mid))
                                     .template traverse<FromDepth, ToDepth, TraversalType, TraversalResult,
                                         TraversalCont, std::remove_reference_t<decltype(*std::begin(cont))>, Op,
                                         CurrDepth + 1>(*std::begin(cont), op))>;

                    transform_t res;
                    res.info() = transform_t::info_type(
                        info_.dims(), info_.strides(), info_.indices_boundary(), info_.hints());
                    res.shared_storage() = shared_storage_ ? std::allocate_shared<typename transform_t::storage_type>(
                                               typename transform_t::template allocator_template_type<
                                                   typename transform_t::storage_type>(),
                                               shared_storage_->size())
                                                           : nullptr;
                    if (shared_storage_) {
                        res.shared_storage()->reserve(shared_storage_->capacity());
                    }

                    for (auto t : zip(zipped(mid), zipped(res), zipped(cont))) {
                        std::get<1>(t)
                            = std::get<0>(t)
                                  .template traverse<FromDepth, ToDepth, TraversalType, TraversalResult, TraversalCont,
                                      std::remove_reference_t<decltype(std::get<2>(t))>, Op, CurrDepth + 1>(
                                      std::get<2>(t), op);
                    }

                    return res;
                }
            } else if constexpr (arrnd_type<value_type>) {
                if constexpr (TraversalCont == arrnd_traversal_container::carry) {
                    using transform_t
                        = replaced_type<decltype(std::declval<value_type>()
                                                     .template traverse<FromDepth, ToDepth, TraversalType,
                                                         TraversalResult, TraversalCont, Cont, Op, CurrDepth + 1>(
                                                         cont, op))>;

                    transform_t res;
                    res.info() = transform_t::info_type(
                        info_.dims(), info_.strides(), info_.indices_boundary(), info_.hints());
                    res.shared_storage() = shared_storage_ ? std::allocate_shared<typename transform_t::storage_type>(
                                               typename transform_t::template allocator_template_type<
                                                   typename transform_t::storage_type>(),
                                               shared_storage_->size())
                                                           : nullptr;
                    if (shared_storage_) {
                        res.shared_storage()->reserve(shared_storage_->capacity());
                    }

                    for (auto t : zip(zipped(*this), zipped(res))) {
                        std::get<1>(t) = std::get<0>(t)
                                             .template traverse<FromDepth, ToDepth, TraversalType, TraversalResult,
                                                 TraversalCont, Cont, Op, CurrDepth + 1>(cont, op);
                    }

                    return res;
                } else {
                    using transform_t = replaced_type<
                        decltype(std::declval<value_type>()
                                     .template traverse<FromDepth, ToDepth, TraversalType, TraversalResult,
                                         TraversalCont, std::remove_reference_t<decltype(*std::begin(cont))>, Op,
                                         CurrDepth + 1>(*std::begin(cont), op))>;

                    transform_t res;
                    res.info() = transform_t::info_type(
                        info_.dims(), info_.strides(), info_.indices_boundary(), info_.hints());
                    res.shared_storage() = shared_storage_ ? std::allocate_shared<typename transform_t::storage_type>(
                                               typename transform_t::template allocator_template_type<
                                                   typename transform_t::storage_type>(),
                                               shared_storage_->size())
                                                           : nullptr;
                    if (shared_storage_) {
                        res.shared_storage()->reserve(shared_storage_->capacity());
                    }

                    for (auto t : zip(zipped(*this), zipped(res), zipped(cont))) {
                        std::get<1>(t)
                            = std::get<0>(t)
                                  .template traverse<FromDepth, ToDepth, TraversalType, TraversalResult, TraversalCont,
                                      std::remove_reference_t<decltype(std::get<2>(t))>, Op, CurrDepth + 1>(
                                      std::get<2>(t), op);
                    }

                    return res;
                }
            } else if constexpr (!arrnd_type<value_type> && is_current_depth_relevant && !is_next_depth_relevant) {
                using transform_t = std::conditional_t<std::is_void_v<decltype(op(*this, cont))>, this_type,
                    decltype(op(*this, cont))>;

                transform_t res;

                if constexpr (std::is_void_v<decltype(op(*this, cont))>) {
                    res = *this;
                    op(res, cont);
                    return res;
                } else {
                    this_type mid = *this;
                    return op(mid, cont);
                }
            } else if constexpr (!arrnd_type<value_type> && is_current_depth_relevant && is_next_depth_relevant) {
                auto invoke = [&op, &cont](auto arr) {
                    if constexpr (std::is_void_v<decltype(op(arr, cont))>) {
                        op(arr, cont);
                        return arr;
                    } else {
                        return op(arr, cont);
                    }
                };
                auto mid = invoke(*this);

                if constexpr (TraversalCont == arrnd_traversal_container::carry) {
                    using transform_t
                        = replaced_type<std::conditional_t<std::is_void_v<decltype(op(*std::begin(mid), cont))>,
                            value_type, decltype(op(*std::begin(mid), cont))>>;

                    transform_t res;
                    res.info() = transform_t::info_type(
                        info_.dims(), info_.strides(), info_.indices_boundary(), info_.hints());
                    res.shared_storage() = shared_storage_ ? std::allocate_shared<typename transform_t::storage_type>(
                                               typename transform_t::template allocator_template_type<
                                                   typename transform_t::storage_type>(),
                                               shared_storage_->size())
                                                           : nullptr;
                    if (shared_storage_) {
                        res.shared_storage()->reserve(shared_storage_->capacity());
                    }

                    for (auto t : zip(zipped(mid), zipped(res))) {
                        if constexpr (std::is_void_v<decltype(op(std::get<0>(t), cont))>) {
                            std::get<1>(t) = std::get<0>(t);
                            op(std::get<1>(t), cont);
                        } else {
                            std::get<1>(t) = op(std::get<0>(t), cont);
                        }
                    }

                    return res;
                } else {
                    using transform_t = replaced_type<
                        std::conditional_t<std::is_void_v<decltype(op(*std::begin(mid), *std::begin(cont)))>,
                            value_type, decltype(op(*std::begin(mid), *std::begin(cont)))>>;

                    transform_t res;
                    res.info() = transform_t::info_type(
                        info_.dims(), info_.strides(), info_.indices_boundary(), info_.hints());
                    res.shared_storage() = shared_storage_ ? std::allocate_shared<typename transform_t::storage_type>(
                                               typename transform_t::template allocator_template_type<
                                                   typename transform_t::storage_type>(),
                                               shared_storage_->size())
                                                           : nullptr;
                    if (shared_storage_) {
                        res.shared_storage()->reserve(shared_storage_->capacity());
                    }

                    for (auto t : zip(zipped(mid), zipped(res), zipped(cont))) {
                        if constexpr (std::is_void_v<decltype(op(std::get<0>(t), std::get<2>(t)))>) {
                            std::get<1>(t) = std::get<0>(t);
                            op(std::get<1>(t), std::get<2>(t));
                        } else {
                            std::get<1>(t) = op(std::get<0>(t), std::get<2>(t));
                        }
                    }

                    return res;
                }
            } else if constexpr (!arrnd_type<value_type> && !is_current_depth_relevant && is_next_depth_relevant) {
                if constexpr (TraversalCont == arrnd_traversal_container::carry) {
                    using transform_t
                        = replaced_type<std::conditional_t<std::is_void_v<decltype(op(*std::begin(*this), cont))>,
                            value_type, decltype(op(*std::begin(*this), cont))>>;

                    transform_t res;
                    res.info() = transform_t::info_type(
                        info_.dims(), info_.strides(), info_.indices_boundary(), info_.hints());
                    res.shared_storage() = shared_storage_ ? std::allocate_shared<typename transform_t::storage_type>(
                                               typename transform_t::template allocator_template_type<
                                                   typename transform_t::storage_type>(),
                                               shared_storage_->size())
                                                           : nullptr;
                    if (shared_storage_) {
                        res.shared_storage()->reserve(shared_storage_->capacity());
                    }

                    for (auto t : zip(zipped(*this), zipped(res))) {
                        if constexpr (std::is_void_v<decltype(op(std::get<0>(t), cont))>) {
                            std::get<1>(t) = std::get<0>(t);
                            op(std::get<1>(t), cont);
                        } else {
                            std::get<1>(t) = op(std::get<0>(t), cont);
                        }
                    }

                    return res;
                } else {
                    using transform_t = replaced_type<
                        std::conditional_t<std::is_void_v<decltype(op(*std::begin(*this), *std::begin(cont)))>,
                            value_type, decltype(op(*std::begin(*this), *std::begin(cont)))>>;

                    transform_t res;
                    res.info() = transform_t::info_type(
                        info_.dims(), info_.strides(), info_.indices_boundary(), info_.hints());
                    res.shared_storage() = shared_storage_ ? std::allocate_shared<typename transform_t::storage_type>(
                                               typename transform_t::template allocator_template_type<
                                                   typename transform_t::storage_type>(),
                                               shared_storage_->size())
                                                           : nullptr;
                    if (shared_storage_) {
                        res.shared_storage()->reserve(shared_storage_->capacity());
                    }

                    for (auto t : zip(zipped(*this), zipped(res), zipped(cont))) {
                        if constexpr (std::is_void_v<decltype(op(std::get<0>(t), std::get<2>(t)))>) {
                            std::get<1>(t) = std::get<0>(t);
                            op(std::get<1>(t), std::get<2>(t));
                        } else {
                            std::get<1>(t) = op(std::get<0>(t), std::get<2>(t));
                        }
                    }

                    return res;
                }
            } else {
                // No transformations
                return *this;
            }
        }

        template <std::size_t AtDepth = this_type::depth, typename UnaryOp>
        [[nodiscard]] constexpr auto transform(UnaryOp op) const
        {
            return traverse<AtDepth + 1, AtDepth + 1, arrnd_traversal_type::dfs, arrnd_traversal_result::transform>(op);
        }

        template <std::size_t AtDepth = this_type::depth, arrnd_type Arrnd, typename BinaryOp>
        [[nodiscard]] constexpr auto transform(const Arrnd& arr, BinaryOp op) const
        {
            auto transform_impl = [&op](const auto& lhs, const auto& rhs) {
                using transform_t
                    = decltype(lhs.template traverse<1, 1, arrnd_traversal_type::dfs, arrnd_traversal_result::transform,
                               arrnd_traversal_container::propagate>(rhs, op));

                if (lhs.empty() && rhs.empty()) {
                    return transform_t{};
                }

                if (isscalar(rhs.info())) {
                    return lhs.template transform<0>(
                        [&op, &rval = std::as_const(rhs(0))](const auto& lval) -> typename transform_t::value_type {
                            if constexpr (std::is_void_v<decltype(op(std::forward<decltype(lval)>(lval), rval))>) {
                                op(std::forward<decltype(lval)>(lval), rval);
                            } else {
                                return op(std::forward<decltype(lval)>(lval), rval);
                            }
                        });
                }

                if (isscalar(lhs.info())) {
                    return rhs.template transform<0>(
                        [&op, &lval = std::as_const(lhs(0))](auto&& rval) -> typename transform_t::value_type {
                            if constexpr (std::is_void_v<decltype(op(lval, std::forward<decltype(rval)>(rval)))>) {
                                op(lval, std::forward<decltype(rval)>(rval));
                            } else {
                                return op(lval, std::forward<decltype(rval)>(rval));
                            }
                        });
                }

                if (total(lhs.info()) == total(rhs.info())) {
                    return lhs.template traverse<1, 1, arrnd_traversal_type::dfs, arrnd_traversal_result::transform,
                        arrnd_traversal_container::propagate>(rhs, op);
                }

                if (total(lhs.info()) > total(rhs.info()) && size(lhs.info()) == size(rhs.info())) {
                    auto zipped_dims = (zip(zipped(lhs.info().dims()), zipped(rhs.info().dims())));
                    if (std::any_of(std::begin(zipped_dims), std::end(zipped_dims), [](auto t) {
                            return std::get<0>(t) % std::get<1>(t) != 0;
                        })) {
                        throw std::invalid_argument("invalid input array dims for boradcast");
                    }

                    transform_t res;
                    res.info() = transform_t::info_type(
                        lhs.info().dims(), lhs.info().strides(), lhs.info().indices_boundary(), lhs.info().hints());
                    res.shared_storage() = lhs.shared_storage()
                        ? std::allocate_shared<typename transform_t::storage_type>(
                            typename transform_t::template allocator_template_type<
                                typename transform_t::storage_type>(),
                            lhs.shared_storage()->size())
                        : nullptr;
                    if (lhs.shared_storage()) {
                        res.shared_storage()->reserve(lhs.shared_storage()->capacity());
                    }

                    typename transform_t::info_type::storage_traits_type::template replaced_type<
                        typename transform_t::window_type>::storage_type windows(size(lhs.info()));

                    std::transform(std::begin(rhs.info().dims()), std::end(rhs.info().dims()), std::begin(windows),
                        [](typename transform_t::difference_type d) {
                            return typename transform_t::window_type(
                                typename transform_t::window_type::interval_type{0, d}, arrnd_window_type::complete);
                        });

                    for (typename transform_t::windows_slider_type slider(lhs.info(), windows); slider; ++slider) {
                        auto slc = lhs[*slider];
                        res[*slider] = slc.template traverse<1, 1, arrnd_traversal_type::dfs,
                            arrnd_traversal_result::transform, arrnd_traversal_container::propagate>(rhs, op);
                    }

                    return res;
                }

                if (total(lhs.info()) < total(rhs.info()) && size(lhs.info()) == size(rhs.info())) {
                    auto zipped_dims = (zip(zipped(lhs.info().dims()), zipped(rhs.info().dims())));
                    if (std::any_of(std::begin(zipped_dims), std::end(zipped_dims), [](auto t) {
                            return std::get<1>(t) % std::get<0>(t) != 0;
                        })) {
                        throw std::invalid_argument("invalid input array dims for boradcast");
                    }

                    transform_t res;
                    res.info() = transform_t::info_type(
                        rhs.info().dims(), rhs.info().strides(), rhs.info().indices_boundary(), rhs.info().hints());
                    res.shared_storage() = rhs.shared_storage()
                        ? std::allocate_shared<typename transform_t::storage_type>(
                            typename transform_t::template allocator_template_type<
                                typename transform_t::storage_type>(),
                            rhs.shared_storage()->size())
                        : nullptr;
                    if (rhs.shared_storage()) {
                        res.shared_storage()->reserve(rhs.shared_storage()->capacity());
                    }

                    typename transform_t::info_type::storage_traits_type::template replaced_type<
                        typename transform_t::window_type>::storage_type windows(size(rhs.info()));

                    std::transform(std::begin(lhs.info().dims()), std::end(lhs.info().dims()), std::begin(windows),
                        [](typename transform_t::difference_type d) {
                            return typename transform_t::window_type(
                                typename transform_t::window_type::interval_type{0, d}, arrnd_window_type::complete);
                        });

                    for (typename transform_t::windows_slider_type slider(rhs.info(), windows); slider; ++slider) {
                        auto slc = rhs[*slider];
                        res[*slider] = lhs.template traverse<1, 1, arrnd_traversal_type::dfs,
                            arrnd_traversal_result::transform, arrnd_traversal_container::propagate>(slc, op);
                    }

                    return res;
                }

                throw std::invalid_argument("invalid input array dims");
            };

            return traverse<AtDepth, AtDepth, arrnd_traversal_type::dfs, arrnd_traversal_result::transform,
                arrnd_traversal_container::propagate>(arr, transform_impl);
        }

        template <std::size_t AtDepth = this_type::depth, typename UnaryOp>
        constexpr auto& apply(UnaryOp op)
        {
            return traverse<AtDepth + 1, AtDepth + 1, arrnd_traversal_type::dfs, arrnd_traversal_result::apply>(op);
        }

        template <std::size_t AtDepth = this_type::depth, arrnd_type Arrnd, typename BinaryOp>
        constexpr auto& apply(const Arrnd& arr, BinaryOp op)
        {
            auto apply_impl = [&op](auto& lhs, auto& rhs) {
                if (lhs.empty() && rhs.empty()) {
                    return lhs;
                }

                constexpr bool is_void_op = std::is_same_v<
                    std::invoke_result_t<BinaryOp, decltype(*std::begin(lhs)), decltype(*std::begin(rhs))>, void>;

                if (isscalar(rhs.info())) {
                    return lhs.template apply<0>([&op, &rval = rhs(0)](auto&& lval) {
                        if constexpr (std::is_void_v<decltype(op(std::forward<decltype(lval)>(lval), rval))>) {
                            op(std::forward<decltype(lval)>(lval), rval);
                        } else {
                            return op(std::forward<decltype(lval)>(lval), rval);
                        }
                    });
                }

                if (total(lhs.info()) == total(rhs.info())) {
                    return lhs.template traverse<1, 1, arrnd_traversal_type::dfs, arrnd_traversal_result::apply,
                        arrnd_traversal_container::propagate>(rhs, op);
                }

                if (total(lhs.info()) > total(rhs.info()) && size(lhs.info()) == size(rhs.info())) {
                    auto zipped_dims = (zip(zipped(lhs.info().dims()), zipped(rhs.info().dims())));
                    if (std::any_of(std::begin(zipped_dims), std::end(zipped_dims), [](auto t) {
                            return std::get<0>(t) % std::get<1>(t) != 0;
                        })) {
                        throw std::invalid_argument("invalid input array dims for boradcast");
                    }

                    using lhs_t = std::remove_cvref_t<decltype(lhs)>;

                    typename lhs_t::info_type::storage_traits_type::template replaced_type<
                        typename lhs_t::window_type>::storage_type windows(size(lhs.info()));

                    std::transform(std::begin(rhs.info().dims()), std::end(rhs.info().dims()), std::begin(windows),
                        [](typename lhs_t::difference_type d) {
                            return typename lhs_t::window_type(
                                typename lhs_t::window_type::interval_type{0, d}, arrnd_window_type::complete);
                        });

                    for (typename lhs_t::windows_slider_type slider(lhs.info(), windows); slider; ++slider) {
                        auto slc = lhs[*slider];
                        slc.template traverse<1, 1, arrnd_traversal_type::dfs, arrnd_traversal_result::apply,
                            arrnd_traversal_container::propagate>(rhs, op);
                    }

                    return lhs;
                }

                throw std::invalid_argument("invalid input array dims");
            };

            return traverse<AtDepth, AtDepth, arrnd_traversal_type::dfs, arrnd_traversal_result::apply,
                arrnd_traversal_container::propagate>(arr, apply_impl);
        }

        template <std::size_t AtDepth = this_type::depth, typename BinaryOp>
        [[nodiscard]] constexpr auto reduce(BinaryOp op) const
        {
            auto reduce_impl = [&op](const auto& arr) {
                using reduce_t = std::invoke_result_t<BinaryOp, typename std::remove_cvref_t<decltype(arr)>::value_type,
                    typename std::remove_cvref_t<decltype(arr)>::value_type>;

                if (arr.empty()) {
                    return reduce_t{};
                }

                return std::reduce(std::next(arr.begin(), 1), arr.end(), static_cast<reduce_t>(*(arr.begin())), op);
            };

            return traverse<AtDepth, AtDepth, arrnd_traversal_type::dfs, arrnd_traversal_result::transform>(
                reduce_impl);
        }

        template <std::size_t AtDepth = this_type::depth, typename U, typename BinaryOp>
        [[nodiscard]] constexpr auto fold(const U& init, BinaryOp op) const
        {
            auto fold_impl = [&op, &init](const auto& arr) {
                using fold_t
                    = std::invoke_result_t<BinaryOp, U, typename std::remove_cvref_t<decltype(arr)>::value_type>;

                if (arr.empty()) {
                    return fold_t{};
                }

                return std::reduce(std::begin(arr), std::end(arr), init, op);
            };

            return traverse<AtDepth, AtDepth, arrnd_traversal_type::dfs, arrnd_traversal_result::transform>(fold_impl);
        }

        template <std::size_t AtDepth = this_type::depth, typename BinaryOp>
        [[nodiscard]] constexpr auto reduce(size_type axis, BinaryOp op) const
        {
            auto reduce_impl = [axis, &op](const auto& arr) {
                using reduce_t = typename std::remove_cvref_t<decltype(arr)>::template replaced_type<
                    std::invoke_result_t<BinaryOp, typename std::remove_cvref_t<decltype(arr)>::value_type,
                        typename std::remove_cvref_t<decltype(arr)>::value_type>>;

                auto new_info = oc::arrnd::reduce(arr.info(), axis);
                reduce_t res(std::move(new_info),
                    std::allocate_shared<typename reduce_t::storage_type>(
                        typename reduce_t::template allocator_template_type<typename reduce_t::storage_type>(),
                        total(new_info)));

                assert(total(res.info()) == total(arr.info()) / arr.info().dims()[axis]);

                size_type reduction_size = arr.info().dims()[axis];

                auto ritb = arr.begin(size(arr.info()) - axis - 1, arrnd_returned_element_iterator_tag{});
                auto rite = ritb + reduction_size;

                for (auto& value : res) {
                    value = std::reduce(ritb + 1, rite, static_cast<typename reduce_t::value_type>(*ritb), op);
                    ritb = rite;
                    rite += reduction_size;
                }

                return res;
            };

            return traverse<AtDepth, AtDepth, arrnd_traversal_type::dfs, arrnd_traversal_result::transform>(
                reduce_impl);
        }

        template <std::size_t AtDepth = this_type::depth, iterator_type InputIt, typename BinaryOp>
        [[nodiscard]] constexpr auto fold(size_type axis, InputIt first_init, InputIt last_init, BinaryOp op) const
        {
            auto fold_impl = [axis, &op, &first_init, &last_init](const auto& arr) {
                using fold_t =
                    typename std::remove_cvref_t<decltype(arr)>::template replaced_type<std::invoke_result_t<BinaryOp,
                        iterator_value_t<InputIt>, typename std::remove_cvref_t<decltype(arr)>::value_type>>;

                if (total(arr.info()) / arr.info().dims()[axis] != std::distance(first_init, last_init)) {
                    throw std::invalid_argument("different size inits from input array");
                }

                auto new_info = oc::arrnd::reduce(arr.info(), axis);
                fold_t res(std::move(new_info),
                    std::allocate_shared<typename fold_t::storage_type>(
                        typename fold_t::template allocator_template_type<typename fold_t::storage_type>(),
                        total(new_info)));

                assert(total(res.info()) == total(arr.info()) / arr.info().dims()[axis]);

                size_type reduction_size = arr.info().dims()[axis];

                auto ritb = arr.begin(size(arr.info()) - axis - 1, arrnd_returned_element_iterator_tag{});
                auto rite = ritb + reduction_size;

                auto init = first_init;

                for (auto& value : res) {
                    value = std::reduce(ritb, rite, *init, op);
                    ritb = rite;
                    rite += reduction_size;
                    ++init;
                }

                return res;
            };

            return traverse<AtDepth, AtDepth, arrnd_traversal_type::dfs, arrnd_traversal_result::transform>(fold_impl);
        }

        template <std::size_t AtDepth = this_type::depth, iterable_type Cont, typename BinaryOp>
        [[nodiscard]] constexpr auto fold(size_type axis, const Cont& inits, BinaryOp&& op) const
        {
            return fold<AtDepth>(axis, std::begin(inits), std::end(inits), std::forward<BinaryOp>(op));
        }

        template <std::size_t AtDepth = this_type::depth, typename U, typename BinaryOp>
        [[nodiscard]] constexpr auto fold(size_type axis, std::initializer_list<U> inits, BinaryOp&& op) const
        {
            return fold<AtDepth>(axis, inits.begin(), inits.end(), std::forward<BinaryOp>(op));
        }

        template <std::size_t AtDepth = this_type::depth, typename Pred>
            requires(!iterable_type<Pred>)
        [[nodiscard]] constexpr auto filter(Pred pred) const
        {
            auto filter_impl = [&pred](const auto& arr) {
                using filter_t = std::remove_cvref_t<decltype(arr)>;

                if (arr.empty()) {
                    return filter_t{};
                }

                filter_t res({total(arr.info())});

                size_type count = 0;

                std::copy_if(arr.cbegin(), arr.cend(), res.begin(), [&pred, &count](const auto& value) {
                    if (pred(value)) {
                        ++count;
                        return true;
                    }
                    return false;
                });

                return count == 0 ? filter_t{} : res.resize({count});
            };

            return traverse<AtDepth, AtDepth, arrnd_traversal_type::dfs, arrnd_traversal_result::transform>(
                filter_impl);
        }

        template <std::size_t AtDepth = this_type::depth, iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto filter(InputIt first, InputIt last) const
        {
            auto filter_impl = [&first, &last](const auto& arr) {
                using filter_t = std::remove_cvref_t<decltype(arr)>;

                if (std::distance(first, last) > total(arr.info())) {
                    throw std::invalid_argument("different total size between arr and selector");
                }

                if (arr.empty()) {
                    return filter_t{};
                }

                filter_t res({static_cast<size_type>(std::distance(first, last))});
                auto rit = std::begin(res);

                for (auto ind_it = first; ind_it != last; ++ind_it) {
                    *rit = arr[*ind_it];
                    ++rit;
                }

                return res;
            };

            return traverse<AtDepth, AtDepth, arrnd_traversal_type::dfs, arrnd_traversal_result::transform>(
                filter_impl);
        }

        template <std::size_t AtDepth = this_type::depth, iterable_of_type_integral Cont>
            requires(arrnd_type<Cont>)
        [[nodiscard]] constexpr auto filter(const Cont& selector) const
        {
            auto filter_impl = [&selector](const auto& arr) {
                using filter_t = std::remove_cvref_t<decltype(arr)>;

                // mask - should be inside arr dims limit
                if constexpr (std::is_same_v<bool, typename Cont::value_type>) {
                    if (!std::equal(std::begin(arr.info().dims()), std::end(arr.info().dims()),
                            std::begin(selector.info().dims()), std::end(selector.info().dims()))) {
                        throw std::invalid_argument("different dims between arr and selector");
                    }

                    if (arr.empty()) {
                        return filter_t{};
                    }

                    filter_t res({total(arr.info())});
                    auto rit = res.begin();
                    size_type count = 0;

                    for (auto t : zip(zipped(arr), zipped(selector))) {
                        if (std::get<1>(t)) {
                            *rit = std::get<0>(t);
                            ++rit;
                            ++count;
                        }
                    }

                    return count == 0 ? filter_t{} : res.resize({count});
                } else {
                    auto first_ind = std::begin(selector);
                    auto last_ind = std::end(selector);

                    if (std::distance(first_ind, last_ind) > total(arr.info())) {
                        throw std::invalid_argument("different total size between arr and selector");
                    }

                    if (arr.empty()) {
                        return filter_t{};
                    }

                    filter_t res({static_cast<size_type>(std::distance(first_ind, last_ind))});
                    auto rit = std::begin(res);

                    for (auto ind : selector) {
                        *rit = arr[ind];
                        ++rit;
                    }

                    return res;
                }
            };

            return traverse<AtDepth, AtDepth, arrnd_traversal_type::dfs, arrnd_traversal_result::transform>(
                filter_impl);
        }

        template <std::size_t AtDepth = this_type::depth, iterable_of_type_integral Cont>
            requires(!arrnd_type<Cont>)
        [[nodiscard]] constexpr auto filter(const Cont& selector) const
        {
            return filter<AtDepth>(std::begin(selector), std::end(selector));
        }

        template <std::size_t AtDepth = this_type::depth>
        [[nodiscard]] constexpr auto filter(std::initializer_list<size_type> inds) const
        {
            return filter<AtDepth>(inds.begin(), inds.end());
        }

        template <std::size_t AtDepth = this_type::depth, typename Pred>
            requires(!iterable_type<Pred>)
        [[nodiscard]] constexpr auto find(Pred pred) const
        {
            auto find_impl = [&pred](const auto& arr) {
                using find_t = typename std::remove_cvref_t<decltype(arr)>::template replaced_type<size_type>;

                if (arr.empty()) {
                    return find_t{};
                }

                find_t res({total(arr.info())});

                size_type count = 0;
                auto rit = res.begin();

                for (typename std::remove_cvref_t<decltype(arr)>::indexer_type indexer(arr.info()); indexer;
                     ++indexer) {
                    if (pred(arr[*indexer])) {
                        *rit = *indexer;
                        ++rit;
                        ++count;
                    }
                }

                return count == 0 ? find_t{} : res.resize({count});
            };

            return traverse<AtDepth, AtDepth, arrnd_traversal_type::dfs, arrnd_traversal_result::transform>(find_impl);
        }

        template <std::size_t AtDepth = this_type::depth, arrnd_type Arrnd>
        [[nodiscard]] constexpr auto find(const Arrnd& mask) const
        {
            auto find_impl = [&mask](const auto& arr) {
                using find_t = typename std::remove_cvref_t<decltype(arr)>::template replaced_type<size_type>;

                if (!std::equal(std::begin(arr.info().dims()), std::end(arr.info().dims()),
                        std::begin(mask.info().dims()), std::end(mask.info().dims()))) {
                    throw std::invalid_argument("different dims between arr and mask");
                }

                if (arr.empty()) {
                    return find_t{};
                }

                find_t res({total(arr.info())});

                auto rit = res.begin();
                size_type count = 0;

                typename std::remove_cvref_t<decltype(arr)>::indexer_type arr_indexer(arr.info());
                typename Arrnd::indexer_type mask_indexer(mask.info());

                for (; arr_indexer && mask_indexer; ++arr_indexer, ++mask_indexer) {
                    if (mask[*mask_indexer]) {
                        *rit = *arr_indexer;
                        ++rit;
                        ++count;
                    }
                }

                return count == 0 ? find_t{} : res.resize({count});
            };

            return traverse<AtDepth, AtDepth, arrnd_traversal_type::dfs, arrnd_traversal_result::transform>(find_impl);
        }

        [[nodiscard]] constexpr auto expand(size_type axis, size_type division = 0) const
        {
            using expand_t = replaced_type<this_type>;

            if (empty()) {
                return expand_t{};
            }

            if (axis < 0 || axis >= size(info_)) {
                throw std::invalid_argument("invalid axis");
            }

            if (division > info_.dims()[axis]) {
                throw std::invalid_argument("invalid division for axis dim");
            }

            auto fixed_div = division > 0 ? std::min(info_.dims()[axis], division) : info_.dims()[axis];

            typename expand_t::info_type::extent_storage_type new_dims(size(info_), 1);
            new_dims[axis] = fixed_div;

            expand_t res(new_dims);
            auto res_it = res.begin();

            auto curr_div = fixed_div;
            auto curr_axis_dim = info_.dims()[axis];
            auto curr_ival_width = static_cast<size_type>(std::ceil(curr_axis_dim / static_cast<double>(curr_div)));

            size_type res_total = 0;

            windows_slider_type slider(info_, axis,
                window_type(typename window_type::interval_type(0, curr_ival_width), arrnd_window_type::partial));

            while (curr_div > 0) {
                *(res_it++) = (*this)[std::make_pair((*slider).cbegin(), (*slider).cend())];

                slider += curr_ival_width;

                --curr_div;
                curr_axis_dim -= curr_ival_width;

                if (curr_div > 0) {
                    curr_ival_width = static_cast<size_type>(std::ceil(curr_axis_dim / static_cast<double>(curr_div)));
                    slider.modify_window(axis,
                        window_type(
                            typename window_type::interval_type(0, curr_ival_width), arrnd_window_type::partial));
                }

                ++res_total;
            }

            if (res_total != fixed_div) {
                new_dims[axis] = res_total;
                return res.resize(new_dims);
            }
            return res;
        }

        // collapse should only used on valid expanded arrays.
        // returns reference to expanded array creator if
        // exists, or new collpsed array otherwise.
        [[nodiscard]] constexpr auto collapse(bool ignore_creators = false) const
            requires(!this_type::is_flat)
        {
            using collpase_t = value_type;

            if (empty()) {
                return collpase_t{};
            }

            // from array creator

            if (!ignore_creators) {
                bool all_nested_values_have_the_same_creator
                    = std::adjacent_find(cbegin(), cend(),
                          [](const collpase_t& vt1, const collpase_t& vt2) {
                              return !vt1.creator() || !vt2.creator() || vt1.creator() != vt2.creator();
                          })
                    == cend();

                if (all_nested_values_have_the_same_creator && (*this)(0).creator() != nullptr) {
                    return *((*this)(0).creator());
                }
            }

            // from one collapsed array

            if (total(info_) == 1) {
                return (*this)(0);
            }

            // from assumed axis hint

            if (std::count(std::begin(info_.dims()), std::end(info_.dims()), 1) != size(info_) - 1) {
                throw std::exception("invalid exapnded array dims structure");
            }

            auto assumed_axis
                = std::find(std::begin(info_.dims()), std::end(info_.dims()), total(info_)) - std::begin(info_.dims());

            auto element_it = begin();

            collpase_t res = (*element_it).clone();
            ++element_it;

            while (element_it != end()) {
                res = res.push_back((*element_it).clone(), assumed_axis);
                ++element_it;
            }

            return res;
        }

        template <typename UnaryOp>
        [[nodiscard]] constexpr auto slide(size_type axis, window_type window, UnaryOp op) const
        {
            using slide_t = replaced_type<std::invoke_result_t<UnaryOp, this_type>>;

            if (empty()) {
                return slide_t{};
            }

            if (axis < 0 || axis >= size(info_)) {
                throw std::invalid_argument("invalid axis");
            }

            windows_slider_type slider(info_, axis, window);

            size_type res_total = window.type == arrnd_window_type::complete
                ? info_.dims()[axis] - window.ival.stop() + window.ival.start() + 1
                : info_.dims()[axis];

            slide_t res({res_total});
            if (res.empty()) {
                return res;
            }

            auto res_it = res.begin();

            for (; slider && res_it != res.end(); ++slider, ++res_it) {
                *res_it = op((*this)[std::make_pair((*slider).cbegin(), (*slider).cend())]);
            }

            return res;
        }

        template <typename BinaryOp, typename UnaryOp>
        [[nodiscard]] constexpr auto accumulate(
            size_type axis, window_type window, BinaryOp reduce_op, UnaryOp transform_op) const
        {
            using transform_t = std::invoke_result_t<UnaryOp, this_type>;
            using reduce_t = std::invoke_result_t<BinaryOp, transform_t, transform_t>;
            using accumulate_t = replaced_type<reduce_t>;

            if (empty()) {
                return accumulate_t{};
            }

            if (axis < 0 || axis >= size(info_)) {
                throw std::invalid_argument("invalid axis");
            }

            size_type axis_dim = *std::next(info_.dims().cbegin(), axis);

            windows_slider_type slider(info_, axis, window);

            size_type res_total = window.type == arrnd_window_type::complete
                ? info_.dims()[axis] - window.ival.stop() + window.ival.start() + 1
                : info_.dims()[axis];

            accumulate_t res({res_total});
            if (res.empty()) {
                return res;
            }

            auto res_it = res.begin();

            *res_it = transform_op((*this)[std::make_pair((*slider).cbegin(), (*slider).cend())]);
            auto prev = *res_it;
            ++res_it;
            ++slider;

            for (; slider && res_it != res.end(); ++slider, ++res_it) {
                *res_it = reduce_op(prev, transform_op((*this)[std::make_pair((*slider).cbegin(), (*slider).cend())]));
                prev = *res_it;
            }

            return res;
        }

        template <typename UnaryOp>
        constexpr auto browse(size_type page_size, UnaryOp&& op) const
        {
            if constexpr (std::is_void_v<std::invoke_result_t<UnaryOp, this_type>>) {
                using browse_t = this_type;

                if (empty()) {
                    return browse_t{};
                }

                if (size(info_) < page_size) {
                    throw std::invalid_argument("invalid page size - smaller than number of dims");
                }

                if (size(info_) == page_size) {
                    op(*this);
                    return *this;
                }

                pages(page_size).template apply<0>(std::forward<UnaryOp>(op));
                return *this;
            } else {
                using type_t = std::invoke_result_t<UnaryOp, this_type>;
                using browse_t = std::conditional_t<arrnd_depths_match<type_t, this_type>, type_t,
                    replaced_type<type_t>>;

                if (empty()) {
                    return browse_t{};
                }

                if (size(info_) < page_size) {
                    throw std::invalid_argument("invalid page size - smaller than number of dims");
                }

                auto invoke = [&op](auto page) {
                    if constexpr (arrnd_depths_match<type_t, this_type>) {
                        return op(page);
                    } else { // in case that the returned type of op is not arrnd_type, then it should not be void returned type
                        return browse_t({1}, {op(page)});
                    }
                };

                if (size(info_) == page_size) {
                    return invoke(*this);
                }

                return pages(page_size).template transform<0>(invoke).book();
            }
        }

        [[nodiscard]] constexpr auto pages(size_type page_size = 2) const
        {
            using pages_t = replaced_type<this_type>;

            if (empty()) {
                return pages_t{};
            }

            if (page_size <= 0 || page_size > size(info_)) {
                throw std::invalid_argument("invalid page size");
            }

            if (page_size == size(info_)) {
                return pages_t({1}, {*this});
            }

            pages_t pages(std::begin(info_.dims()), std::next(std::begin(info_.dims()), size(info_) - page_size));

            for (auto indexer = typename pages_t::indexer_type(pages.info()); indexer; ++indexer) {
                const auto& subs = indexer.subs();
                auto page = *this;
                for (size_type axis = 0; axis < size(info_) - page_size; ++axis) {
                    page = page[boundary_type::at(subs[axis])];
                }

                pages[*indexer] = page;
            }

            return pages;
        }

        [[nodiscard]] constexpr auto book(bool ignore_creators = false) const
            requires(!this_type::is_flat)
        {
            if (empty()) {
                return value_type{};
            }

            if (!ignore_creators) {
                bool all_pages_source_known = std::all_of(cbegin(), cend(), [](const auto& page) {
                    return page.creator() != nullptr;
                });

                bool all_pages_from_same_source = std::adjacent_find(cbegin(), cend(),
                                                      [](const auto& page1, const auto& page2) {
                                                          return page1.creator() != page2.creator();
                                                      })
                    == cend();

                if (all_pages_source_known && all_pages_from_same_source) {
                    return *((*this)(0).creator());
                }
            }

            bool all_pages_with_same_dimes
                = std::adjacent_find(cbegin(), cend(),
                      [](const auto& page1, const auto& page2) {
                          return !std::equal(page1.info().dims().cbegin(), page1.info().dims().cend(),
                              page2.info().dims().cbegin(), page2.info().dims().cend());
                      })
                == cend();

            if (!all_pages_with_same_dimes) {
                throw std::exception("unable to book pages with different dims");
            }

            size_type this_size = size(info_);
            size_type page_size = size((*this)(0).info());

            const auto& this_dims = info_.dims();
            const auto& page_dims = (*this)(0).info().dims();

            typename info_type::extent_storage_type new_dims(this_size + page_size);
            std::copy(std::begin(this_dims), std::end(this_dims), std::begin(new_dims));
            std::copy(std::begin(page_dims), std::end(page_dims), std::next(std::begin(new_dims), this_size));

            value_type res(new_dims);
            auto res_it = res.begin();

            for (const auto& page : *this) {
                for (auto page_it = page.begin(); page_it != page.end() && res_it != res.end(); ++page_it, ++res_it) {
                    *res_it = *page_it;
                }
            }

            return res;
        }

        template <iterator_of_type_integral AxesIt>
        [[nodiscard]] constexpr auto split(AxesIt first_axis, AxesIt last_axis, size_type division) const
        {
            using split_t = replaced_type<this_type>;

            if (empty()) {
                return split_t{};
            }

            if (!std::is_sorted(first_axis, last_axis)) {
                throw std::invalid_argument("invalid axes - not sorted");
            }

            if (std::adjacent_find(first_axis, last_axis) != last_axis) {
                throw std::invalid_argument("invalid axes - not unique");
            }

            if (std::distance(first_axis, last_axis) < 0
                || std::distance(first_axis, last_axis) > info_.dims().size()) {
                throw std::invalid_argument("invalid number of axes");
            }

            if (std::any_of(first_axis, last_axis, [&](auto axis) {
                    return axis < 0 || axis >= size(info_);
                })) {
                throw std::invalid_argument("invalid axes for dims");
            }

            if (std::any_of(std::begin(info_.dims()), std::end(info_.dims()), [division](auto dim) {
                    return division <= 0 || division > dim;
                })) {
                throw std::invalid_argument("invalid division");
            }

            // calculate dimensions according to number of slices in each dimension

            typename info_type::extent_storage_type new_dims(size(info_));
            if (std::distance(first_axis, last_axis) == 0) {
                std::fill(std::begin(new_dims), std::end(new_dims), division);
            } else {
                std::fill(std::begin(new_dims), std::end(new_dims), 1);
                std::for_each(first_axis, last_axis, [&new_dims, division](auto axis) {
                    new_dims[axis] = division;
                });
            }

            // --------------------------------------------------------------------

            split_t slices(new_dims);
            auto slices_it = slices.begin();
            size_type actual_num_slices = 0;

            std::function<void(this_type, size_type)> split_impl;

            split_impl = [&](this_type arr, size_type current_depth) -> void {
                if (arr.empty()) {
                    return;
                }

                if (current_depth == 0) {
                    *slices_it = arr;
                    ++slices_it;
                    ++actual_num_slices;
                    return;
                }

                if (std::distance(first_axis, last_axis) > 0
                    && std::find(first_axis, last_axis, size(arr.info()) - current_depth) == last_axis) {
                    split_impl(arr(boundary_type::full(), size(arr.info()) - current_depth), current_depth - 1);
                    return;
                }

                auto exp = arr.expand(size(arr.info()) - current_depth, division);
                for (auto value : exp) {
                    split_impl(value, current_depth - 1);
                }
            };

            split_impl(*this, size(info_));

            assert(actual_num_slices == total(slices.info()));

            return slices;
        }
        template <iterable_of_type_integral Cont>
        [[nodiscard]] constexpr auto split(const Cont& axes, size_type division) const
        {
            return split(std::begin(axes), std::end(axes), division);
        }
        [[nodiscard]] constexpr auto split(std::initializer_list<size_type> axes, size_type division) const
        {
            return split(axes.begin(), axes.end(), division);
        }

        template <iterator_of_type_integral AxesIt, iterator_of_type_integral IndsIt>
        [[nodiscard]] constexpr auto split(AxesIt first_axis, AxesIt last_axis, IndsIt first_ind, IndsIt last_ind) const
        {
            using split_t = replaced_type<this_type>;

            if (empty()) {
                return split_t{};
            }

            if (!std::is_sorted(first_ind, last_ind)) {
                throw std::invalid_argument("invalid indices - not sorted");
            }

            if (std::adjacent_find(first_ind, last_ind) != last_ind) {
                throw std::invalid_argument("invalid indices - not unique");
            }

            if (!std::is_sorted(first_axis, last_axis)) {
                throw std::invalid_argument("invalid axes - not sorted");
            }

            if (std::adjacent_find(first_axis, last_axis) != last_axis) {
                throw std::invalid_argument("invalid axes - not unique");
            }

            if (std::distance(first_axis, last_axis) < 0
                || std::distance(first_axis, last_axis) > info_.dims().size()) {
                throw std::invalid_argument("invalid number of axes");
            }

            if (std::any_of(first_axis, last_axis, [&](auto axis) {
                    return axis < 0 || axis >= size(info_);
                })) {
                throw std::invalid_argument("invalid axes for dims");
            }

            // calculate dimensions according to number of slices in each dimension

            typename info_type::extent_storage_type new_dims(info_.dims().size());
            std::fill(new_dims.begin(), new_dims.end(), 1);
            auto calc_num_slices_at_dim = [&](auto indf, auto indl) {
                if (*indf == 0) {
                    return std::distance(indf, indl);
                }
                return std::distance(indf, indl) + 1;
            };
            if (std::distance(first_axis, last_axis) == 0) {
                std::for_each(new_dims.begin(), new_dims.end(), [&](size_type& d) {
                    d = calc_num_slices_at_dim(first_ind, last_ind);
                });
            } else {
                std::for_each(first_axis, last_axis, [&](size_type axis) {
                    new_dims[axis] = calc_num_slices_at_dim(first_ind, last_ind);
                });
            }

            // --------------------------------------------------------------------

            split_t slices(new_dims);
            auto slices_it = slices.begin();
            size_type actual_num_slices = 0;

            std::function<void(this_type, size_type)> split_impl;

            split_impl = [&](this_type arr, size_type current_depth) -> void {
                if (arr.empty()) {
                    return;
                }

                if (current_depth == 0) {
                    *slices_it = arr;
                    ++slices_it;
                    ++actual_num_slices;
                    return;
                }

                if (std::distance(first_axis, last_axis) > 0
                    && std::find(first_axis, last_axis, size(arr.info()) - current_depth) == last_axis) {
                    split_impl(arr(boundary_type::full(), size(arr.info()) - current_depth), current_depth - 1);
                    return;
                }

                size_type current_dim = arr.info().dims()[size(arr.info()) - current_depth];

                if (std::distance(first_ind, last_ind) <= 0 || std::distance(first_ind, last_ind) > current_dim) {
                    throw std::invalid_argument("invalid number of indices");
                }
                if (std::any_of(first_ind, last_ind, [current_dim](size_type ind) {
                        return ind < 0 || ind >= current_dim;
                    })) {
                    throw std::invalid_argument("invalid indices for dims");
                }

                size_type prev_ind = *first_ind;
                auto ind_it = first_ind;
                ++ind_it;
                if (prev_ind > 0) {
                    split_impl(arr(boundary_type{0, prev_ind}, size(arr.info()) - current_depth), current_depth - 1);
                }
                size_type current_ind = prev_ind;
                for (; ind_it != last_ind; ++ind_it) {
                    current_ind = *ind_it;
                    if (prev_ind < current_ind) {
                        split_impl(arr(boundary_type{prev_ind, current_ind}, size(arr.info()) - current_depth),
                            current_depth - 1);
                    }
                    prev_ind = current_ind;
                }
                if (current_ind < current_dim) {
                    split_impl(arr(boundary_type{current_ind, current_dim}, size(arr.info()) - current_depth),
                        current_depth - 1);
                }
            };

            split_impl(*this, size(info_));

            assert(actual_num_slices == total(slices.info()));

            return slices;
        }

        template <iterable_of_type_integral AxesCont, iterable_of_type_integral Cont>
        [[nodiscard]] constexpr auto split(const AxesCont& axes, const Cont& inds) const
        {
            return split(std::begin(axes), std::end(axes), std::begin(inds), std::end(inds));
        }

        [[nodiscard]] constexpr auto split(
            std::initializer_list<size_type> axes, std::initializer_list<size_type> inds) const
        {
            return split(axes.begin(), axes.end(), inds.begin(), inds.end());
        }

        template <iterator_of_type_integral AxesIt, iterator_of_type_integral IndsIt>
        [[nodiscard]] constexpr auto exclude(
            AxesIt first_axis, AxesIt last_axis, IndsIt first_ind, IndsIt last_ind) const
        {
            using exclude_t = replaced_type<this_type>;

            if (empty()) {
                return exclude_t();
            }

            if (!std::is_sorted(first_ind, last_ind)) {
                throw std::invalid_argument("invalid indices - not sorted");
            }

            if (std::adjacent_find(first_ind, last_ind) != last_ind) {
                throw std::invalid_argument("invalid indices - not unique");
            }

            if (!std::is_sorted(first_axis, last_axis)) {
                throw std::invalid_argument("invalid axes - not sorted");
            }

            if (std::adjacent_find(first_axis, last_axis) != last_axis) {
                throw std::invalid_argument("invalid axes - not unique");
            }

            if (std::distance(first_axis, last_axis) < 0
                || std::distance(first_axis, last_axis) > info_.dims().size()) {
                throw std::invalid_argument("invalid number of axes");
            }

            if (std::any_of(first_axis, last_axis, [&](auto axis) {
                    return axis < 0 || axis >= size(info_);
                })) {
                throw std::invalid_argument("invalid axes for dims");
            }

            // calculate dimensions according to number of slices in each dimension

            auto calc_num_slices_at_dim = [](auto indf, auto indl, size_type dim) {
                size_type num_slices = 0;
                size_type prev = *indf;
                if (prev > 0) {
                    ++num_slices;
                }
                ++indf;
                size_type current_ind = prev;
                for (auto it = indf; it != indl; ++it) {
                    current_ind = *it;
                    if (prev < current_ind + 1) {
                        ++num_slices;
                    }
                    prev = current_ind;
                }
                if (current_ind + 1 < dim) {
                    ++num_slices;
                }
                return num_slices;
            };

            typename info_type::extent_storage_type new_dims(size(info_), 1);
            if (std::distance(first_axis, last_axis) == 0) {
                std::transform(std::begin(info_.dims()), std::end(info_.dims()), std::begin(new_dims), [&](auto dim) {
                    return calc_num_slices_at_dim(first_ind, last_ind, dim);
                });
            } else {
                std::for_each(first_axis, last_axis, [&](auto axis) {
                    new_dims[axis] = calc_num_slices_at_dim(first_ind, last_ind, info_.dims()[axis]);
                });
            }

            // --------------------------------------------------------------------

            exclude_t slices(new_dims);
            auto slices_it = slices.begin();
            size_type actual_num_slices = 0;

            std::function<void(this_type, size_type)> exclude_impl;

            exclude_impl = [&](this_type arr, size_type current_depth) -> void {
                if (arr.empty()) {
                    return;
                }

                if (current_depth == 0) {
                    *slices_it = arr;
                    ++slices_it;
                    ++actual_num_slices;

                    return;
                }

                if (std::distance(first_axis, last_axis) > 0
                    && std::find(first_axis, last_axis, size(arr.info()) - current_depth) == last_axis) {
                    exclude_impl(arr(boundary_type::full(), size(arr.info()) - current_depth), current_depth - 1);
                    return;
                }

                size_type current_dim = arr.info().dims()[size(arr.info()) - current_depth];

                if (std::distance(first_ind, last_ind) <= 0 || std::distance(first_ind, last_ind) > current_dim) {
                    throw std::invalid_argument("invalid number of indices");
                }
                if (std::any_of(first_ind, last_ind, [current_dim](size_type ind) {
                        return ind < 0 || ind >= current_dim;
                    })) {
                    throw std::invalid_argument("invalid indices for dims");
                }

                size_type prev_ind = *first_ind;
                auto ind_it = first_ind;
                ++ind_it;
                if (prev_ind > 0) {
                    exclude_impl(arr(boundary_type{0, prev_ind}, size(arr.info()) - current_depth), current_depth - 1);
                }
                size_type current_ind = prev_ind;
                for (; ind_it != last_ind; ++ind_it) {
                    current_ind = *ind_it;
                    if (prev_ind + 1 < current_ind) {
                        exclude_impl(arr(boundary_type{prev_ind + 1, current_ind}, size(arr.info()) - current_depth),
                            current_depth - 1);
                    }
                    prev_ind = current_ind;
                }
                if (current_ind + 1 < current_dim) {
                    exclude_impl(arr(boundary_type{current_ind + 1, current_dim}, size(arr.info()) - current_depth),
                        current_depth - 1);
                }
            };

            exclude_impl(*this, size(info_));

            assert(actual_num_slices == total(slices.info()));

            return slices;
        }

        template <iterable_of_type_integral AxesCont, iterable_of_type_integral Cont>
        [[nodiscard]] constexpr auto exclude(const AxesCont& axes, const Cont& inds) const
        {
            return exclude(std::begin(axes), std::end(axes), std::begin(inds), std::end(inds));
        }

        [[nodiscard]] constexpr auto exclude(
            std::initializer_list<size_type> axes, std::initializer_list<size_type> inds) const
        {
            return exclude(axes.begin(), axes.end(), inds.begin(), inds.end());
        }

        [[nodiscard]] constexpr auto merge() const
            requires(!this_type::is_flat)
        {
            if (empty()) {
                return value_type{};
            }

            this_type res = clone().reduce<0>(size(info_) - 1, [&](value_type a, value_type b) {
                return a.push_back(b, size(info_) - 1);
            });

            for (difference_type axis = size(info_) - 2; axis >= 0; --axis) {
                res = res.reduce<0>(axis, [axis](value_type a, value_type b) {
                    return a.push_back(b, axis);
                });
            }

            assert(total(res.info()) == 1);

            return res(0);
        }

        [[nodiscard]] constexpr this_type squeeze(arrnd_squeeze_type type = arrnd_squeeze_type::full) const
        {
            this_type squeezed = *this;
            squeezed.info_ = oc::arrnd::squeeze(info_, type);

            return squeezed;
        }

        template <typename Comp>
        constexpr this_type& sort(Comp comp)
        {
            std::sort(begin(), end(), [&comp](const auto& lhs, const auto& rhs) {
                return comp(lhs, rhs);
            });

            return *this;
        }

        template <typename Comp>
        [[nodiscard]] constexpr this_type& sort(size_type axis, Comp&& comp)
        {
            if (empty()) {
                return *this;
            }

            if (axis < 0 || axis >= size(info_)) {
                throw std::invalid_argument("invalid axis");
            }

            auto sorted_tmp = expand(axis).sort(std::forward<Comp>(comp)).collapse(true);
            std::copy(sorted_tmp.cbegin(), sorted_tmp.cend(), begin());

            return *this;
        }

        template <typename Comp>
        [[nodiscard]] constexpr bool is_sorted(Comp comp) const
        {
            if (empty()) {
                return true;
            }

            return std::is_sorted(cbegin(), cend(), [&comp](const auto& lhs, const auto& rhs) {
                return comp(lhs, rhs);
            });
        }

        template <typename Comp>
        [[nodiscard]] constexpr bool is_sorted(size_type axis, Comp&& comp) const
        {
            if (empty()) {
                return true;
            }

            if (axis < 0 || axis >= size(info_)) {
                throw std::invalid_argument("invalid axis");
            }

            return expand(axis).is_sorted(std::forward<Comp>(comp));
        }

        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr this_type& reorder(InputIt first_order, InputIt last_order)
        {
            if (empty()) {
                return *this;
            }

            if (std::distance(first_order, last_order) != total(info_)) {
                throw std::invalid_argument("invalid order - different from total elements");
            }

            typename info_type::extent_storage_type order(first_order, last_order);

            auto z = zip(zipped(order), zipped(*this));
            std::sort(z.begin(), z.end(), [](const auto& t1, const auto& t2) {
                return std::get<0>(t1) < std::get<0>(t2);
            });

            return *this;
        }

        template <iterable_of_type_integral Cont>
        constexpr this_type& reorder(const Cont& order)
        {
            return reorder(std::begin(order), std::end(order));
        }

        constexpr this_type& reorder(std::initializer_list<size_type> order)
        {
            return reorder(order.begin(), order.end());
        }

        template <iterator_of_type_integral InputIt>
        constexpr this_type& reorder(size_type axis, InputIt first_order, InputIt last_order)
        {
            if (empty()) {
                return *this;
            }

            if (axis < 0 || axis >= size(info_)) {
                throw std::invalid_argument("invalid axis");
            }

            if (std::distance(first_order, last_order) != info_.dims()[axis]) {
                throw std::invalid_argument("invalid order - different from total elements at axis");
            }

            typename info_type::extent_storage_type order(first_order, last_order);

            auto expanded = expand(axis);

            auto z = zip(zipped(order), zipped(expanded));
            std::sort(z.begin(), z.end(), [](const auto& t1, const auto& t2) {
                return std::get<0>(t1) < std::get<0>(t2);
            });

            auto sorted_tmp = expanded.collapse(true);
            std::copy(sorted_tmp.cbegin(), sorted_tmp.cend(), begin());

            return *this;
        }

        template <iterable_of_type_integral Cont>
        constexpr this_type& reorder(size_type axis, const Cont& order)
        {
            return reorder(axis, std::begin(order), std::end(order));
        }

        constexpr this_type& reorder(size_type axis, std::initializer_list<size_type> order)
        {
            return reorder(axis, order.begin(), order.end());
        }

        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto adjacent_indices(InputIt first_sub, InputIt last_sub, size_type offset = 1) const
        {
            using adjacent_indices_t = replaced_type<size_type>;

            if (empty()) {
                return adjacent_indices_t{};
            }

            if (std::distance(first_sub, last_sub) != size(info_)) {
                throw std::invalid_argument("invalid number of subs");
            }

            if (offset <= 0) {
                throw std::invalid_argument("invalid offset <= 0");
            }

            auto signed_offset = static_cast<difference_type>(offset);

            auto compute_num_adj = [](difference_type ndims, difference_type soffset) {
                difference_type base1 = 3 + 2 * (soffset - 1);
                difference_type base2 = 3 + 2 * (soffset - 2);
                return static_cast<size_type>(std::pow(base1, ndims) - std::pow(base2, ndims));
            };

            adjacent_indices_t res({compute_num_adj(static_cast<difference_type>(size(info_)), signed_offset)});
            size_type actual_num_adj = 0;

            std::function<void(adjacent_indices_t, size_type, bool)> impl;

            impl = [&](adjacent_indices_t subs, size_type perm_pos, bool used_offset) {
                if (perm_pos == size(info_)) {
                    return;
                }

                for (difference_type i = -signed_offset; i <= signed_offset; ++i) {
                    size_type abs_i = static_cast<size_type>(std::abs(i));

                    if (abs_i > 0 && abs_i <= signed_offset) {
                        auto new_subs = subs.clone();
                        new_subs[perm_pos] += i;
                        if (new_subs[perm_pos] >= 0 && new_subs[perm_pos] < info_.dims()[perm_pos]) {
                            if (used_offset || abs_i == signed_offset) {
                                res[actual_num_adj++] = sub2ind(info_, new_subs.cbegin(), new_subs.cend());
                            }
                            impl(new_subs, perm_pos + 1, abs_i == signed_offset);
                        }
                    } else {
                        impl(subs, perm_pos + 1, used_offset);
                    }
                }
            };

            std::initializer_list<size_type> res_dims{static_cast<size_type>(std::distance(first_sub, last_sub))};
            impl(adjacent_indices_t(res_dims.begin(), res_dims.end(), first_sub, last_sub), 0, false);

            if (total(res.info()) > actual_num_adj) {
                return res.resize({actual_num_adj});
            }
            return res;
        }

        template <iterable_of_type_integral Cont>
        [[nodiscard]] constexpr auto adjacent_indices(const Cont& subs, difference_type offset = 1) const
        {
            return adjacent_indices(std::begin(subs), std::end(subs), offset);
        }
        [[nodiscard]] constexpr auto adjacent_indices(
            std::initializer_list<size_type> subs, difference_type offset = 1) const
        {
            return adjacent_indices(subs.begin(), subs.end(), offset);
        }

        template <std::size_t AtDepth = this_type::depth, typename Pred>
        [[nodiscard]] constexpr bool all(Pred&& pred) const
        {
            if (empty()) {
                return true;
            }

            for (auto inner : *this) {
                if constexpr (AtDepth == 0) {
                    if (!pred(inner)) {
                        return false;
                    }
                } else {
                    if (!inner.template all<AtDepth - 1, Pred>(std::forward<Pred>(pred))) {
                        return false;
                    }
                }
            }

            return true;
        }

        template <std::size_t AtDepth = this_type::depth>
        [[nodiscard]] constexpr bool all() const
        {
            return all<AtDepth>([](const auto& value) {
                return static_cast<bool>(value);
            });
        }

        template <std::size_t AtDepth = this_type::depth, arrnd_type Arrnd, typename BinaryPred>
        [[nodiscard]] constexpr bool all_match(const Arrnd& arr, BinaryPred&& pred) const
        {
            if (empty() && arr.empty()) {
                return true;
            }

            if (empty() || arr.empty()) {
                return false;
            }

            if (!std::equal(std::begin(info_.dims()), std::end(info_.dims()), std::begin(arr.info().dims()),
                    std::end(arr.info().dims()))) {
                return false;
            }

            for (auto inners : zip(zipped(*this), zipped(arr))) {
                if constexpr (AtDepth == 0) {
                    if (!pred(std::get<0>(inners), std::get<1>(inners))) {
                        return false;
                    }
                } else {
                    if (!std::get<0>(inners).template all_match<AtDepth - 1, typename Arrnd::value_type, BinaryPred>(
                            std::get<1>(inners), std::forward<BinaryPred>(pred))) {
                        return false;
                    }
                }
            }

            return true;
        }

        template <std::size_t AtDepth = this_type::depth, arrnd_type Arrnd>
        [[nodiscard]] constexpr bool all_match(const Arrnd& arr) const
        {
            return all_match<AtDepth, Arrnd>(arr, [](const auto& a, const auto& b) {
                if constexpr (arrnd_type<decltype(a)> && arrnd_type<decltype(b)>) {
                    return (a == b).template all<this_type::depth>();
                } else {
                    return a == b;
                }
            });
        }

        template <std::size_t AtDepth = this_type::depth, typename U, typename BinaryPred>
            requires(!arrnd_type<U>)
        [[nodiscard]] constexpr bool all_match(const U& u, BinaryPred pred) const
        {
            return all<AtDepth>([&u, &pred](const auto& a) {
                return pred(a, u);
            });
        }

        template <std::size_t AtDepth = this_type::depth, typename U>
            requires(!arrnd_type<U>)
        [[nodiscard]] constexpr bool all_match(const U& u) const
        {
            return all<AtDepth>([&u](const auto& a) {
                if constexpr (arrnd_type<decltype(a)>) {
                    return (a == u).template all<this_type::depth>();
                } else {
                    return a == u;
                }
            });
        }

        template <std::size_t AtDepth = this_type::depth, typename Pred>
        [[nodiscard]] constexpr bool any(Pred&& pred) const
        {
            if (empty()) {
                return true;
            }

            for (auto inner : *this) {
                if constexpr (AtDepth == 0) {
                    if (pred(inner)) {
                        return true;
                    }
                } else {
                    if (inner.template any<AtDepth - 1, Pred>(std::forward<Pred>(pred))) {
                        return true;
                    }
                }
            }

            return false;
        }

        template <std::size_t AtDepth = this_type::depth>
        [[nodiscard]] constexpr bool any() const
        {
            return any<AtDepth>([](const auto& value) {
                return static_cast<bool>(value);
            });
        }

        template <std::size_t AtDepth = this_type::depth, arrnd_type Arrnd, typename BinaryPred>
        [[nodiscard]] constexpr bool any_match(const Arrnd& arr, BinaryPred&& pred) const
        {
            if (empty() && arr.empty()) {
                return true;
            }

            if (empty() || arr.empty()) {
                return false;
            }

            if (!std::equal(std::begin(info_.dims()), std::end(info_.dims()), std::begin(arr.info().dims()),
                    std::end(arr.info().dims()))) {
                return false;
            }

            for (auto inners : zip(zipped(*this), zipped(arr))) {
                if constexpr (AtDepth == 0) {
                    if (pred(std::get<0>(inners), std::get<1>(inners))) {
                        return true;
                    }
                } else {
                    if (std::get<0>(inners).template any_match<AtDepth - 1, typename Arrnd::value_type, BinaryPred>(
                            std::get<1>(inners), std::forward<BinaryPred>(pred))) {
                        return true;
                    }
                }
            }

            return false;
        }

        template <std::size_t AtDepth = this_type::depth, arrnd_type Arrnd>
        [[nodiscard]] constexpr bool any_match(const Arrnd& arr) const
        {
            return any_match<AtDepth, Arrnd>(arr, [](const auto& a, const auto& b) {
                if constexpr (arrnd_type<decltype(a)> && arrnd_type<decltype(b)>) {
                    return (a == b).template any<this_type::depth>();
                } else {
                    return a == b;
                }
            });
        }

        template <std::size_t AtDepth = this_type::depth, typename U, typename BinaryPred>
            requires(!arrnd_type<U>)
        [[nodiscard]] constexpr bool any_match(const U& u, BinaryPred pred) const
        {
            return any<AtDepth>([&u, &pred](const auto& a) {
                return pred(a, u);
            });
        }

        template <std::size_t AtDepth = this_type::depth, typename U>
            requires(!arrnd_type<U>)
        [[nodiscard]] constexpr bool any_match(const U& u) const
        {
            return any<AtDepth>([&u](const auto& a) {
                if constexpr (arrnd_type<decltype(a)>) {
                    return (a == u).template any<this_type::depth>();
                } else {
                    return a == u;
                }
            });
        }

        template <std::size_t AtDepth = this_type::depth, typename Pred>
        [[nodiscard]] constexpr replaced_type<bool> all(size_type axis, Pred pred) const
        {
            return reduce<AtDepth>(axis, [&pred](const auto& a, const auto& b) {
                return a && pred(b);
            });
        }

        template <std::size_t AtDepth = this_type::depth>
        [[nodiscard]] constexpr replaced_type<bool> all(size_type axis) const
        {
            return all(axis, [](const auto& a) {
                return a;
            });
        }

        template <std::size_t AtDepth = this_type::depth, typename Pred>
        [[nodiscard]] constexpr replaced_type<bool> any(size_type axis, Pred pred) const
        {
            return reduce<AtDepth>(axis, [&pred](const auto& a, const auto& b) {
                return a || pred(b);
            });
        }

        template <std::size_t AtDepth = this_type::depth>
        [[nodiscard]] constexpr replaced_type<bool> any(size_type axis) const
        {
            return any(axis, [](const auto& a) {
                return a;
            });
        }

        [[nodiscard]] constexpr auto begin()
        {
            return begin(0, arrnd_returned_element_iterator_tag{});
        }
        [[nodiscard]] constexpr auto begin() const
        {
            return begin(0, arrnd_returned_element_iterator_tag{});
        }
        [[nodiscard]] constexpr auto cbegin() const
        {
            return cbegin(0, arrnd_returned_element_iterator_tag{});
        }

        [[nodiscard]] constexpr auto end()
        {
            return end(0, arrnd_returned_element_iterator_tag{});
        }
        [[nodiscard]] constexpr auto end() const
        {
            return end(0, arrnd_returned_element_iterator_tag{});
        }
        [[nodiscard]] constexpr auto cend() const
        {
            return cend(0, arrnd_returned_element_iterator_tag{});
        }

        [[nodiscard]] constexpr auto rbegin()
        {
            return rbegin(0, arrnd_returned_element_iterator_tag{});
        }
        [[nodiscard]] constexpr auto rbegin() const
        {
            return rbegin(0, arrnd_returned_element_iterator_tag{});
        }
        [[nodiscard]] constexpr auto crbegin() const
        {
            return crbegin(0, arrnd_returned_element_iterator_tag{});
        }

        [[nodiscard]] constexpr auto rend()
        {
            return rend(0, arrnd_returned_element_iterator_tag{});
        }
        [[nodiscard]] constexpr auto rend() const
        {
            return rend(0, arrnd_returned_element_iterator_tag{});
        }
        [[nodiscard]] constexpr auto crend() const
        {
            return crend(0, arrnd_returned_element_iterator_tag{});
        }

        [[nodiscard]] constexpr auto begin(arrnd_returned_element_iterator_tag)
        {
            return begin(0, arrnd_returned_element_iterator_tag{});
        }
        [[nodiscard]] constexpr auto begin(arrnd_returned_element_iterator_tag) const
        {
            return begin(0, arrnd_returned_element_iterator_tag{});
        }
        [[nodiscard]] constexpr auto cbegin(arrnd_returned_element_iterator_tag) const
        {
            return cbegin(0, arrnd_returned_element_iterator_tag{});
        }
        [[nodiscard]] constexpr auto end(arrnd_returned_element_iterator_tag)
        {
            return end(0, arrnd_returned_element_iterator_tag{});
        }
        [[nodiscard]] constexpr auto end(arrnd_returned_element_iterator_tag) const
        {
            return end(0, arrnd_returned_element_iterator_tag{});
        }
        [[nodiscard]] constexpr auto cend(arrnd_returned_element_iterator_tag) const
        {
            return cend(0, arrnd_returned_element_iterator_tag{});
        }
        [[nodiscard]] constexpr auto rbegin(arrnd_returned_element_iterator_tag)
        {
            return rbegin(0, arrnd_returned_element_iterator_tag{});
        }
        [[nodiscard]] constexpr auto rbegin(arrnd_returned_element_iterator_tag) const
        {
            return rbegin(0, arrnd_returned_element_iterator_tag{});
        }
        [[nodiscard]] constexpr auto crbegin(arrnd_returned_element_iterator_tag) const
        {
            return crbegin(0, arrnd_returned_element_iterator_tag{});
        }
        [[nodiscard]] constexpr auto rend(arrnd_returned_element_iterator_tag)
        {
            return rend(0, arrnd_returned_element_iterator_tag{});
        }
        [[nodiscard]] constexpr auto rend(arrnd_returned_element_iterator_tag) const
        {
            return rend(0, arrnd_returned_element_iterator_tag{});
        }
        [[nodiscard]] constexpr auto crend(arrnd_returned_element_iterator_tag) const
        {
            return crend(0, arrnd_returned_element_iterator_tag{});
        }

        // by axis iterator functions

        [[nodiscard]] constexpr auto begin(size_type axis, arrnd_returned_element_iterator_tag)
        {
            return empty()
                ? iterator()
                : iterator(shared_storage_->data(), indexer_type(move(info_, axis, 0), arrnd_iterator_position::begin));
        }
        [[nodiscard]] constexpr auto begin(size_type axis, arrnd_returned_element_iterator_tag) const
        {
            return empty()
                ? iterator()
                : iterator(shared_storage_->data(), indexer_type(move(info_, axis, 0), arrnd_iterator_position::begin));
        }
        [[nodiscard]] constexpr auto cbegin(size_type axis, arrnd_returned_element_iterator_tag) const
        {
            return empty() ? const_iterator()
                           : const_iterator(shared_storage_->data(),
                               indexer_type(move(info_, axis, 0), arrnd_iterator_position::begin));
        }
        [[nodiscard]] constexpr auto end(size_type axis, arrnd_returned_element_iterator_tag)
        {
            return empty()
                ? iterator()
                : iterator(shared_storage_->data(), indexer_type(move(info_, axis, 0), arrnd_iterator_position::end));
        }
        [[nodiscard]] constexpr auto end(size_type axis, arrnd_returned_element_iterator_tag) const
        {
            return empty()
                ? iterator()
                : iterator(shared_storage_->data(), indexer_type(move(info_, axis, 0), arrnd_iterator_position::end));
        }
        [[nodiscard]] constexpr auto cend(size_type axis, arrnd_returned_element_iterator_tag) const
        {
            return empty() ? const_iterator()
                           : const_iterator(shared_storage_->data(),
                               indexer_type(move(info_, axis, 0), arrnd_iterator_position::end));
        }
        [[nodiscard]] constexpr auto rbegin(size_type axis, arrnd_returned_element_iterator_tag)
        {
            return empty() ? reverse_iterator()
                           : reverse_iterator(shared_storage_->data(),
                               indexer_type(move(info_, axis, 0), arrnd_iterator_position::rbegin));
        }
        [[nodiscard]] constexpr auto rbegin(size_type axis, arrnd_returned_element_iterator_tag) const
        {
            return empty() ? reverse_iterator()
                           : reverse_iterator(shared_storage_->data(),
                               indexer_type(move(info_, axis, 0), arrnd_iterator_position::rbegin));
        }
        [[nodiscard]] constexpr auto crbegin(size_type axis, arrnd_returned_element_iterator_tag) const
        {
            return empty() ? const_reverse_iterator()
                           : const_reverse_iterator(shared_storage_->data(),
                               indexer_type(move(info_, axis, 0), arrnd_iterator_position::rbegin));
        }
        [[nodiscard]] constexpr auto rend(size_type axis, arrnd_returned_element_iterator_tag)
        {
            return empty() ? reverse_iterator()
                           : reverse_iterator(shared_storage_->data(),
                               indexer_type(move(info_, axis, 0), arrnd_iterator_position::rend));
        }
        [[nodiscard]] constexpr auto rend(size_type axis, arrnd_returned_element_iterator_tag) const
        {
            return empty() ? reverse_iterator()
                           : reverse_iterator(shared_storage_->data(),
                               indexer_type(move(info_, axis, 0), arrnd_iterator_position::rend));
        }
        [[nodiscard]] constexpr auto crend(size_type axis, arrnd_returned_element_iterator_tag) const
        {
            return empty() ? const_reverse_iterator()
                           : const_reverse_iterator(shared_storage_->data(),
                               indexer_type(move(info_, axis, 0), arrnd_iterator_position::rend));
        }

        // by order iterator functions

        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto begin(InputIt first_order, InputIt last_order)
        {
            return empty()
                ? iterator()
                : iterator(shared_storage_->data(), indexer_type(oc::arrnd::transpose(info_, first_order, last_order)));
        }
        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto begin(InputIt first_order, InputIt last_order) const
        {
            return empty()
                ? iterator()
                : iterator(shared_storage_->data(), indexer_type(oc::arrnd::transpose(info_, first_order, last_order)));
        }
        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto cbegin(InputIt first_order, InputIt last_order) const
        {
            return empty() ? const_iterator()
                           : const_iterator(shared_storage_->data(),
                               indexer_type(oc::arrnd::transpose(info_, first_order, last_order)));
        }
        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto end(InputIt first_order, InputIt last_order)
        {
            return empty()
                ? iterator()
                : iterator(shared_storage_->data(),
                    indexer_type(oc::arrnd::transpose(info_, first_order, last_order), arrnd_iterator_position::end));
        }
        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto end(InputIt first_order, InputIt last_order) const
        {
            return empty()
                ? iterator()
                : iterator(shared_storage_->data(),
                    indexer_type(oc::arrnd::transpose(info_, first_order, last_order), arrnd_iterator_position::end));
        }
        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto cend(InputIt first_order, InputIt last_order) const
        {
            return empty()
                ? const_iterator()
                : const_iterator(shared_storage_->data(),
                    indexer_type(oc::arrnd::transpose(info_, first_order, last_order), arrnd_iterator_position::end));
        }
        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto rbegin(InputIt first_order, InputIt last_order)
        {
            return empty() ? reverse_iterator()
                           : reverse_iterator(shared_storage_->data(),
                               indexer_type(oc::arrnd::transpose(info_, first_order, last_order),
                                   arrnd_iterator_position::rbegin));
        }
        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto rbegin(InputIt first_order, InputIt last_order) const
        {
            return empty() ? reverse_iterator()
                           : reverse_iterator(shared_storage_->data(),
                               indexer_type(oc::arrnd::transpose(info_, first_order, last_order),
                                   arrnd_iterator_position::rbegin));
        }
        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto crbegin(InputIt first_order, InputIt last_order) const
        {
            return empty() ? const_reverse_iterator()
                           : const_reverse_iterator(shared_storage_->data(),
                               indexer_type(oc::arrnd::transpose(info_, first_order, last_order),
                                   arrnd_iterator_position::rbegin));
        }
        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto rend(InputIt first_order, InputIt last_order)
        {
            return empty()
                ? reverse_iterator()
                : reverse_iterator(shared_storage_->data(),
                    indexer_type(oc::arrnd::transpose(info_, first_order, last_order), arrnd_iterator_position::rend));
        }
        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto rend(InputIt first_order, InputIt last_order) const
        {
            return empty()
                ? reverse_iterator()
                : reverse_iterator(shared_storage_->data(),
                    indexer_type(oc::arrnd::transpose(info_, first_order, last_order), arrnd_iterator_position::rend));
        }
        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto crend(InputIt first_order, InputIt last_order) const
        {
            return empty()
                ? const_reverse_iterator()
                : const_reverse_iterator(shared_storage_->data(),
                    indexer_type(oc::arrnd::transpose(info_, first_order, last_order), arrnd_iterator_position::rend));
        }

        [[nodiscard]] constexpr auto begin(std::initializer_list<size_type> order)
        {
            return begin(order.begin(), order.end());
        }
        [[nodiscard]] constexpr auto begin(std::initializer_list<size_type> order) const
        {
            return begin(order.begin(), order.end());
        }
        [[nodiscard]] constexpr auto cbegin(std::initializer_list<size_type> order) const
        {
            return cbegin(order.begin(), order.end());
        }
        [[nodiscard]] constexpr auto end(std::initializer_list<size_type> order)
        {
            return end(order.begin(), order.end());
        }
        [[nodiscard]] constexpr auto end(std::initializer_list<size_type> order) const
        {
            return end(order.begin(), order.end());
        }
        [[nodiscard]] constexpr auto cend(std::initializer_list<size_type> order) const
        {
            return cend(order.begin(), order.end());
        }
        [[nodiscard]] constexpr auto rbegin(std::initializer_list<size_type> order)
        {
            return rbegin(order.begin(), order.end());
        }
        [[nodiscard]] constexpr auto rbegin(std::initializer_list<size_type> order) const
        {
            return rbegin(order.begin(), order.end());
        }
        [[nodiscard]] constexpr auto crbegin(std::initializer_list<size_type> order) const
        {
            return crbegin(order.begin(), order.end());
        }
        [[nodiscard]] constexpr auto rend(std::initializer_list<size_type> order)
        {
            return rend(order.begin(), order.end());
        }
        [[nodiscard]] constexpr auto rend(std::initializer_list<size_type> order) const
        {
            return rend(order.begin(), order.end());
        }
        [[nodiscard]] constexpr auto crend(std::initializer_list<size_type> order) const
        {
            return crend(order.begin(), order.end());
        }

        template <iterable_of_type_integral Cont>
        [[nodiscard]] constexpr auto begin(const Cont& order)
        {
            return begin(std::begin(order), std::end(order));
        }
        template <iterable_of_type_integral Cont>
        [[nodiscard]] constexpr auto begin(const Cont& order) const
        {
            return begin(std::begin(order), std::end(order));
        }
        template <iterable_of_type_integral Cont>
        [[nodiscard]] constexpr auto cbegin(const Cont& order) const
        {
            return cbegin(std::begin(order), std::end(order));
        }
        template <iterable_of_type_integral Cont>
        [[nodiscard]] constexpr auto end(const Cont& order)
        {
            return end(std::begin(order), std::end(order));
        }
        template <iterable_of_type_integral Cont>
        [[nodiscard]] constexpr auto end(const Cont& order) const
        {
            return end(std::begin(order), std::end(order));
        }
        template <iterable_of_type_integral Cont>
        [[nodiscard]] constexpr auto cend(const Cont& order) const
        {
            return cend(std::begin(order), std::end(order));
        }
        template <iterable_of_type_integral Cont>
        [[nodiscard]] constexpr auto rbegin(const Cont& order)
        {
            return rbegin(std::begin(order), std::end(order));
        }
        template <iterable_of_type_integral Cont>
        [[nodiscard]] constexpr auto rbegin(const Cont& order) const
        {
            return rbegin(std::begin(order), std::end(order));
        }
        template <iterable_of_type_integral Cont>
        [[nodiscard]] constexpr auto crbegin(const Cont& order) const
        {
            return crbegin(std::begin(order), std::end(order));
        }
        template <iterable_of_type_integral Cont>
        [[nodiscard]] constexpr auto rend(const Cont& order)
        {
            return rend(std::begin(order), std::end(order));
        }
        template <iterable_of_type_integral Cont>
        [[nodiscard]] constexpr auto rend(const Cont& order) const
        {
            return rend(std::begin(order), std::end(order));
        }
        template <iterable_of_type_integral Cont>
        [[nodiscard]] constexpr auto crend(const Cont& order) const
        {
            return crend(std::begin(order), std::end(order));
        }

        // slice iterator functions

        [[nodiscard]] constexpr auto begin(arrnd_returned_slice_iterator_tag)
        {
            return empty() ? slice_iterator()
                           : slice_iterator(*this,
                               windows_slider_type(info_, 0, window_type(typename window_type::interval_type{0, 1}),
                                   arrnd_iterator_position::begin));
        }
        [[nodiscard]] constexpr auto begin(arrnd_returned_slice_iterator_tag) const
        {
            return empty() ? slice_iterator()
                           : slice_iterator(*this,
                               windows_slider_type(info_, 0, window_type(typename window_type::interval_type{0, 1}),
                                   arrnd_iterator_position::begin));
        }
        [[nodiscard]] constexpr auto cbegin(arrnd_returned_slice_iterator_tag) const
        {
            return empty() ? const_slice_iterator()
                           : const_slice_iterator(*this,
                               windows_slider_type(info_, 0, window_type(typename window_type::interval_type{0, 1}),
                                   arrnd_iterator_position::begin));
        }
        [[nodiscard]] constexpr auto end(arrnd_returned_slice_iterator_tag)
        {
            return empty() ? slice_iterator()
                           : slice_iterator(*this,
                               windows_slider_type(info_, 0, window_type(typename window_type::interval_type{0, 1}),
                                   arrnd_iterator_position::end));
        }
        [[nodiscard]] constexpr auto end(arrnd_returned_slice_iterator_tag) const
        {
            return empty() ? slice_iterator()
                           : slice_iterator(*this,
                               windows_slider_type(info_, 0, window_type(typename window_type::interval_type{0, 1}),
                                   arrnd_iterator_position::end));
        }
        [[nodiscard]] constexpr auto cend(arrnd_returned_slice_iterator_tag) const
        {
            return empty() ? const_slice_iterator()
                           : const_slice_iterator(*this,
                               windows_slider_type(info_, 0, window_type(typename window_type::interval_type{0, 1}),
                                   arrnd_iterator_position::end));
        }
        [[nodiscard]] constexpr auto rbegin(arrnd_returned_slice_iterator_tag)
        {
            return empty() ? reverse_slice_iterator()
                           : reverse_slice_iterator(*this,
                               windows_slider_type(info_, 0,
                                   window_type(typename window_type::interval_type{0, 1}, arrnd_window_type::partial),
                                   arrnd_iterator_position::rbegin));
        }
        [[nodiscard]] constexpr auto rbegin(arrnd_returned_slice_iterator_tag) const
        {
            return empty() ? reverse_slice_iterator()
                           : reverse_slice_iterator(*this,
                               windows_slider_type(info_, 0,
                                   window_type(typename window_type::interval_type{0, 1}, arrnd_window_type::partial),
                                   arrnd_iterator_position::rbegin));
        }
        [[nodiscard]] constexpr auto crbegin(arrnd_returned_slice_iterator_tag) const
        {
            return empty() ? const_reverse_slice_iterator()
                           : const_reverse_slice_iterator(*this,
                               windows_slider_type(info_, 0,
                                   window_type(typename window_type::interval_type{0, 1}, arrnd_window_type::partial),
                                   arrnd_iterator_position::rbegin));
        }
        [[nodiscard]] constexpr auto rend(arrnd_returned_slice_iterator_tag)
        {
            return empty() ? reverse_slice_iterator()
                           : reverse_slice_iterator(*this,
                               windows_slider_type(info_, 0,
                                   window_type(typename window_type::interval_type{0, 1}, arrnd_window_type::partial),
                                   arrnd_iterator_position::rend));
        }
        [[nodiscard]] constexpr auto rend(arrnd_returned_slice_iterator_tag) const
        {
            return empty() ? reverse_slice_iterator()
                           : reverse_slice_iterator(*this,
                               windows_slider_type(info_, 0,
                                   window_type(typename window_type::interval_type{0, 1}, arrnd_window_type::partial),
                                   arrnd_iterator_position::rend));
        }
        [[nodiscard]] constexpr auto crend(arrnd_returned_slice_iterator_tag) const
        {
            return empty() ? const_reverse_slice_iterator()
                           : const_reverse_slice_iterator(*this,
                               windows_slider_type(info_, 0,
                                   window_type(typename window_type::interval_type{0, 1}, arrnd_window_type::partial),
                                   arrnd_iterator_position::rend));
        }

        [[nodiscard]] constexpr auto begin(size_type axis, arrnd_returned_slice_iterator_tag)
        {
            return empty() ? slice_iterator()
                           : slice_iterator(*this,
                               windows_slider_type(info_, axis,
                                   window_type(typename window_type::interval_type{0, 1}, arrnd_window_type::partial),
                                   arrnd_iterator_position::begin));
        }
        [[nodiscard]] constexpr auto begin(size_type axis, arrnd_returned_slice_iterator_tag) const
        {
            return empty() ? slice_iterator()
                           : slice_iterator(*this,
                               windows_slider_type(info_, axis,
                                   window_type(typename window_type::interval_type{0, 1}, arrnd_window_type::partial),
                                   arrnd_iterator_position::begin));
        }
        [[nodiscard]] constexpr auto cbegin(size_type axis, arrnd_returned_slice_iterator_tag) const
        {
            return empty() ? const_slice_iterator()
                           : const_slice_iterator(*this,
                               windows_slider_type(info_, axis,
                                   window_type(typename window_type::interval_type{0, 1}, arrnd_window_type::partial),
                                   arrnd_iterator_position::begin));
        }
        [[nodiscard]] constexpr auto end(size_type axis, arrnd_returned_slice_iterator_tag)
        {
            return empty() ? slice_iterator()
                           : slice_iterator(*this,
                               windows_slider_type(info_, axis,
                                   window_type(typename window_type::interval_type{0, 1}, arrnd_window_type::partial),
                                   arrnd_iterator_position::end));
        }
        [[nodiscard]] constexpr auto end(size_type axis, arrnd_returned_slice_iterator_tag) const
        {
            return empty() ? slice_iterator()
                           : slice_iterator(*this,
                               windows_slider_type(info_, axis,
                                   window_type(typename window_type::interval_type{0, 1}, arrnd_window_type::partial),
                                   arrnd_iterator_position::end));
        }
        [[nodiscard]] constexpr auto cend(size_type axis, arrnd_returned_slice_iterator_tag) const
        {
            return empty() ? const_slice_iterator()
                           : const_slice_iterator(*this,
                               windows_slider_type(info_, axis,
                                   window_type(typename window_type::interval_type{0, 1}, arrnd_window_type::partial),
                                   arrnd_iterator_position::end));
        }
        [[nodiscard]] constexpr auto rbegin(size_type axis, arrnd_returned_slice_iterator_tag)
        {
            return empty() ? reverse_slice_iterator()
                           : reverse_slice_iterator(*this,
                               windows_slider_type(info_, axis,
                                   window_type(typename window_type::interval_type{0, 1}, arrnd_window_type::partial),
                                   arrnd_iterator_position::rbegin));
        }
        [[nodiscard]] constexpr auto rbegin(size_type axis, arrnd_returned_slice_iterator_tag) const
        {
            return empty() ? reverse_slice_iterator()
                           : reverse_slice_iterator(*this,
                               windows_slider_type(info_, axis,
                                   window_type(typename window_type::interval_type{0, 1}, arrnd_window_type::partial),
                                   arrnd_iterator_position::rbegin));
        }
        [[nodiscard]] constexpr auto crbegin(size_type axis, arrnd_returned_slice_iterator_tag) const
        {
            return empty() ? const_reverse_slice_iterator()
                           : const_reverse_slice_iterator(*this,
                               windows_slider_type(info_, axis,
                                   window_type(typename window_type::interval_type{0, 1}, arrnd_window_type::partial),
                                   arrnd_iterator_position::rbegin));
        }
        [[nodiscard]] constexpr auto rend(size_type axis, arrnd_returned_slice_iterator_tag)
        {
            return empty() ? reverse_slice_iterator()
                           : reverse_slice_iterator(*this,
                               windows_slider_type(info_, axis,
                                   window_type(typename window_type::interval_type{0, 1}, arrnd_window_type::partial),
                                   arrnd_iterator_position::rend));
        }
        [[nodiscard]] constexpr auto rend(size_type axis, arrnd_returned_slice_iterator_tag) const
        {
            return empty() ? reverse_slice_iterator()
                           : reverse_slice_iterator(*this,
                               windows_slider_type(info_, axis,
                                   window_type(typename window_type::interval_type{0, 1}, arrnd_window_type::partial),
                                   arrnd_iterator_position::rend));
        }
        [[nodiscard]] constexpr auto crend(size_type axis, arrnd_returned_slice_iterator_tag) const
        {
            return empty() ? const_reverse_slice_iterator()
                           : const_reverse_slice_iterator(*this,
                               windows_slider_type(info_, axis,
                                   window_type(typename window_type::interval_type{0, 1}, arrnd_window_type::partial),
                                   arrnd_iterator_position::rend));
        }

    private:
        struct creators_chain {
            std::shared_ptr<bool> has_original_creator = std::allocate_shared<bool>(allocator_template_type<bool>());
            std::weak_ptr<bool> is_creator_valid{};
            const this_type* latest_creator = nullptr;
        };

        info_type info_{};
        std::shared_ptr<storage_type> shared_storage_{nullptr};

        creators_chain creators_{};
    };

    // arrnd type deduction by constructors

    template <iterator_of_type_integral InputDimsIt, iterator_type InputDataIt,
        typename DataStorageTraits = simple_vector_traits<iterator_value_t<InputDataIt>>,
        typename ArrndInfo = arrnd_info<>>
    arrnd(const InputDimsIt&, const InputDimsIt&, const InputDataIt&, const InputDataIt&)
        -> arrnd<iterator_value_t<InputDataIt>, DataStorageTraits, ArrndInfo>;

    template <iterator_of_type_integral InputDimsIt, typename U, typename DataStorageTraits = simple_vector_traits<U>,
        typename ArrndInfo = arrnd_info<>>
    arrnd(const InputDimsIt&, const InputDimsIt&, const U&) -> arrnd<U, DataStorageTraits, ArrndInfo>;
    template <iterable_of_type_integral Cont, typename U, typename DataStorageTraits = simple_vector_traits<U>,
        typename ArrndInfo = arrnd_info<>>
    arrnd(const Cont&, const U&) -> arrnd<U, DataStorageTraits, ArrndInfo>;
    template <typename U, typename DataStorageTraits = simple_vector_traits<U>, typename ArrndInfo = arrnd_info<>>
    arrnd(std::initializer_list<typename DataStorageTraits::storage_type::size_type>, const U&)
        -> arrnd<U, DataStorageTraits, ArrndInfo>;

    template <typename U, typename DataStorageTraits = simple_vector_traits<U>, typename ArrndInfo = arrnd_info<>>
    arrnd(std::initializer_list<typename DataStorageTraits::storage_type::size_type>, std::initializer_list<U>)
        -> arrnd<U, DataStorageTraits, ArrndInfo>;

    template <iterable_of_type_integral Cont, iterable_type DataCont,
        typename DataStorageTraits = simple_vector_traits<iterable_value_t<DataCont>>,
        typename ArrndInfo = arrnd_info<>>
    arrnd(const Cont&, const DataCont&) -> arrnd<iterable_value_t<DataCont>, DataStorageTraits, ArrndInfo>;

    template <typename Func, typename DataStorageTraits = simple_vector_traits<std::invoke_result_t<Func>>,
        typename ArrndInfo = arrnd_info<>, iterator_of_type_integral InputDimsIt>
        requires(!arrnd_type<Func> && std::is_invocable_v<Func>)
    arrnd(const InputDimsIt&, const InputDimsIt&, Func&&)
        -> arrnd<std::invoke_result_t<Func>, DataStorageTraits, ArrndInfo>;
    template <typename Func, typename DataStorageTraits = simple_vector_traits<std::invoke_result_t<Func>>,
        typename ArrndInfo = arrnd_info<>, iterable_of_type_integral Cont>
        requires(!arrnd_type<Func> && std::is_invocable_v<Func>)
    arrnd(const Cont&, Func&&) -> arrnd<std::invoke_result_t<Func>, DataStorageTraits, ArrndInfo>;
    template <typename Func, typename DataStorageTraits = simple_vector_traits<std::invoke_result_t<Func>>,
        typename ArrndInfo = arrnd_info<>>
        requires(!arrnd_type<Func> && std::is_invocable_v<Func>)
    arrnd(std::initializer_list<typename DataStorageTraits::storage_type::size_type>, Func&&)
        -> arrnd<std::invoke_result_t<Func>, DataStorageTraits, ArrndInfo>;

    template <typename T, typename DataStorageTraits = simple_vector_traits<typename T::value_type>,
        typename ArrndInfo = arrnd_info<>>
    arrnd(const ArrndInfo&, std::shared_ptr<T>) -> arrnd<typename T::value_type, DataStorageTraits, ArrndInfo>;

    // free arrnd iterator functions

    template <arrnd_type Arrnd, typename... Args>
    [[nodiscard]] inline constexpr auto begin(Arrnd& c, Args&&... args)
    {
        return c.begin(std::forward<Args>(args)...);
    }
    template <arrnd_type Arrnd, typename... Args>
    [[nodiscard]] inline constexpr auto begin(const Arrnd& c, Args&&... args)
    {
        return c.begin(std::forward<Args>(args)...);
    }
    template <arrnd_type Arrnd, typename... Args>
    [[nodiscard]] inline constexpr auto cbegin(const Arrnd& c, Args&&... args)
    {
        return c.cbegin(std::forward<Args>(args)...);
    }

    template <arrnd_type Arrnd, typename... Args>
    [[nodiscard]] inline constexpr auto end(Arrnd& c, Args&&... args)
    {
        return c.end(std::forward<Args>(args)...);
    }
    template <arrnd_type Arrnd, typename... Args>
    [[nodiscard]] inline constexpr auto end(const Arrnd& c, Args&&... args)
    {
        return c.end(std::forward<Args>(args)...);
    }
    template <arrnd_type Arrnd, typename... Args>
    [[nodiscard]] inline constexpr auto cend(const Arrnd& c, Args&&... args)
    {
        return c.cend(std::forward<Args>(args)...);
    }

    template <arrnd_type Arrnd, typename... Args>
    [[nodiscard]] inline constexpr auto rbegin(Arrnd& c, Args&&... args)
    {
        return c.rbegin(std::forward<Args>(args)...);
    }
    template <arrnd_type Arrnd, typename... Args>
    [[nodiscard]] inline constexpr auto rbegin(const Arrnd& c, Args&&... args)
    {
        return c.rbegin(std::forward<Args>(args)...);
    }
    template <arrnd_type Arrnd, typename... Args>
    [[nodiscard]] inline constexpr auto crbegin(const Arrnd& c, Args&&... args)
    {
        return c.crbegin(std::forward<Args>(args)...);
    }

    template <arrnd_type Arrnd, typename... Args>
    [[nodiscard]] inline constexpr auto rend(Arrnd& c, Args&&... args)
    {
        return c.rend(std::forward<Args>(args)...);
    }
    template <arrnd_type Arrnd, typename... Args>
    [[nodiscard]] inline constexpr auto rend(const Arrnd& c, Args&&... args)
    {
        return c.rend(std::forward<Args>(args)...);
    }
    template <arrnd_type Arrnd, typename... Args>
    [[nodiscard]] inline constexpr auto crend(const Arrnd& c, Args&&... args)
    {
        return c.crend(std::forward<Args>(args)...);
    }

    template <arrnd_type First, arrnd_type Second>
    [[nodiscard]] inline constexpr First concat(const First& first, const Second& second)
    {
        return first.clone().push_back(second);
    }

    template <arrnd_type First, arrnd_type Second, typename Third, typename... Others>
        requires(arrnd_type<Third> || std::signed_integral<Third>)
    [[nodiscard]] inline constexpr First
        concat(const First& first, const Second& second, const Third& third, Others&&... others)
    {
        if constexpr (sizeof...(Others) == 0) {
            if constexpr (std::signed_integral<Third>) {
                return first.clone().push_back(second, third);
            } else {
                return first.clone().push_back(second).push_back(third);
            }
        } else {
            if constexpr (std::signed_integral<Third>) {
                return concat(first.clone().push_back(second, third), std::forward<Others>(others)...);
            } else {
                return concat(first.clone().push_back(second), third, std::forward<Others>(others)...);
            }
        }
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr auto dot(const Arrnd1& lhs, const Arrnd2& rhs)
    {
        using dot_t = typename Arrnd1::template replaced_type<decltype((typename Arrnd1::value_type{})
            * (typename Arrnd2::value_type{}))>;

        if (size(lhs.info()) < 2 || size(rhs.info()) < 2) {
            throw std::invalid_argument("invalid inputs - should be at least matrices");
        }

        auto dot_impl = [](const auto& lmat, const auto& rmat) {
            if (!ismatrix(lmat.info()) || !ismatrix(rmat.info())) {
                throw std::invalid_argument("invalid inputs - not matrices");
            }

            if (lmat.info().dims().back() != rmat.info().dims().front()) {
                throw std::invalid_argument("invalid inputs - matrices size not suitable for multiplication");
            }

            dot_t res({lmat.info().dims().front(), rmat.info().dims().back()});

            typename Arrnd1::size_type element_index = 0;

            auto trmat = transpose(rmat, {1, 0});

            std::for_each(lmat.cbegin(arrnd_returned_slice_iterator_tag{}),
                lmat.cend(arrnd_returned_slice_iterator_tag{}), [&res, &trmat, &element_index](const auto& row) {
                    std::for_each(trmat.cbegin(arrnd_returned_slice_iterator_tag{}),
                        trmat.cend(arrnd_returned_slice_iterator_tag{}), [&res, &element_index, &row](const auto& col) {
                            res[element_index++] = (row * col).template reduce<0>(std::plus<>{});
                        });
                });

            return res;
        };

        if (ismatrix(lhs.info()) && ismatrix(rhs.info())) {
            return dot_impl(lhs, rhs);
        }

        if (ismatrix(rhs.info())) {
            return lhs.browse(2, [&rhs, dot_impl](const auto& mat) {
                return dot_impl(mat, rhs);
            });
        } else {
            auto lhs_num_pages
                = total(lhs.info()) / (lhs.info().dims()[size(lhs.info()) - 2] * lhs.info().dims().back());
            auto rhs_num_pages
                = total(rhs.info()) / (rhs.info().dims()[size(rhs.info()) - 2] * rhs.info().dims().back());

            if (lhs_num_pages != rhs_num_pages) {
                std::invalid_argument("invalid inputs - arrays does not have the same number of pages");
            }

            auto rhs_pages = rhs.pages(2);
            auto rhs_pages_it = rhs_pages.begin();

            return lhs.browse(2, [&rhs_pages_it, &dot_impl](const auto& mat) {
                return dot_impl(mat, *(rhs_pages_it++));
            });
        }
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2, arrnd_type... Arrnds>
    [[nodiscard]] inline constexpr auto dot(const Arrnd1& arr1, const Arrnd2& arr2, Arrnds&&... others)
    {
        if constexpr (sizeof...(others) == 0) {
            return dot(arr1, arr2);
        } else {
            return dot(dot(arr1, arr2), std::forward<Arrnds>(others)...);
        }
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto det(const Arrnd& arr)
    {
        if (size(arr.info()) < 2) {
            throw std::invalid_argument("invalid input - should be at least matrix");
        }

        std::function<typename Arrnd::value_type(Arrnd)> det_impl;

        det_impl = [&](const auto& arr) {
            if (!ismatrix(arr.info())) {
                throw std::invalid_argument("invalid input - not matrix");
            }

            if (arr.info().dims().front() != arr.info().dims().back()) {
                throw std::invalid_argument("invalid input - not squared");
            }

            auto n = arr.info().dims().front();

            if (n == 1) {
                return arr(0);
            }

            if (n == 2) {
                return arr(0) * arr(3) - arr(1) * arr(2);
            }

            typename Arrnd::value_type sign{1};
            typename Arrnd::value_type d{0};

            for (typename Arrnd::size_type j = 0; j < n; ++j) {
                typename Arrnd::value_type p{arr[{0, j}]};
                if (p != typename Arrnd::value_type{0}) {
                    d += sign * p
                        * det_impl(arr.exclude({0}, {0})
                                       .template transform<0>([j](const auto& val) {
                                           return val.exclude({1}, {j});
                                       })
                                       .merge()
                                       .merge());
                }
                sign *= typename Arrnd::value_type{-1};
            }
            return d;
        };

        if (ismatrix(arr.info())) {
            return Arrnd({1}, det_impl(arr));
        }

        return arr.browse(2, [det_impl](auto page) {
            return det_impl(page);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto inv(const Arrnd& arr)
    {
        if (size(arr.info()) < 2) {
            throw std::invalid_argument("invalid input - should be at least matrix");
        }

        std::function<Arrnd(Arrnd)> inv_impl;

        inv_impl = [&](const auto& arr) {
            if (!ismatrix(arr.info())) {
                throw std::invalid_argument("invalid input - not matrix");
            }

            if (arr.info().dims().front() != arr.info().dims().back()) {
                throw std::invalid_argument("invalid input - not squared");
            }

            typename Arrnd::value_type d = det(arr)(0);
            if (d == typename Arrnd::value_type{0}) {
                throw std::invalid_argument("invalid input - zero determinant");
            }

            auto n = arr.info().dims().front();

            typename Arrnd::this_type res(arr.info().dims());

            for (typename Arrnd::size_type i = 0; i < n; ++i) {
                typename Arrnd::value_type sign
                    = (i + 1) % 2 == 0 ? typename Arrnd::value_type{-1} : typename Arrnd::value_type{1};
                for (typename Arrnd::size_type j = 0; j < n; ++j) {

                    res[{i, j}] = sign
                        * det(arr.exclude({0}, {i})
                                  .template transform<0>([j](const auto& val) {
                                      return val.exclude({1}, {j});
                                  })
                                  .merge()
                                  .merge())(0);
                    sign *= typename Arrnd::value_type{-1};
                }
            }

            return (typename Arrnd::value_type{1} / d) * transpose(res, {1, 0});
        };

        if (ismatrix(arr.info())) {
            return inv_impl(arr);
        }

        return arr.browse(2, [inv_impl](auto page) {
            return inv_impl(page);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto all(const Arrnd& arr, typename Arrnd::size_type axis)
    {
        return arr.template all<Arrnd::depth>(axis);
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto any(const Arrnd& arr, typename Arrnd::size_type axis)
    {
        return arr.template any<Arrnd::depth>(axis);
    }

    template <std::size_t AtDepth, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto sum(const Arrnd& arr)
    {
        return arr.template reduce<AtDepth>([](const auto& a, const auto& b) {
            return a + b;
        });
    }

    template <std::size_t AtDepth, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto sum(const Arrnd& arr, typename Arrnd::size_type axis)
    {
        return arr.template reduce<AtDepth>(axis, [](const auto& a, const auto& b) {
            return a + b;
        });
    }

    template <std::size_t AtDepth, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto min(const Arrnd& arr)
    {
        return arr.template reduce<AtDepth>([](const auto& a, const auto& b) {
            using std::min;
            return min(a, b);
        });
    }

    template <std::size_t AtDepth, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto min(const Arrnd& arr, typename Arrnd::size_type axis)
    {
        return arr.template reduce<AtDepth>(axis, [](const auto& a, const auto& b) {
            using std::min;
            return min(a, b);
        });
    }

    template <std::size_t AtDepth, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto max(const Arrnd& arr)
    {
        return arr.template reduce<AtDepth>([](const auto& a, const auto& b) {
            using std::max;
            return max(a, b);
        });
    }

    template <std::size_t AtDepth, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto max(const Arrnd& arr, typename Arrnd::size_type axis)
    {
        return arr.template reduce<AtDepth>(axis, [](const auto& a, const auto& b) {
            using std::max;
            return max(a, b);
        });
    }

    template <std::size_t AtDepth, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto prod(const Arrnd& arr)
    {
        return arr.template reduce<AtDepth>([](const auto& a, const auto& b) {
            return a * b;
        });
    }

    template <std::size_t AtDepth, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto prod(const Arrnd& arr, typename Arrnd::size_type axis)
    {
        return arr.template reduce<AtDepth>(axis, [](const auto& a, const auto& b) {
            return a * b;
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto sum(const Arrnd& arr)
    {
        return sum<Arrnd::depth, Arrnd>(arr);
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto sum(const Arrnd& arr, typename Arrnd::size_type axis)
    {
        return sum<Arrnd::depth, Arrnd>(arr, axis);
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto min(const Arrnd& arr)
    {
        return min<Arrnd::depth, Arrnd>(arr);
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto min(const Arrnd& arr, typename Arrnd::size_type axis)
    {
        return min<Arrnd::depth, Arrnd>(arr, axis);
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto max(const Arrnd& arr)
    {
        return max<Arrnd::depth, Arrnd>(arr);
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto max(const Arrnd& arr, typename Arrnd::size_type axis)
    {
        return max<Arrnd::depth, Arrnd>(arr, axis);
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto prod(const Arrnd& arr)
    {
        return prod<Arrnd::depth, Arrnd>(arr);
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto prod(const Arrnd& arr, typename Arrnd::size_type axis)
    {
        return prod<Arrnd::depth, Arrnd>(arr, axis);
    }

    template <arrnd_type Arrnd, iterator_of_type_integral InputIt>
    [[nodiscard]] inline constexpr auto transpose(const Arrnd& arr, InputIt first_axis, InputIt last_axis)
    {
        if (arr.empty() || size(arr.info()) == 1) {
            return arr;
        }

        Arrnd res({total(arr.info())});
        res.info() = simplify(transpose(arr.info(), first_axis, last_axis));

        for (auto t : zip(zipped(arr.begin(first_axis, last_axis), arr.end(first_axis, last_axis)), zipped(res))) {
            std::get<1>(t) = std::get<0>(t);
        }

        return res;
    }

    template <arrnd_type Arrnd, iterable_of_type_integral Cont>
    [[nodiscard]] inline constexpr auto transpose(const Arrnd& arr, const Cont& axes)
    {
        return transpose(arr, std::begin(axes), std::end(axes));
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto transpose(
        const Arrnd& arr, std::initializer_list<typename Arrnd::size_type> axes)
    {
        return transpose(arr, axes.begin(), axes.end());
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto transpose(const Arrnd& arr)
    {
        if (arr.empty() || size(arr.info()) == 1) {
            return arr;
        }

        typename Arrnd::info_type::extent_storage_type axes(size(arr.info()));
        std::iota(std::begin(axes), std::end(axes), 0);

        std::swap(axes[std::size(axes) - 1], axes[std::size(axes) - 2]);

        return transpose(arr, axes);
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr auto operator==(const Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.transform(rhs, [](const auto& a, const auto& b) {
            return a == b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    [[nodiscard]] inline constexpr auto operator==(const Arrnd& lhs, const T& rhs)
    {
        return lhs.transform([&rhs](const auto& a) {
            return a == rhs;
        });
    }

    template <typename T, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto operator==(const T& lhs, const Arrnd& rhs)
    {
        return rhs.transform([&lhs](const auto& b) {
            return lhs == b;
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr auto operator!=(const Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.transform(rhs, [](const auto& a, const auto& b) {
            return a != b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    [[nodiscard]] inline constexpr auto operator!=(const Arrnd& lhs, const T& rhs)
    {
        return lhs.transform([&rhs](const auto& a) {
            return a != rhs;
        });
    }

    template <typename T, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto operator!=(const T& lhs, const Arrnd& rhs)
    {
        return rhs.transform([&lhs](const auto& b) {
            return lhs != b;
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr auto close(const Arrnd1& lhs, const Arrnd2& rhs,
        const arrnd_tol_type<Arrnd1, Arrnd2>& atol = default_atol<arrnd_tol_type<Arrnd1, Arrnd2>>(),
        const arrnd_tol_type<Arrnd1, Arrnd2>& rtol = default_rtol<arrnd_tol_type<Arrnd1, Arrnd2>>())
    {
        return lhs.template transform<Arrnd1::depth>(rhs, [&atol, &rtol](const auto& a, const auto& b) {
            return details::close(a, b, atol, rtol);
        });
    }

    template <arrnd_type Arrnd, typename U>
        requires(!arrnd_type<U>)
    [[nodiscard]] inline constexpr auto close(const Arrnd& arr, const U& value,
        const arrnd_lhs_tol_type<Arrnd, U>& atol = default_atol<arrnd_lhs_tol_type<Arrnd, U>>(),
        const arrnd_lhs_tol_type<Arrnd, U>& rtol = default_rtol<arrnd_lhs_tol_type<Arrnd, U>>())
    {
        return arr.template transform<Arrnd::depth>([&atol, &rtol, &value](const auto& a) {
            return details::close(a, value, atol, rtol);
        });
    }

    template <typename U, arrnd_type Arrnd>
        requires(!arrnd_type<U>)
    [[nodiscard]] inline constexpr auto close(const U& value, const Arrnd& arr,
        const arrnd_rhs_tol_type<U, Arrnd>& atol = default_atol<arrnd_rhs_tol_type<U, Arrnd>>(),
        const arrnd_rhs_tol_type<U, Arrnd>& rtol = default_rtol<arrnd_rhs_tol_type<U, Arrnd>>())
    {
        return close<Arrnd, U>(arr, value, atol, rtol);
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr auto operator>(const Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.transform(rhs, [](const auto& a, const auto& b) {
            return a > b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    [[nodiscard]] inline constexpr auto operator>(const Arrnd& lhs, const T& rhs)
    {
        return lhs.transform([&rhs](const auto& a) {
            return a > rhs;
        });
    }

    template <typename T, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto operator>(const T& lhs, const Arrnd& rhs)
    {
        return rhs.transform([&lhs](const auto& b) {
            return lhs > b;
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr auto operator>=(const Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.transform(rhs, [](const auto& a, const auto& b) {
            return a >= b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    [[nodiscard]] inline constexpr auto operator>=(const Arrnd& lhs, const T& rhs)
    {
        return lhs.transform([&rhs](const auto& a) {
            return a >= rhs;
        });
    }

    template <typename T, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto operator>=(const T& lhs, const Arrnd& rhs)
    {
        return rhs.transform([&lhs](const auto& b) {
            return lhs >= b;
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr auto operator<(const Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.transform(rhs, [](const auto& a, const auto& b) {
            return a < b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    [[nodiscard]] inline constexpr auto operator<(const Arrnd& lhs, const T& rhs)
    {
        return lhs.transform([&rhs](const auto& a) {
            return a < rhs;
        });
    }

    template <typename T, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto operator<(const T& lhs, const Arrnd& rhs)
    {
        return rhs.transform([&lhs](const auto& b) {
            return lhs < b;
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr auto operator<=(const Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.transform(rhs, [](const auto& a, const auto& b) {
            return a <= b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    [[nodiscard]] inline constexpr auto operator<=(const Arrnd& lhs, const T& rhs)
    {
        return lhs.transform([&rhs](const auto& a) {
            return a <= rhs;
        });
    }

    template <typename T, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto operator<=(const T& lhs, const Arrnd& rhs)
    {
        return rhs.transform([&lhs](const auto& b) {
            return lhs <= b;
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr auto operator+(const Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.transform(rhs, [](const auto& a, const auto& b) {
            return a + b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    [[nodiscard]] inline constexpr auto operator+(const Arrnd& lhs, const T& rhs)
    {
        return lhs.transform([&rhs](const auto& a) {
            return a + rhs;
        });
    }

    template <typename T, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto operator+(const T& lhs, const Arrnd& rhs)
    {
        return rhs.transform([&lhs](const auto& b) {
            return lhs + b;
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    inline constexpr auto& operator+=(Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.apply(rhs, [](const auto& a, const auto& b) {
            return a + b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    inline constexpr auto& operator+=(Arrnd& lhs, const T& rhs)
    {
        return lhs.apply([&rhs](const auto& a) {
            return a + rhs;
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr auto operator-(const Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.transform(rhs, [](const auto& a, const auto& b) {
            return a - b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    [[nodiscard]] inline constexpr auto operator-(const Arrnd& lhs, const T& rhs)
    {
        return lhs.transform([&rhs](const auto& a) {
            return a - rhs;
        });
    }

    template <typename T, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto operator-(const T& lhs, const Arrnd& rhs)
    {
        return rhs.transform([&lhs](const auto& b) {
            return lhs - b;
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    inline constexpr auto& operator-=(Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.apply(rhs, [](const auto& a, const auto& b) {
            return a - b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    inline constexpr auto& operator-=(Arrnd& lhs, const T& rhs)
    {
        return lhs.apply([&rhs](const auto& a) {
            return a - rhs;
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr auto operator*(const Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.transform(rhs, [](const auto& a, const auto& b) {
            return a * b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    [[nodiscard]] inline constexpr auto operator*(const Arrnd& lhs, const T& rhs)
    {
        return lhs.transform([&rhs](const auto& a) {
            return a * rhs;
        });
    }

    template <typename T, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto operator*(const T& lhs, const Arrnd& rhs)
    {
        return rhs.transform([&lhs](const auto& b) {
            return lhs * b;
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    inline constexpr auto& operator*=(Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.apply(rhs, [](const auto& a, const auto& b) {
            return a * b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    inline constexpr auto& operator*=(Arrnd& lhs, const T& rhs)
    {
        return lhs.apply([&rhs](const auto& a) {
            return a * rhs;
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr auto operator/(const Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.transform(rhs, [](const auto& a, const auto& b) {
            return a / b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    [[nodiscard]] inline constexpr auto operator/(const Arrnd& lhs, const T& rhs)
    {
        return lhs.transform([&rhs](const auto& a) {
            return a / rhs;
        });
    }

    template <typename T, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto operator/(const T& lhs, const Arrnd& rhs)
    {
        return rhs.transform([&lhs](const auto& b) {
            return lhs / b;
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    inline constexpr auto& operator/=(Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.apply(rhs, [](const auto& a, const auto& b) {
            return a / b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    inline constexpr auto& operator/=(Arrnd& lhs, const T& rhs)
    {
        return lhs.apply([&rhs](const auto& a) {
            return a / rhs;
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr auto operator%(const Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.transform(rhs, [](const auto& a, const auto& b) {
            return a % b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    [[nodiscard]] inline constexpr auto operator%(const Arrnd& lhs, const T& rhs)
    {
        return lhs.transform([&rhs](const auto& a) {
            return a % rhs;
        });
    }

    template <typename T, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto operator%(const T& lhs, const Arrnd& rhs)
    {
        return rhs.transform([&lhs](const auto& b) {
            return lhs % b;
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    inline constexpr auto& operator%=(Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.apply(rhs, [](const auto& a, const auto& b) {
            return a % b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    inline constexpr auto& operator%=(Arrnd& lhs, const T& rhs)
    {
        return lhs.apply([&rhs](const auto& a) {
            return a % rhs;
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr auto operator^(const Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.transform(rhs, [](const auto& a, const auto& b) {
            return a ^ b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    [[nodiscard]] inline constexpr auto operator^(const Arrnd& lhs, const T& rhs)
    {
        return lhs.transform([&rhs](const auto& a) {
            return a ^ rhs;
        });
    }

    template <typename T, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto operator^(const T& lhs, const Arrnd& rhs)
    {
        return rhs.transform([&lhs](const auto& b) {
            return lhs ^ b;
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    inline constexpr auto& operator^=(Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.apply(rhs, [](const auto& a, const auto& b) {
            return a ^ b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    inline constexpr auto& operator^=(Arrnd& lhs, const T& rhs)
    {
        return lhs.apply([&rhs](const auto& a) {
            return a ^ rhs;
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr auto operator&(const Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.transform(rhs, [](const auto& a, const auto& b) {
            return a & b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    [[nodiscard]] inline constexpr auto operator&(const Arrnd& lhs, const T& rhs)
    {
        return lhs.transform([&rhs](const auto& a) {
            return a & rhs;
        });
    }

    template <typename T, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto operator&(const T& lhs, const Arrnd& rhs)
    {
        return rhs.transform([&lhs](const auto& b) {
            return lhs & b;
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    inline constexpr auto& operator&=(Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.apply(rhs, [](const auto& a, const auto& b) {
            return a & b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    inline constexpr auto& operator&=(Arrnd& lhs, const T& rhs)
    {
        return lhs.apply([&rhs](const auto& a) {
            return a & rhs;
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr auto operator|(const Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.transform(rhs, [](const auto& a, const auto& b) {
            return a | b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    [[nodiscard]] inline constexpr auto operator|(const Arrnd& lhs, const T& rhs)
    {
        return lhs.transform([&rhs](const auto& a) {
            return a | rhs;
        });
    }

    template <typename T, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto operator|(const T& lhs, const Arrnd& rhs)
    {
        return rhs.transform([&lhs](const auto& b) {
            return lhs | b;
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    inline constexpr auto& operator|=(Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.apply(rhs, [](const auto& a, const auto& b) {
            return a | b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    inline constexpr auto& operator|=(Arrnd& lhs, const T& rhs)
    {
        return lhs.apply([&rhs](const auto& a) {
            return a | rhs;
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr auto operator<<(const Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.transform(rhs, [](const auto& a, const auto& b) {
            return a << b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    [[nodiscard]] inline constexpr auto operator<<(const Arrnd& lhs, const T& rhs)
    {
        return lhs.transform([&rhs](const auto& a) {
            return a << rhs;
        });
    }

    template <typename T, arrnd_type Arrnd>
        requires(!std::derived_from<T, std::ios_base> && !arrnd_type<T>)
    [[nodiscard]] inline constexpr auto operator<<(const T& lhs, const Arrnd& rhs)
    {
        return rhs.transform([&lhs](const auto& b) {
            return lhs << b;
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    inline constexpr auto& operator<<=(Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.apply(rhs, [](const auto& a, const auto& b) {
            return a << b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    inline constexpr auto& operator<<=(Arrnd& lhs, const T& rhs)
    {
        return lhs.apply([&rhs](const auto& a) {
            return a << rhs;
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr auto operator>>(const Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.transform(rhs, [](const auto& a, const auto& b) {
            return a >> b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    [[nodiscard]] inline constexpr auto operator>>(const Arrnd& lhs, const T& rhs)
    {
        return lhs.transform([&rhs](const auto& a) {
            return a >> rhs;
        });
    }

    template <typename T, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto operator>>(const T& lhs, const Arrnd& rhs)
    {
        return rhs.transform([&lhs](const auto& b) {
            return lhs >> b;
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    inline constexpr auto& operator>>=(Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.apply(rhs, [](const auto& a, const auto& b) {
            return a >> b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    inline constexpr auto& operator>>=(Arrnd& lhs, const T& rhs)
    {
        return lhs.apply([&rhs](const auto& a) {
            return a >> rhs;
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto operator~(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            return ~a;
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto operator!(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            return !a;
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto operator+(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            return +a;
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto operator-(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            return -a;
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto abs(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            using std::abs;
            return abs(a);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto acos(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            using std::acos;
            return acos(a);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto acosh(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            using std::acosh;
            return acosh(a);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto asin(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            using std::asin;
            return asin(a);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto asinh(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            using std::asinh;
            return asinh(a);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto atan(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            using std::atan;
            return atan(a);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto atanh(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            using std::atanh;
            return atanh(a);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto cos(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            using std::cos;
            return cos(a);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto cosh(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            using std::cosh;
            return cosh(a);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto exp(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            using std::exp;
            return exp(a);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto log(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            using std::log;
            return log(a);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto log10(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            using std::log10;
            return log10(a);
        });
    }

    template <typename Arrnd, typename U>
        requires(!arrnd_type<U>)
    [[nodiscard]] inline constexpr auto pow(const Arrnd& arr, const U& value)
    {
        return arr.transform([&value](const auto& a) {
            using std::pow;
            return pow(a, value);
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr auto pow(const Arrnd1& arr, const Arrnd2& values)
    {
        return arr.transform(values, [](const auto& a, const auto& b) {
            using std::pow;
            return pow(a, b);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto sin(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            using std::sin;
            return sin(a);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto sinh(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            using std::sinh;
            return sinh(a);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto sqrt(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            using std::sqrt;
            return sqrt(a);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto tan(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            using std::tan;
            return tan(a);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto tanh(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            using std::tanh;
            return tanh(a);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto round(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            using std::round;
            return round(a);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto ceil(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            using std::ceil;
            return ceil(a);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto floor(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            using std::floor;
            return floor(a);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto real(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            using std::real;
            return real(a);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto imag(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            using std::imag;
            return imag(a);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto arg(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            using std::arg;
            return arg(a);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto norm(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            using std::norm;
            return norm(a);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto conj(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            using std::conj;
            return conj(a);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto proj(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            using std::proj;
            return proj(a);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto polar(const Arrnd& arr)
    {
        return arr.transform([](const auto& r) {
            using std::polar;
            return polar(r);
        });
    }

    template <arrnd_type Arrnd1, typename Arrnd2>
    [[nodiscard]] inline constexpr auto polar(const Arrnd1& arr, const Arrnd2& thetas)
    {
        return arr.transform(thetas, [](const auto& r, const auto& theta) {
            using std::polar;
            return polar(r, theta);
        });
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto sign(const Arrnd& arr)
    {
        return arr.transform([](const auto& a) {
            return details::sign(a);
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr auto operator&&(const Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.transform(rhs, [](const auto& a, const auto& b) {
            return a && b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    [[nodiscard]] inline constexpr auto operator&&(const Arrnd& lhs, const T& rhs)
    {
        return lhs.transform([&rhs](const auto& a) {
            return a && rhs;
        });
    }

    template <typename T, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto operator&&(const T& lhs, const Arrnd& rhs)
    {
        return rhs.transform([&lhs](const auto& b) {
            return lhs && b;
        });
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr auto operator||(const Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.transform(rhs, [](const auto& a, const auto& b) {
            return a || b;
        });
    }

    template <arrnd_type Arrnd, typename T>
    [[nodiscard]] inline constexpr auto operator||(const Arrnd& lhs, const T& rhs)
    {
        return lhs.transform([&rhs](const auto& a) {
            return a || rhs;
        });
    }

    template <typename T, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto operator||(const T& lhs, const Arrnd& rhs)
    {
        return rhs.transform([&lhs](const auto& b) {
            return lhs || b;
        });
    }

    template <arrnd_type Arrnd>
    inline constexpr auto& operator++(Arrnd& arr)
    {
        if (arr.empty()) {
            return arr;
        }

        for (auto& value : arr) {
            ++value;
        }
        return arr;
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto operator++(Arrnd&& arr)
    {
        return operator++(arr);
    }

    template <arrnd_type Arrnd>
    inline constexpr auto operator++(Arrnd& arr, int)
    {
        Arrnd old = arr.clone();
        operator++(arr);
        return old;
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto operator++(Arrnd&& arr, int)
    {
        return operator++(arr, int{});
    }

    template <arrnd_type Arrnd>
    inline constexpr auto& operator--(Arrnd& arr)
    {
        if (arr.empty()) {
            return arr;
        }

        for (auto& value : arr) {
            --value;
        }
        return arr;
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto operator--(Arrnd&& arr)
    {
        return operator--(arr);
    }

    template <arrnd_type Arrnd>
    inline constexpr auto operator--(Arrnd& arr, int)
    {
        Arrnd old = arr.clone();
        operator--(arr);
        return old;
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto operator--(Arrnd&& arr, int)
    {
        return operator--(arr, int{});
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr bool all(const Arrnd& arr)
    {
        return arr.template all<Arrnd::depth>();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr bool any(const Arrnd& arr)
    {
        return arr.template any<Arrnd::depth>();
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr bool all_equal(const Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.template all_match<Arrnd1::depth>(rhs);
    }

    template <arrnd_type Arrnd, typename U>
        requires(!arrnd_type<U>)
    [[nodiscard]] inline constexpr bool all_equal(const Arrnd& arr, const U& u)
    {
        return arr.template all<Arrnd::depth>([&u](const auto& a) {
            return a == u;
        });
    }

    template <typename U, arrnd_type Arrnd>
        requires(!arrnd_type<U>)
    [[nodiscard]] inline constexpr bool all_equal(const U& u, const Arrnd& arr)
    {
        return all_equal<Arrnd, U>(arr, u);
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr bool all_close(const Arrnd1& lhs, const Arrnd2& rhs,
        const arrnd_tol_type<Arrnd1, Arrnd2>& atol = default_atol<arrnd_tol_type<Arrnd1, Arrnd2>>(),
        const arrnd_tol_type<Arrnd1, Arrnd2>& rtol = default_rtol<arrnd_tol_type<Arrnd1, Arrnd2>>())
    {
        return lhs.template all_match<Arrnd1::depth>(rhs, [&atol, &rtol](const auto& a, const auto& b) {
            return details::close(a, b, atol, rtol);
        });
    }

    template <arrnd_type Arrnd, typename U>
        requires(!arrnd_type<U>)
    [[nodiscard]] inline constexpr bool all_close(const Arrnd& arr, const U& u,
        const arrnd_lhs_tol_type<Arrnd, U>& atol = default_atol<arrnd_lhs_tol_type<Arrnd, U>>(),
        const arrnd_lhs_tol_type<Arrnd, U>& rtol = default_rtol<arrnd_lhs_tol_type<Arrnd, U>>())
    {
        return arr.template all<Arrnd::depth>([&u, &atol, &rtol](const auto& a) {
            return details::close(a, u, atol, rtol);
        });
    }

    template <typename U, arrnd_type Arrnd>
        requires(!arrnd_type<U>)
    [[nodiscard]] inline constexpr bool all_close(const U& u, const Arrnd& arr,
        const arrnd_rhs_tol_type<U, Arrnd>& atol = default_atol<arrnd_rhs_tol_type<U, Arrnd>>(),
        const arrnd_rhs_tol_type<U, Arrnd>& rtol = default_rtol<arrnd_rhs_tol_type<U, Arrnd>>())
    {
        return all_close<Arrnd, U>(arr, u, atol, rtol);
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr bool any_equal(const Arrnd1& lhs, const Arrnd2& rhs)
    {
        return lhs.template any_match<Arrnd1::depth>(rhs);
    }

    template <arrnd_type Arrnd, typename U>
        requires(!arrnd_type<U>)
    [[nodiscard]] inline constexpr bool any_equal(const Arrnd& arr, const U& u)
    {
        return arr.template any<Arrnd::depth>([&u](const auto& a) {
            return a == u;
        });
    }

    template <typename U, arrnd_type Arrnd>
        requires(!arrnd_type<U>)
    [[nodiscard]] inline constexpr bool any_equal(const U& u, const Arrnd& arr)
    {
        return any_equal<Arrnd, U>(arr, u);
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr bool any_close(const Arrnd1& lhs, const Arrnd2& rhs,
        const arrnd_tol_type<Arrnd1, Arrnd2>& atol = default_atol<arrnd_tol_type<Arrnd1, Arrnd2>>(),
        const arrnd_tol_type<Arrnd1, Arrnd2>& rtol = default_rtol<arrnd_tol_type<Arrnd1, Arrnd2>>())
    {
        return lhs.template any_match<Arrnd1::depth>(rhs, [&atol, &rtol](const auto& a, const auto& b) {
            return details::close(a, b, atol, rtol);
        });
    }

    template <arrnd_type Arrnd, typename U>
        requires(!arrnd_type<U>)
    [[nodiscard]] inline constexpr bool any_close(const Arrnd& arr, const U& u,
        const arrnd_lhs_tol_type<Arrnd, U>& atol = default_atol<arrnd_lhs_tol_type<Arrnd, U>>(),
        const arrnd_lhs_tol_type<Arrnd, U>& rtol = default_rtol<arrnd_lhs_tol_type<Arrnd, U>>())
    {
        return arr.template any<Arrnd::depth>([&u, &atol, &rtol](const auto& a) {
            return details::close(a, u, atol, rtol);
        });
    }

    template <typename U, arrnd_type Arrnd>
        requires(!arrnd_type<U>)
    [[nodiscard]] inline constexpr bool any_close(const U& u, const Arrnd& arr,
        const arrnd_rhs_tol_type<U, Arrnd>& atol = default_atol<arrnd_rhs_tol_type<U, Arrnd>>(),
        const arrnd_rhs_tol_type<U, Arrnd>& rtol = default_rtol<arrnd_rhs_tol_type<U, Arrnd>>())
    {
        return any_close<Arrnd, U>(arr, u, atol, rtol);
    }

    template <arrnd_type Arrnd>
    inline std::ostream& ostream_operator_recursive(std::ostream& os, const Arrnd& arr,
        typename Arrnd::size_type nvectical_spaces, typename Arrnd::size_type ndepth_spaces)
    {
        constexpr auto block_start_char = Arrnd::depth > 0 ? '{' : '[';
        constexpr auto block_stop_char = Arrnd::depth > 0 ? '}' : ']';

        if (arr.empty()) {
            os << block_start_char << block_stop_char;
            return os;
        }

        if constexpr (Arrnd::is_flat) {
            if (std::ssize(arr.info().dims()) > 1) {
                os << block_start_char;
                for (typename Arrnd::size_type i = 0; i < arr.info().dims()[0]; ++i) {
                    if (i > 0) {
                        for (typename Arrnd::size_type i = 0; i < ndepth_spaces + nvectical_spaces + 1; ++i) {
                            os << ' ';
                        }
                    }
                    ostream_operator_recursive(
                        os, arr[typename Arrnd::boundary_type{i, i + 1}], nvectical_spaces + 1, ndepth_spaces);
                    if (i < arr.info().dims()[0] - 1) {
                        os << '\n';
                    }
                }
                os << block_stop_char;
                return os;
            }

            os << block_start_char;
            auto arr_it = arr.cbegin();
            os << *arr_it;
            ++arr_it;
            for (; arr_it != arr.cend(); ++arr_it) {
                os << ' ' << *arr_it;
            }
            os << block_stop_char;
        } else {
            os << block_start_char;
            typename Arrnd::size_type inner_count = 0;
            for (const auto& inner : arr) {
                ostream_operator_recursive(os, inner, nvectical_spaces, ndepth_spaces + 1);
                if (++inner_count < total(arr.info())) {
                    os << '\n';
                    for (typename Arrnd::size_type i = 0; i < ndepth_spaces + 1; ++i) {
                        os << ' ';
                    }
                }
            }
            os << block_stop_char;
        }

        return os;
    }

    template <arrnd_type Arrnd>
    inline constexpr std::ostream& operator<<(std::ostream& os, const Arrnd& arr)
    {
        typename Arrnd::size_type nvectical_spaces = 0;
        typename Arrnd::size_type ndepth_spaces = 0;
        return ostream_operator_recursive(os, arr, nvectical_spaces, ndepth_spaces);
    }

    struct arrnd_json_manip {
        explicit arrnd_json_manip(std::ostream& os)
            : os_(os)
        { }

        template <typename T>
            requires(!arrnd_type<T>)
        friend std::ostream& operator<<(const arrnd_json_manip& ajm, const T& rhs)
        {
            return ajm.os_ << rhs;
        }

        template <arrnd_type Arrnd>
        friend std::ostream& operator<<(const arrnd_json_manip& ajm, const Arrnd& arr)
        {
            typename Arrnd::size_type nvertical_spaces = 4;
            ajm.os_ << "{\n";
            ajm.os_ << std::string(nvertical_spaces, ' ') << "\"base_type\": \""
                    << type_name<typename Arrnd::template inner_type<Arrnd::depth>::value_type>() << "\"\n";
            to_json(ajm.os_, arr, nvertical_spaces);
            ajm.os_ << "}";
            return ajm.os_;
        }

    private:
        template <arrnd_type Arrnd>
        static std::ostream& to_json(std::ostream& os, const Arrnd& arr, typename Arrnd::size_type nvertical_spaces)
        {
            auto replace_newlines = [](std::string s) {
                std::string::size_type n = 0;
                std::string d = s;
                while ((n = d.find("\n", n)) != std::string::npos) {
                    d.replace(n, 1, "\\n");
                    n += 2;
                }
                return d;
            };

            if (arr.empty()) {
                os << std::string(nvertical_spaces, ' ') << "\"info\": \"empty\",\n";
                os << std::string(nvertical_spaces, ' ') << "\"values\": \"empty\"\n";
                return os;
            }

            if constexpr (Arrnd::is_flat) {
                // info
                {
                    std::stringstream ss;
                    ss << arr.info();
                    os << std::string(nvertical_spaces, ' ') << "\"info\": \"" << replace_newlines(ss.str()) << "\",\n";
                }
                // array
                {
                    std::stringstream ss;
                    ss << arr;
                    os << std::string(nvertical_spaces, ' ') << "\"values\": \"" << replace_newlines(ss.str())
                       << "\"\n";
                }
            } else {
                // info
                {
                    std::stringstream ss;
                    ss << arr.info();
                    os << std::string(nvertical_spaces, ' ') << "\"info\": \"" << replace_newlines(ss.str()) << "\",\n";
                }
                // arrays
                os << std::string(nvertical_spaces, ' ') << "\"arrays\": [\n";
                auto arr_it = arr.cbegin();
                for (typename Arrnd::size_type i = 0; arr_it != arr.cend(); ++arr_it, ++i) {
                    os << std::string(nvertical_spaces + 4, ' ') << "{\n";
                    to_json(os, *arr_it, nvertical_spaces + 8);
                    os << std::string(nvertical_spaces + 4, ' ') << '}';
                    if (i < total(arr.info()) - 1) {
                        os << ',';
                    }
                    os << '\n';
                }
                os << std::string(nvertical_spaces, ' ') << "]\n";
            }

            return os;
        }

        std::ostream& os_;
    };

    static constexpr struct arrnd_json_manip_tag {
    } arrnd_json{};
    inline arrnd_json_manip operator<<(std::ostream& os, arrnd_json_manip_tag)
    {
        return arrnd_json_manip(os);
    }
}

using details::arrnd_type;
using details::arrnd_json;

using details::arrnd_traversal_type;
using details::arrnd_traversal_result;
using details::arrnd_traversal_container;

using details::arrnd_common_shape;
using details::arrnd_lazy_filter;
using details::arrnd;

using details::begin;
using details::cbegin;
using details::end;
using details::cend;
using details::rbegin;
using details::crbegin;
using details::rend;
using details::crend;

using details::concat;

using details::all;
using details::any;
using details::all_equal;
using details::all_close;
using details::any_equal;
using details::any_close;

using details::sum;
using details::prod;
using details::min;
using details::max;
using details::dot;
using details::det;
using details::inv;
using details::close;
using details::abs;
using details::acos;
using details::acosh;
using details::asin;
using details::asinh;
using details::atan;
using details::atanh;
using details::cos;
using details::cosh;
using details::exp;
using details::log;
using details::log10;
using details::pow;
using details::sin;
using details::sinh;
using details::sqrt;
using details::tan;
using details::tanh;
using details::round;
using details::ceil;
using details::floor;
using details::real;
using details::imag;
using details::arg;
using details::norm;
using details::conj;
using details::proj;
using details::polar;
using details::sign;

using details::zeros;
using details::eye;

}

#endif // OC_ARRAY_H