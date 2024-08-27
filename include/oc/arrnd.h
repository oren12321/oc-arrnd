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
void swap(Tuple&& lhs, Tuple&& rhs)
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
        using cont_type = Cont;
        using iter_type = decltype(begin(std::declval<Cont&>(), Args{}...));
        using riter_type = decltype(rbegin(std::declval<Cont&>(), Args{}...));

        constexpr zipped_container(Cont& cont, Args&&... args)
            : cont_(cont)
            , args_(std::forward_as_tuple(std::forward<Args>(args)...))
        { }

        constexpr zipped_container(const zipped_container&) = default;
        constexpr zipped_container(zipped_container&&) = default;

        constexpr zipped_container& operator=(const zipped_container&) = default;
        constexpr zipped_container& operator=(zipped_container&&) = default;

        constexpr virtual ~zipped_container() = default;

        constexpr auto& cont() noexcept
        {
            return cont_;
        }
        constexpr const auto& cont() const noexcept
        {
            return cont_;
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
        Cont& cont_;
        std::tuple<Args...> args_ = std::tuple<>{};
    };

    template <iterator_type InputIt>
    class zipped_iterator {
    public:
        struct unknown { };
        using cont_type = unknown;
        using iter_type = InputIt;
        using riter_type = InputIt;

        constexpr zipped_iterator(const InputIt& first, const InputIt& last)
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

    template <typename... ItPack>
    class zip {
    public:
        struct iterator {
            using iterator_category = std::random_access_iterator_tag;
            using value_type = std::tuple<std::iter_value_t<typename ItPack::iter_type>...>;
            using reference = std::tuple<std::iter_reference_t<typename ItPack::iter_type>...>;
            using difference_type = std::int64_t;

            [[nodiscard]] constexpr reference operator*() const
            {
                return std::apply(
                    []<typename... Ts>(Ts&&... e) {
                        return std::forward_as_tuple(*std::forward<Ts>(e)...);
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
                return !any_equals(data_, iter.data_, std::index_sequence_for<typename ItPack::cont_type...>{});
            }

            [[nodiscard]] constexpr auto operator==(const iterator& iter) const
            {
                return all_equals(data_, iter.data_, std::index_sequence_for<typename ItPack::cont_type...>{});
            }

            [[nodiscard]] constexpr reference operator[](std::int64_t index) const
            {
                return std::apply(
                    [index]<typename... Ts>(Ts&&... e) {
                        return std::forward_as_tuple(std::forward<Ts>(e)[index]...);
                    },
                    data_);
            }

            [[nodiscard]] constexpr bool operator<(const iterator& iter) const noexcept
            {
                return all_lesseq(data_, iter.data_, std::index_sequence_for<typename ItPack::cont_type...>{});
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

                auto diffs = impl(data_, iter.data_, std::index_sequence_for<typename ItPack::cont_type...>{});
                return diffs;
            }

            std::tuple<typename ItPack::iter_type...> data_;
        };

        struct reverse_iterator {
            using iterator_category = std::random_access_iterator_tag;
            using value_type = std::tuple<std::iter_value_t<typename ItPack::riter_type>...>;
            using reference = std::tuple<std::iter_reference_t<typename ItPack::riter_type>...>;
            using difference_type = std::int64_t;

            [[nodiscard]] constexpr reference operator*() const
            {
                return std::apply(
                    []<typename... Ts>(Ts&&... e) {
                        return std::forward_as_tuple(*std::forward<Ts>(e)...);
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
                return !any_equals(data_, iter.data_, std::index_sequence_for<typename ItPack::cont_type...>{});
            }

            [[nodiscard]] constexpr auto operator==(const reverse_iterator& iter) const
            {
                return all_equals(data_, iter.data_, std::index_sequence_for<typename ItPack::cont_type...>{});
            }

            [[nodiscard]] constexpr reference operator[](std::int64_t index) const
            {
                return std::apply(
                    [index]<typename... Ts>(Ts&&... e) {
                        return std::forward_as_tuple(std::forward<Ts>(e)[index]...);
                    },
                    data_);
            }

            [[nodiscard]] constexpr bool operator<(const reverse_iterator& iter) const noexcept
            {
                return all_lesseq(data_, iter.data_, std::index_sequence_for<typename ItPack::cont_type...>{});
            }

            [[nodiscard]] constexpr std::int64_t operator-(const reverse_iterator& iter) const noexcept
            {
                auto impl
                    = []<typename T1, typename T2, std::size_t... I>(T1&& t1, T2&& t2, std::index_sequence<I...>) {
                          return std::max({
                              (std::get<I>(std::forward<T1>(t1)) - std::get<I>(std::forward<T2>(t2)))...,
                          });
                      };

                auto diffs = impl(data_, iter.data_, std::index_sequence_for<typename ItPack::cont_type...>{});
                return diffs;
            }

            std::tuple<typename ItPack::riter_type...> data_;
        };

        zip(ItPack... packs)
            : packs_(std::forward_as_tuple(packs...))
        { }

        auto begin()
        {
            auto impl = []<typename P, std::size_t... I>(P ip, std::index_sequence<I...>) {
                if constexpr (template_type<P, zipped_container>) {
                    using std::begin;
                    return begin(ip.cont(), std::get<I>(ip.args())...);
                } else {
                    return ip.first();
                }
            };

            return iterator(std::apply(
                [&]<typename... Ts>(Ts&&... e) {
                    return std::make_tuple(impl(std::forward<Ts>(e),
                        std::make_index_sequence<
                            std::tuple_size_v<std::remove_cvref_t<decltype(std::forward<Ts>(e).args())>>>{})...);
                },
                packs_));
        }

        auto end()
        {
            auto impl = []<typename P, std::size_t... I>(P ip, std::index_sequence<I...>) {
                if constexpr (template_type<P, zipped_container>) {
                    using std::end;
                    return end(ip.cont(), std::get<I>(ip.args())...);
                } else {
                    return ip.last();
                }
            };

            return iterator{std::apply(
                [&]<typename... Ts>(Ts&&... e) {
                    return std::make_tuple(impl(std::forward<Ts>(e),
                        std::make_index_sequence<
                            std::tuple_size_v<std::remove_cvref_t<decltype(std::forward<Ts>(e).args())>>>{})...);
                },
                packs_)};
        }

        auto rbegin()
        {
            auto impl = []<typename P, std::size_t... I>(P ip, std::index_sequence<I...>) {
                if constexpr (template_type<P, zipped_container>) {
                    using std::rbegin;
                    return rbegin(ip.cont(), std::get<I>(ip.args())...);
                } else {
                    return ip.first();
                }
            };

            return reverse_iterator(std::apply(
                [&]<typename... Ts>(Ts&&... e) {
                    return std::make_tuple(impl(std::forward<Ts>(e),
                        std::make_index_sequence<
                            std::tuple_size_v<std::remove_cvref_t<decltype(std::forward<Ts>(e).args())>>>{})...);
                },
                packs_));
        }

        auto rend()
        {
            auto impl = []<typename P, std::size_t... I>(P ip, std::index_sequence<I...>) {
                if constexpr (template_type<P, zipped_container>) {
                    using std::rend;
                    return rend(ip.cont(), std::get<I>(ip.args())...);
                } else {
                    return ip.last();
                }
            };

            return reverse_iterator{std::apply(
                [&]<typename... Ts>(Ts&&... e) {
                    return std::make_tuple(impl(std::forward<Ts>(e),
                        std::make_index_sequence<
                            std::tuple_size_v<std::remove_cvref_t<decltype(std::forward<Ts>(e).args())>>>{})...);
                },
                packs_)};
        }

    private:
        std::tuple<ItPack...> packs_;
    };
}

using details::zipped_container;
using details::zipped_iterator;
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
        using allocator_type = Allocator<U>;
    };

    template <typename T, std::int64_t N>
    struct simple_array_traits {
        using storage_type = simple_array<T, N>;
        template <typename U>
        using replaced_type = simple_array_traits<U, N>;
        template <typename U>
        using allocator_type = simple_allocator<U>;
    };
}

using details::simple_vector_traits;
using details::simple_array_traits;
}

namespace oc::arrnd {
namespace details {
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
        const decltype(T1{} - T2{})& atol = default_atol<decltype(T1{} - T2{})>(),
        const decltype(T1{} - T2{})& rtol = default_rtol<decltype(T1{} - T2{})>()) noexcept
    {
        using std::abs;
        if (a == b) {
            return true;
        }
        const decltype(a - b) reps{rtol * (abs(a) > abs(b) ? abs(a) : abs(b))};
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
    [[nodiscard]] constexpr bool operator==(const interval<T>& lhs, const interval<U>& rhs) noexcept
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

    template <typename StorageInfo = simple_vector_traits<std::size_t>>
        requires(std::is_same_v<std::size_t, typename StorageInfo::storage_type::value_type>)
    class arrnd_info {
    public:
        using storage_info_type = StorageInfo;

        using extent_storage_type = typename storage_info_type::storage_type;
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

    template <typename StorageInfo, iterator_of_type_interval InputIt>
    [[nodiscard]] inline constexpr arrnd_info<StorageInfo> slice(
        const arrnd_info<StorageInfo>& ai, InputIt first_dim_boundary, InputIt last_dim_boundary)
    {
        if (std::distance(first_dim_boundary, last_dim_boundary) < 0) {
            throw std::invalid_argument("invalid input iterators distance < 0");
        }

        if (std::size(ai.dims()) == 0 && std::distance(first_dim_boundary, last_dim_boundary) == 0) {
            return arrnd_info<StorageInfo>(ai.hints() | arrnd_hint::continuous | arrnd_hint::sliced);
        }

        if (std::size(ai.dims()) < std::distance(first_dim_boundary, last_dim_boundary)) {
            throw std::invalid_argument("invalid dim boundaries - different number from dims");
        }

        // input boundries might not be of the same type
        typename arrnd_info<StorageInfo>::storage_info_type::template replaced_type<
            typename arrnd_info<StorageInfo>::boundary_type>::storage_type
            bounded_dim_boundaries(std::distance(first_dim_boundary, last_dim_boundary));

        std::transform(first_dim_boundary, last_dim_boundary, std::begin(ai.dims()), std::begin(bounded_dim_boundaries),
            [](auto boundary, auto dim) {
                return bound(static_cast<typename arrnd_info<StorageInfo>::boundary_type>(boundary), 0, dim);
            });

        if (!std::transform_reduce(std::begin(bounded_dim_boundaries), std::end(bounded_dim_boundaries),
                std::begin(ai.dims()), true, std::logical_and<>{}, [](auto boundary, auto dim) {
                    return empty(boundary) || isbetween(boundary, 0, dim);
                })) {
            throw std::out_of_range("invalid dim boundaries - not suitable for dims");
        }

        typename arrnd_info<StorageInfo>::extent_storage_type dims = ai.dims();
        std::transform(
            std::begin(bounded_dim_boundaries), std::end(bounded_dim_boundaries), std::begin(dims), [](auto boundary) {
                return absdiff(boundary);
            });

        if (std::any_of(std::begin(dims), std::end(dims), [](auto dim) {
                return dim == 0;
            })) {
            return arrnd_info<StorageInfo>(ai.hints() | arrnd_hint::continuous | arrnd_hint::sliced);
        }

        if (std::equal(std::begin(dims), std::end(dims), std::begin(ai.dims()), std::end(ai.dims()))) {
            return ai;
        }

        typename arrnd_info<StorageInfo>::extent_storage_type strides(std::begin(ai.strides()), std::end(ai.strides()));

        std::transform(std::begin(bounded_dim_boundaries), std::end(bounded_dim_boundaries), std::begin(ai.strides()),
            std::begin(strides), [](auto boundary, auto stride) {
                if (boundary.step()
                    > std::numeric_limits<typename arrnd_info<StorageInfo>::extent_type>::max() / stride) {
                    throw std::overflow_error("invalid multiplication");
                }
                return boundary.step() * stride;
            });

        typename arrnd_info<StorageInfo>::extent_type offset = ai.indices_boundary().start()
            + std::transform_reduce(std::begin(bounded_dim_boundaries), std::end(bounded_dim_boundaries),
                std::begin(ai.strides()), typename arrnd_info<StorageInfo>::extent_type{0},
                overflow_check_plus<typename arrnd_info<StorageInfo>::extent_type>{}, [](auto boundary, auto stride) {
                    if (boundary.start()
                        > std::numeric_limits<typename arrnd_info<StorageInfo>::extent_type>::max() / stride) {
                        throw std::overflow_error("invalid multiplication");
                    }
                    return boundary.start() * stride;
                });

        typename arrnd_info<StorageInfo>::extent_type max_index = std::transform_reduce(std::begin(dims),
            std::end(dims), std::begin(strides), typename arrnd_info<StorageInfo>::extent_type{0},
            overflow_check_plus<typename arrnd_info<StorageInfo>::extent_type>{}, [](auto dim, auto stride) {
                if (dim - 1 > std::numeric_limits<typename arrnd_info<StorageInfo>::extent_type>::max() / stride) {
                    throw std::overflow_error("invalid multiplication");
                }
                return (dim - 1) * stride;
            });

        typename arrnd_info<StorageInfo>::boundary_type indices_boundary(offset, offset + max_index + 1);

        typename arrnd_info<StorageInfo>::extent_type num_elem
            = std::reduce(std::begin(dims), std::end(dims), typename arrnd_info<StorageInfo>::extent_type{1},
                overflow_check_multiplies<typename arrnd_info<StorageInfo>::extent_type>{});

        arrnd_hint hints = ai.hints() | arrnd_hint::sliced;
        if (max_index + 1 == num_elem) {
            hints |= arrnd_hint::continuous;
        } else {
            hints &= ~arrnd_hint::continuous;
        }

        return arrnd_info<StorageInfo>(dims, strides, indices_boundary, hints);
    }

    template <typename StorageInfo, iterable_of_type_interval Cont>
    [[nodiscard]] inline constexpr arrnd_info<StorageInfo> slice(
        const arrnd_info<StorageInfo>& ai, Cont&& dim_boundaries)
    {
        return slice(ai, std::begin(dim_boundaries), std::end(dim_boundaries));
    }

    template <typename StorageInfo>
    [[nodiscard]] inline constexpr arrnd_info<StorageInfo> slice(const arrnd_info<StorageInfo>& ai,
        std::initializer_list<typename arrnd_info<StorageInfo>::boundary_type> dim_boundaries)
    {
        return slice(ai, dim_boundaries.begin(), dim_boundaries.end());
    }

    template <typename StorageInfo>
    [[nodiscard]] inline constexpr arrnd_info<StorageInfo> slice(const arrnd_info<StorageInfo>& ai,
        template_type<interval> auto dim_boundary, typename arrnd_info<StorageInfo>::extent_type axis)
    {
        if (std::size(ai.dims()) == 0) {
            throw std::invalid_argument("invalid arrnd info");
        }

        if (axis >= std::size(ai.dims())) {
            throw std::out_of_range("invalid axis");
        }

        typename arrnd_info<StorageInfo>::storage_info_type::template replaced_type<
            typename arrnd_info<StorageInfo>::boundary_type>::storage_type dim_boundaries(std::size(ai.dims()),
            arrnd_info<StorageInfo>::boundary_type::full());

        dim_boundaries[axis] = static_cast<typename arrnd_info<StorageInfo>::boundary_type>(dim_boundary);

        return slice(ai, dim_boundaries);
    }

    template <typename StorageInfo>
    [[nodiscard]] inline constexpr arrnd_info<StorageInfo> squeeze(const arrnd_info<StorageInfo>& ai,
        arrnd_squeeze_type squeeze_type,
        typename arrnd_info<StorageInfo>::extent_type max_count
        = std::numeric_limits<typename arrnd_info<StorageInfo>::extent_type>::max())
    {
        if (squeeze_type == arrnd_squeeze_type::full
            && max_count != std::numeric_limits<typename arrnd_info<StorageInfo>::extent_type>::max()) {
            throw std::invalid_argument("invalid squeeze type - no max count support for full squeeze type");
        }

        if (size(ai) == 0) {
            if (max_count != std::numeric_limits<typename arrnd_info<StorageInfo>::extent_type>::max()
                && max_count != 0) {
                throw std::invalid_argument("invalid squeeze - empty squeeze support for max count > 0");
            }
            return ai;
        }

        if (size(ai) == 1) {
            if (max_count != std::numeric_limits<typename arrnd_info<StorageInfo>::extent_type>::max()
                && max_count > 1) {
                throw std::invalid_argument("invalid squeeze - no 1d squeeze support for max count > 1");
            }
            if (max_count == 0 || (max_count == 1 && ai.dims()[0] == 1)) {
                return ai;
            }

            return arrnd_info<StorageInfo>(typename arrnd_info<StorageInfo>::extent_storage_type(
                                               1, typename arrnd_info<StorageInfo>::extent_type{1}),
                typename arrnd_info<StorageInfo>::extent_storage_type(1, ai.strides()[0]), ai.indices_boundary(),
                ai.hints());
        }

        typename arrnd_info<StorageInfo>::extent_storage_type dims(ai.dims());
        typename arrnd_info<StorageInfo>::extent_storage_type strides(ai.strides());

        if (squeeze_type == arrnd_squeeze_type::left || squeeze_type == arrnd_squeeze_type::trim) {
            typename arrnd_info<StorageInfo>::extent_type left_ones_count = 0;
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
            typename arrnd_info<StorageInfo>::extent_type right_ones_count = 0;
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
                typename arrnd_info<StorageInfo>::extent_type not_one_index = 0;
                for (typename arrnd_info<StorageInfo>::extent_type i = 0; i < std::size(dims); ++i) {
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

        return arrnd_info<StorageInfo>(dims, strides, ai.indices_boundary(), ai.hints());
    }

    template <typename StorageInfo, iterator_of_type_integral InputIt>
    [[nodiscard]] inline constexpr arrnd_info<StorageInfo> transpose(
        const arrnd_info<StorageInfo>& ai, InputIt first_axis, InputIt last_axis)
    {
        if (std::distance(first_axis, last_axis) != std::size(ai.dims())) {
            throw std::invalid_argument("invalid input iterators distance != number of dims");
        }

        if (empty(ai)) {
            return ai;
        }

        if (std::size(ai.dims()) == 0) {
            if (*first_axis != 0) {
                throw std::out_of_range("invalid axis value");
            }
            return ai;
        }

        if (std::any_of(first_axis, last_axis, [&ai](std::size_t axis) {
                return axis < 0 || axis >= std::size(ai.dims());
            })) {
            throw std::out_of_range("invalid axis value");
        }

        auto is_unique_axes = [](InputIt first, InputIt last) {
            typename arrnd_info<StorageInfo>::extent_storage_type sorted_axes(first, last);
            std::sort(std::begin(sorted_axes), std::end(sorted_axes));
            return std::adjacent_find(std::begin(sorted_axes), std::end(sorted_axes)) == std::end(sorted_axes);
        };

        if (!is_unique_axes(first_axis, last_axis)) {
            throw std::invalid_argument("not unique axes");
        }

        if (std::is_sorted(first_axis, last_axis)) {
            return ai;
        }

        typename arrnd_info<StorageInfo>::extent_storage_type dims(std::size(ai.dims()));
        typename arrnd_info<StorageInfo>::extent_storage_type strides(std::size(ai.strides()));

        typename arrnd_info<StorageInfo>::extent_type i = 0;
        for (auto axes_it = first_axis; axes_it != last_axis; ++axes_it, ++i) {
            dims[i] = ai.dims()[*axes_it];
            strides[i] = ai.strides()[*axes_it];
        }

        return arrnd_info<StorageInfo>(dims, strides, ai.indices_boundary(), ai.hints() | arrnd_hint::transposed);
    }

    template <typename StorageInfo, iterable_of_type_integral Cont>
    [[nodiscard]] inline constexpr arrnd_info<StorageInfo> transpose(const arrnd_info<StorageInfo>& ai, Cont&& axes)
    {
        return transpose(ai, std::begin(axes), std::end(axes));
    }

    template <typename StorageInfo>
    [[nodiscard]] inline constexpr arrnd_info<StorageInfo> transpose(
        const arrnd_info<StorageInfo>& ai, std::initializer_list<typename arrnd_info<StorageInfo>::extent_type> axes)
    {
        return transpose(ai, axes.begin(), axes.end());
    }

    template <typename StorageInfo>
    [[nodiscard]] inline constexpr arrnd_info<StorageInfo> swap(const arrnd_info<StorageInfo>& ai,
        typename arrnd_info<StorageInfo>::extent_type first_axis = 0,
        typename arrnd_info<StorageInfo>::extent_type second_axis = 1)
    {

        if (first_axis >= std::size(ai.dims()) || second_axis >= std::size(ai.dims())) {
            throw std::out_of_range("invalid axis value");
        }

        if (first_axis == second_axis) {
            return ai;
        }

        typename arrnd_info<StorageInfo>::extent_storage_type dims(ai.dims());
        typename arrnd_info<StorageInfo>::extent_storage_type strides(ai.strides());

        std::swap(dims[first_axis], dims[second_axis]);
        std::swap(strides[first_axis], strides[second_axis]);

        return arrnd_info<StorageInfo>(dims, strides, ai.indices_boundary(), ai.hints() | arrnd_hint::transposed);
    }

    template <typename StorageInfo>
    [[nodiscard]] inline constexpr arrnd_info<StorageInfo> move(const arrnd_info<StorageInfo>& ai,
        typename arrnd_info<StorageInfo>::extent_type src_axis = 0,
        typename arrnd_info<StorageInfo>::extent_type dst_axis = 1)
    {

        if (src_axis >= std::size(ai.dims()) || dst_axis >= std::size(ai.dims())) {
            throw std::out_of_range("invalid axis value");
        }

        if (src_axis == dst_axis) {
            return ai;
        }

        typename arrnd_info<StorageInfo>::extent_storage_type dims(ai.dims());
        typename arrnd_info<StorageInfo>::extent_storage_type strides(ai.strides());

        auto src_dim = dims[src_axis];
        dims.erase(std::next(std::begin(dims), src_axis));
        dims.insert(std::next(std::begin(dims), dst_axis), 1, src_dim);

        auto src_stride = strides[src_axis];
        strides.erase(std::next(std::begin(strides), src_axis));
        strides.insert(std::next(std::begin(strides), dst_axis), 1, src_stride);

        return arrnd_info<StorageInfo>(dims, strides, ai.indices_boundary(), ai.hints() | arrnd_hint::transposed);
    }

    template <typename StorageInfo>
    [[nodiscard]] inline constexpr arrnd_info<StorageInfo> roll(const arrnd_info<StorageInfo>& ai, int offset = 1)
    {
        if (empty(ai) || std::size(ai.dims()) == 1 || offset == 0) {
            return ai;
        }

        typename arrnd_info<StorageInfo>::extent_storage_type dims(ai.dims());
        typename arrnd_info<StorageInfo>::extent_storage_type strides(ai.strides());

        typename arrnd_info<StorageInfo>::extent_type wrapped_offset
            = static_cast<typename arrnd_info<StorageInfo>::extent_type>(std::abs(offset)) % std::size(ai.dims());

        if (offset > 0) {
            std::rotate(std::rbegin(dims), std::next(std::rbegin(dims), wrapped_offset), std::rend(dims));
            std::rotate(std::rbegin(strides), std::next(std::rbegin(strides), wrapped_offset), std::rend(strides));
        } else {
            std::rotate(std::begin(dims), std::next(std::begin(dims), wrapped_offset), std::end(dims));
            std::rotate(std::begin(strides), std::next(std::begin(strides), wrapped_offset), std::end(strides));
        }

        return arrnd_info<StorageInfo>(dims, strides, ai.indices_boundary(), ai.hints() | arrnd_hint::transposed);
    }

    template <typename StorageInfo>
    [[nodiscard]] inline constexpr typename arrnd_info<StorageInfo>::extent_type total(
        const arrnd_info<StorageInfo>& ai)
    {
        if (empty(ai)) {
            return 0;
        }

        return std::reduce(std::begin(ai.dims()), std::end(ai.dims()), typename arrnd_info<StorageInfo>::extent_type{1},
            overflow_check_multiplies<typename arrnd_info<StorageInfo>::extent_type>{});
    }

    template <typename StorageInfo>
    [[nodiscard]] inline constexpr typename arrnd_info<StorageInfo>::extent_type size(const arrnd_info<StorageInfo>& ai)
    {
        return std::size(ai.dims());
    }

    template <typename StorageInfo>
    [[nodiscard]] inline constexpr bool empty(const arrnd_info<StorageInfo>& ai)
    {
        return size(ai) == 0;
    }

    template <typename StorageInfo>
    [[nodiscard]] inline constexpr bool iscontinuous(const arrnd_info<StorageInfo>& ai)
    {
        return static_cast<bool>(to_underlying(ai.hints() & arrnd_hint::continuous));
    }

    template <typename StorageInfo>
    [[nodiscard]] inline constexpr bool issliced(const arrnd_info<StorageInfo>& ai)
    {
        return static_cast<bool>(to_underlying(ai.hints() & arrnd_hint::sliced));
    }

    template <typename StorageInfo>
    [[nodiscard]] inline constexpr bool istransposed(const arrnd_info<StorageInfo>& ai)
    {
        return static_cast<bool>(to_underlying(ai.hints() & arrnd_hint::transposed));
    }

    template <typename StorageInfo>
    [[nodiscard]] inline constexpr bool isvector(const arrnd_info<StorageInfo>& ai)
    {
        return size(ai) == 1;
    }

    template <typename StorageInfo>
    [[nodiscard]] inline constexpr bool ismatrix(const arrnd_info<StorageInfo>& ai)
    {
        return size(ai) == 2;
    }

    template <typename StorageInfo>
    [[nodiscard]] inline constexpr bool isrow(const arrnd_info<StorageInfo>& ai)
    {
        return ismatrix(ai) && ai.dims().front() == 1;
    }

    template <typename StorageInfo>
    [[nodiscard]] inline constexpr bool iscolumn(const arrnd_info<StorageInfo>& ai)
    {
        return ismatrix(ai) && ai.dims().back() == 1;
    }

    template <typename StorageInfo>
    [[nodiscard]] inline constexpr bool isscalar(const arrnd_info<StorageInfo>& ai)
    {
        return total(ai) == 1;
    }

    template <typename StorageInfo, iterator_of_type_integral InputIt>
    constexpr typename arrnd_info<StorageInfo>::extent_type sub2ind(
        const arrnd_info<StorageInfo>& ai, InputIt first_sub, InputIt last_sub)
    {
        if (std::distance(first_sub, last_sub) <= 0) {
            throw std::invalid_argument("invalid input iterators distance <= 0");
        }

        if (std::distance(first_sub, last_sub) > size(ai)) {
            throw std::invalid_argument("bad sub iterators distance > ndi.ndims");
        }

        if (empty(ai)) {
            throw std::invalid_argument("undefined operation for empty arrnd info");
        }

        if (!std::transform_reduce(first_sub, last_sub,
                std::next(std::begin(ai.dims()), std::size(ai.dims()) - std::distance(first_sub, last_sub)), true,
                std::logical_and<>{}, [](auto sub, auto dim) {
                    return sub >= 0 && sub < dim;
                })) {
            throw std::out_of_range("invalid subs - not suitable for dims");
        }

        return ai.indices_boundary().start()
            + std::transform_reduce(first_sub, last_sub,
                std::next(std::begin(ai.strides()), std::size(ai.strides()) - std::distance(first_sub, last_sub)),
                typename arrnd_info<StorageInfo>::extent_type{0},
                overflow_check_plus<typename arrnd_info<StorageInfo>::extent_type>{},
                overflow_check_multiplies<typename arrnd_info<StorageInfo>::extent_type>{});
    }

    template <typename StorageInfo, iterable_of_type_integral Cont>
    constexpr typename arrnd_info<StorageInfo>::extent_type sub2ind(const arrnd_info<StorageInfo>& ai, Cont&& subs)
    {
        return sub2ind(ai, std::begin(subs), std::end(subs));
    }

    template <typename StorageInfo>
    constexpr typename arrnd_info<StorageInfo>::extent_type sub2ind(
        const arrnd_info<StorageInfo>& ai, std::initializer_list<typename arrnd_info<StorageInfo>::extent_type> subs)
    {
        return sub2ind(ai, subs.begin(), subs.end());
    }

    template <typename StorageInfo, iterator_of_type_integral OutputIt>
    constexpr void ind2sub(
        const arrnd_info<StorageInfo>& ai, typename arrnd_info<StorageInfo>::extent_type ind, OutputIt d_sub)
    {
        if (empty(ai)) {
            throw std::invalid_argument("undefined operation for empty arrnd info");
        }

        if (!isbetween(ai.indices_boundary(), ind)) {
            throw std::out_of_range("invalid ind - not inside indices boundary");
        }

        for (typename arrnd_info<StorageInfo>::extent_type i = 0; i < std::size(ai.dims()); ++i) {
            *d_sub = (ai.dims()[i] > 1 ? ((ind - ai.indices_boundary().start()) / ai.strides()[i]) % ai.dims()[i] : 0);
            ++d_sub;
        }
    }

    template <typename StorageInfo>
    inline constexpr std::ostream& operator<<(std::ostream& os, const arrnd_info<StorageInfo>& ai)
    {
        if (empty(ai)) {
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

        os << "total: " << total(ai) << '\n';
        os << "dims: ";
        print_vec(ai.dims());
        os << '\n';
        os << "strides: ";
        print_vec(ai.strides());
        os << '\n';
        os << "indices_boundary: " << ai.indices_boundary() << "\n";
        os << "hints (transposed|sliced|continuous): "
           << std::bitset<to_underlying(arrnd_hint::bitscount)>(to_underlying(ai.hints())) << "\n";
        os << "props (vector|matrix|row|column|scalar): " << isvector(ai) << ismatrix(ai) << isrow(ai) << iscolumn(ai)
           << isscalar(ai);

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
            const info_type& ai, arrnd_iterator_position start_pos = arrnd_iterator_position::begin)
            : ai_(ai)
            , subs_(size(ai))
            , pos_(empty(ai) ? arrnd_iterator_position::end : start_pos)
        {
            if (!empty(ai)) {
                if (start_pos == arrnd_iterator_position::begin || start_pos == arrnd_iterator_position::rend) {
                    std::fill(std::begin(subs_), std::end(subs_), index_type{0});
                    curr_abs_ind_ = ai.indices_boundary().start();
                    curr_rel_ind_ = index_type{0};
                } else {
                    std::transform(std::begin(ai.dims()), std::end(ai.dims()), std::begin(subs_), [](auto dim) {
                        return dim - index_type{1};
                    });
                    curr_abs_ind_ = ai.indices_boundary().stop() - index_type{1};
                    curr_rel_ind_ = total(ai) - index_type{1};
                }
                if (start_pos == arrnd_iterator_position::end) {
                    curr_rel_ind_ = total(ai);
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

            for (index_type i = size(ai_); i > 0; --i) {
                ++subs_[i - 1];
                curr_abs_ind_ += ai_.strides()[i - 1];
                if (subs_[i - 1] < ai_.dims()[i - 1]) {
                    return *this;
                }
                curr_abs_ind_ -= ai_.strides()[i - 1] * subs_[i - 1];
                subs_[i - 1] = 0;
            }

            std::transform(std::begin(ai_.dims()), std::end(ai_.dims()), std::begin(subs_), [](auto dim) {
                return dim - index_type{1};
            });
            curr_abs_ind_ = ai_.indices_boundary().stop() - index_type{1};
            curr_rel_ind_ = total(ai_);
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

            for (index_type i = size(ai_); i > 0; --i) {
                if (subs_[i - 1] >= 1) {
                    --subs_[i - 1];
                    curr_abs_ind_ -= ai_.strides()[i - 1];
                    return *this;
                }
                subs_[i - 1] = ai_.dims()[i - 1] - index_type{1};
                curr_abs_ind_ += ai_.strides()[i - 1] * subs_[i - 1];
            }

            std::fill(std::begin(subs_), std::end(subs_), index_type{0});
            curr_abs_ind_ = ai_.indices_boundary().start();
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
            assert(index >= 0 && index < total(ai_));

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
        info_type ai_;

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

        using boundary_storage_type = typename info_type::storage_info_type::template replaced_type<
            typename info_type::boundary_type>::storage_type;

        using window_type = arrnd_window<interval<std::make_signed_t<typename info_type::extent_type>>>;

        template <typename InputIt>
            requires(template_type<iterator_value_t<InputIt>, arrnd_window>)
        explicit constexpr arrnd_windows_slider(const info_type& ai, InputIt first_window, InputIt last_window,
            arrnd_iterator_position start_pos = arrnd_iterator_position::begin)
            : ai_(ai)
            , windows_(size(ai))
            , curr_boundaries_(size(ai))
        {
            if (size(ai) < std::distance(first_window, last_window)) {
                throw std::invalid_argument("invalid window size - bigger than number of dims");
            }

            if (std::any_of(first_window, last_window, [](auto window) {
                    return isunbound(window.ival) && (isleftbound(window.ival) || isrightbound(window.ival));
                })) {
                throw std::invalid_argument("invalid window interval - half bounded intervals currently not supported");
            }

            if (!std::transform_reduce(std::begin(ai.dims()),
                    std::next(std::begin(ai.dims()), std::distance(first_window, last_window)), first_window, true,
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
            if (size(ai) > std::distance(first_window, last_window)) {
                std::transform(std::next(std::begin(ai.dims()), std::distance(first_window, last_window)),
                    std::end(ai.dims()), std::next(std::begin(windows_), std::distance(first_window, last_window)),
                    [](auto dim) {
                        return window_type(typename window_type::interval_type(0, static_cast<size_type>(dim)),
                            arrnd_window_type::complete);
                    });
            }

            typename info_type::extent_storage_type indexer_dims(size(ai));
            std::transform(std::begin(ai.dims()), std::end(ai.dims()), std::begin(windows_), std::begin(indexer_dims),
                [](auto dim, auto window) {
                    auto ival = window.ival;
                    if (isunbound(window.ival)) {
                        return index_type{1};
                    }
                    return window.type == arrnd_window_type::complete ? dim - absdiff(ival) + 1 : dim;
                });

            indexer_ = arrnd_indexer(info_type(indexer_dims), start_pos);

            for (size_type i = 0; i < size(ai); ++i) {
                curr_boundaries_[i] = window2boundary(windows_[i], indexer_.subs()[i], ai.dims()[i]);
            }
        }

        template <iterable_type Cont>
            requires(template_type<iterable_value_t<Cont>, arrnd_window>)
        explicit constexpr arrnd_windows_slider(
            const info_type& ai, Cont&& windows, arrnd_iterator_position start_pos = arrnd_iterator_position::begin)
            : arrnd_windows_slider(ai, std::begin(windows), std::end(windows), start_pos)
        { }

        explicit constexpr arrnd_windows_slider(const info_type& ai, std::initializer_list<window_type> windows,
            arrnd_iterator_position start_pos = arrnd_iterator_position::begin)
            : arrnd_windows_slider(ai, windows.begin(), windows.end(), start_pos)
        { }

        explicit constexpr arrnd_windows_slider(const info_type& ai, index_type axis,
            const window_type& window = window_type(typename window_type::interval_type(0, 1)),
            arrnd_iterator_position start_pos = arrnd_iterator_position::begin)
        {
            if (axis >= size(ai)) {
                throw std::out_of_range("invalid axis");
            }

            typename info_type::storage_info_type::template replaced_type<window_type>::storage_type windows(size(ai));

            std::transform(std::begin(ai.dims()), std::end(ai.dims()), std::begin(windows), [](auto dim) {
                return window_type(
                    typename window_type::interval_type(0, static_cast<size_type>(dim)), arrnd_window_type::complete);
            });

            windows[axis] = window;

            *this = arrnd_windows_slider(ai, windows, start_pos);
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

            for (size_type i = 0; i < size(ai_); ++i) {
                curr_boundaries_[i] = window2boundary(windows_[i], indexer_.subs()[i], ai_.dims()[i]);
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

            for (size_type i = 0; i < size(ai_); ++i) {
                curr_boundaries_[i] = window2boundary(windows_[i], indexer_.subs()[i], ai_.dims()[i]);
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

            for (size_type i = 0; i < size(ai_); ++i) {
                temp.curr_boundaries_[i] = window2boundary(windows_[i], subs[i], ai_.dims()[i]);
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
            if (axis >= size(ai_)) {
                throw std::out_of_range("invalid axis");
            }

            const auto& ival = window.ival;

            if (isunbound(ival) && (isleftbound(ival) || isrightbound(ival))) {
                throw std::invalid_argument("invalid window interval - half bounded intervals currently not supported");
            }

            if (!(isunbound(ival) || (ival.start() <= 0 && ival.stop() >= 0 && absdiff(ival) <= ai_.dims()[axis]))) {
                throw std::invalid_argument("invalid window interval");
            }

            windows_[axis] = window;

            curr_boundaries_[axis] = window2boundary(windows_[axis], indexer_.subs()[axis], ai_.dims()[axis]);
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

        info_type ai_;

        typename info_type::storage_info_type::template replaced_type<window_type>::storage_type windows_;
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
        using returned_type_category = arrnd_returned_element_iterator_tag;

        friend class arrnd_const_iterator<Arrnd>;

        explicit constexpr arrnd_iterator(pointer data, const indexer_type& gen)
            : gen_(gen)
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
            ++gen_;
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
            gen_ += count;
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
            --gen_;
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
            gen_ -= count;
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
            return data_[*gen_];
        }

        [[nodiscard]] constexpr bool operator==(const arrnd_iterator& iter) const noexcept
        {
            return gen_ == iter.gen_;
        }

        [[nodiscard]] constexpr bool operator<(const arrnd_iterator& iter) const noexcept
        {
            return gen_ < iter.gen_;
        }

        [[nodiscard]] constexpr bool operator<=(const arrnd_iterator& iter) const noexcept
        {
            return gen_ <= iter.gen_;
        }

        [[nodiscard]] constexpr reference operator[](difference_type index) const noexcept
        {
            return data_[gen_[index]];
        }

        [[nodiscard]] constexpr difference_type operator-(const arrnd_iterator& other) const noexcept
        {
            return gen_ - other.gen_;
        }

    private:
        indexer_type gen_;
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
        using returned_type_category = arrnd_returned_element_iterator_tag;

        explicit constexpr arrnd_const_iterator(pointer data, const indexer_type& gen)
            : gen_(gen)
            , data_(data)
        { }

        constexpr arrnd_const_iterator() = default;

        constexpr arrnd_const_iterator(const arrnd_const_iterator& other) = default;
        constexpr arrnd_const_iterator& operator=(const arrnd_const_iterator& other) = default;

        constexpr arrnd_const_iterator(arrnd_const_iterator&& other) noexcept = default;
        constexpr arrnd_const_iterator& operator=(arrnd_const_iterator&& other) noexcept = default;

        constexpr ~arrnd_const_iterator() = default;

        constexpr arrnd_const_iterator(const arrnd_iterator<Arrnd>& other)
            : gen_(other.gen_)
            , data_(other.data_)
        { }
        constexpr arrnd_const_iterator& operator=(const arrnd_iterator<Arrnd>& other)
        {
            gen_ = other.gen_;
            data_ = other.data_;
            return *this;
        }

        constexpr arrnd_const_iterator(arrnd_iterator<Arrnd>&& other)
            : gen_(std::move(other.gen_))
            , data_(std::move(other.data_))
        { }
        constexpr arrnd_const_iterator& operator=(arrnd_iterator<Arrnd>&& other)
        {
            gen_ = std::move(other.gen_);
            data_ = std::move(other.data_);
            return *this;
        }

        constexpr arrnd_const_iterator& operator++() noexcept
        {
            ++gen_;
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
            gen_ += count;
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
            --gen_;
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
            gen_ -= count;
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
            return data_[*gen_];
        }

        [[nodiscard]] constexpr bool operator==(const arrnd_const_iterator& iter) const noexcept
        {
            return gen_ == iter.gen_;
        }

        [[nodiscard]] constexpr bool operator<(const arrnd_const_iterator& iter) const noexcept
        {
            return gen_ < iter.gen_;
        }

        [[nodiscard]] constexpr bool operator<=(const arrnd_const_iterator& iter) const noexcept
        {
            return gen_ <= iter.gen_;
        }

        [[nodiscard]] constexpr const reference operator[](difference_type index) const noexcept
        {
            return data_[gen_[index]];
        }

        [[nodiscard]] constexpr difference_type operator-(const arrnd_const_iterator& other) const noexcept
        {
            return gen_ - other.gen_;
        }

    private:
        indexer_type gen_;
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
        using returned_type_category = arrnd_returned_element_iterator_tag;

        friend arrnd_const_reverse_iterator<Arrnd>;

        explicit constexpr arrnd_reverse_iterator(pointer data, const indexer_type& gen)
            : gen_(gen)
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
            --gen_;
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
            gen_ -= count;
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
            ++gen_;
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
            gen_ += count;
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
            return data_[*gen_];
        }

        [[nodiscard]] constexpr bool operator==(const arrnd_reverse_iterator& iter) const noexcept
        {
            return gen_ == iter.gen_;
        }

        [[nodiscard]] constexpr bool operator<(const arrnd_reverse_iterator& iter) const noexcept
        {
            return iter.gen_ < gen_;
        }

        [[nodiscard]] constexpr bool operator<=(const arrnd_reverse_iterator& iter) const noexcept
        {
            return iter.gen_ <= gen_;
        }

        [[nodiscard]] constexpr reference operator[](difference_type index) const noexcept
        {
            return data_[gen_[index]];
        }

        [[nodiscard]] constexpr difference_type operator-(const arrnd_reverse_iterator& other) const noexcept
        {
            return other.gen_ - gen_;
        }

    private:
        indexer_type gen_;
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
        using returned_type_category = arrnd_returned_element_iterator_tag;

        explicit constexpr arrnd_const_reverse_iterator(pointer data, const indexer_type& gen)
            : gen_(gen)
            , data_(data)
        { }

        constexpr arrnd_const_reverse_iterator() = default;

        constexpr arrnd_const_reverse_iterator(const arrnd_const_reverse_iterator& other) = default;
        constexpr arrnd_const_reverse_iterator& operator=(const arrnd_const_reverse_iterator& other) = default;

        constexpr arrnd_const_reverse_iterator(arrnd_const_reverse_iterator&& other) noexcept = default;
        constexpr arrnd_const_reverse_iterator& operator=(arrnd_const_reverse_iterator&& other) noexcept = default;

        constexpr ~arrnd_const_reverse_iterator() = default;

        constexpr arrnd_const_reverse_iterator(const arrnd_reverse_iterator<Arrnd>& other)
            : gen_(other.gen_)
            , data_(other.data_)
        { }
        constexpr arrnd_const_reverse_iterator& operator=(const arrnd_reverse_iterator<Arrnd>& other)
        {
            gen_ = other.gen_;
            data_ = other.data_;
            return *this;
        }

        constexpr arrnd_const_reverse_iterator(arrnd_reverse_iterator<Arrnd>&& other)
            : gen_(std::move(other.gen_))
            , data_(std::move(other.data_))
        { }
        constexpr arrnd_const_reverse_iterator& operator=(arrnd_reverse_iterator<Arrnd>&& other)
        {
            gen_ = std::move(other.gen_);
            data_ = std::move(other.data_);
            return *this;
        }

        constexpr arrnd_const_reverse_iterator& operator++() noexcept
        {
            --gen_;
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
            gen_ -= count;
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
            ++gen_;
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
            gen_ += count;
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
            return data_[*gen_];
        }

        [[nodiscard]] constexpr bool operator==(const arrnd_const_reverse_iterator& iter) const noexcept
        {
            return gen_ == iter.gen_;
        }

        [[nodiscard]] constexpr bool operator<(const arrnd_const_reverse_iterator& iter) const noexcept
        {
            return iter.gen_ < gen_;
        }

        [[nodiscard]] constexpr bool operator<=(const arrnd_const_reverse_iterator& iter) const noexcept
        {
            return iter.gen_ <= gen_;
        }

        [[nodiscard]] constexpr const reference operator[](difference_type index) const noexcept
        {
            return data_[gen_[index]];
        }

        [[nodiscard]] constexpr difference_type operator-(const arrnd_const_reverse_iterator& other) const noexcept
        {
            return other.gen_ - gen_;
        }

    private:
        indexer_type gen_;
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

        using ranger_type = typename Arrnd::ranger_type;
        using returned_type_category = arrnd_returned_slice_iterator_tag;

        friend arrnd_slice_const_iterator<Arrnd>;

        explicit constexpr arrnd_slice_iterator(const value_type& arrnd_ref, const ranger_type& far)
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
        ranger_type far_;

        mutable value_type slice_;
    };

    template <typename Arrnd>
    class arrnd_slice_const_iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = typename Arrnd::difference_type;
        using value_type = Arrnd;
        using const_reference = const Arrnd&;

        using ranger_type = typename Arrnd::ranger_type;
        using returned_type_category = arrnd_returned_slice_iterator_tag;

        explicit constexpr arrnd_slice_const_iterator(const value_type& arrnd_ref, const ranger_type& far)
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
        ranger_type far_;

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

        using ranger_type = typename Arrnd::ranger_type;
        using returned_type_category = arrnd_returned_slice_iterator_tag;

        friend arrnd_slice_reverse_const_iterator<Arrnd>;

        explicit constexpr arrnd_slice_reverse_iterator(const value_type& arrnd_ref, const ranger_type& far)
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
        ranger_type far_;

        mutable value_type slice_;
    };

    template <typename Arrnd>
    class arrnd_slice_reverse_const_iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = typename Arrnd::difference_type;
        using value_type = Arrnd;
        using const_reference = const Arrnd&;

        using ranger_type = typename Arrnd::ranger_type;
        using returned_type_category = arrnd_returned_slice_iterator_tag;

        explicit constexpr arrnd_slice_reverse_const_iterator(const value_type& arrnd_ref, const ranger_type& far)
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
        ranger_type far_;

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
            *cont_ = cont_->push_back(cont);
            return *this;
        }

        arrnd_back_insert_iterator& operator=(Arrnd&& cont)
        {
            *cont_ = cont_->push_back(std::move(cont));
            return *this;
        }

        arrnd_back_insert_iterator& operator=(const typename Arrnd::value_type& value)
        {
            *cont_ = cont_->push_back(Arrnd({1}, value));
            return *this;
        }

        arrnd_back_insert_iterator& operator=(typename Arrnd::value_type&& value)
        {
            *cont_ = cont_->push_back(std::move(Arrnd({1}, value)));
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
            *cont_ = cont_->push_front(cont);
            return *this;
        }

        arrnd_front_insert_iterator& operator=(Arrnd&& cont)
        {
            *cont_ = cont_->push_front(std::move(cont));
            return *this;
        }

        arrnd_front_insert_iterator& operator=(const typename Arrnd::value_type& value)
        {
            *cont_ = cont_->push_front(Arrnd({1}, value));
            return *this;
        }

        arrnd_front_insert_iterator& operator=(typename Arrnd::value_type&& value)
        {
            *cont_ = cont_->push_front(std::move(Arrnd({1}, value)));
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
            *cont_ = cont_->insert(cont, ind_);
            ind_ += cont.header().numel();
            return *this;
        }

        arrnd_insert_iterator& operator=(Arrnd&& cont)
        {
            *cont_ = cont_->insert(std::move(cont), ind_);
            ind_ += cont.header().numel();
            return *this;
        }

        arrnd_insert_iterator& operator=(const typename Arrnd::value_type& value)
        {
            *cont_ = cont_->insert(Arrnd({1}, value), ind_);
            ind_ += 1;
            return *this;
        }

        arrnd_insert_iterator& operator=(typename Arrnd::value_type&& value)
        {
            *cont_ = cont_->insert(std::move(Arrnd({1}, value)), ind_);
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
            *cont_ = cont_->push_back(cont, axis_);
            return *this;
        }

        arrnd_slice_back_insert_iterator& operator=(Arrnd&& cont)
        {
            *cont_ = cont_->push_back(std::move(cont), axis_);
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
            *cont_ = cont_->insert(cont, 0, axis_);
            return *this;
        }

        arrnd_slice_front_insert_iterator& operator=(Arrnd&& cont)
        {
            *cont_ = cont_->insert(std::move(cont), 0, axis_);
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
            *cont_ = cont_->insert(cont, ind_, axis_);
            ind_ += cont.header().dims()[axis_];
            return *this;
        }

        arrnd_slice_insert_iterator& operator=(Arrnd&& cont)
        {
            *cont_ = cont_->insert(std::move(cont), ind_, axis_);
            ind_ += cont.header().dims()[axis_];
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

    template <typename T, typename U>
    concept same_depth = (T::depth == U::depth);

    template <typename T, typename... Args>
    concept invocable_no_arrnd = !
    arrnd_type<T>&& std::is_invocable_v<T, Args...>;

    template <arrnd_type T>
    [[nodiscard]] inline constexpr std::int64_t calc_arrnd_depth()
    {
        return T::depth + 1;
    }
    template <typename T>
    [[nodiscard]] inline constexpr std::int64_t calc_arrnd_depth()
    {
        return 0;
    }

    template <typename T>
    concept flat_arrnd_type = arrnd_type<T> && T::is_flat;

    template <typename T, typename U>
    [[nodiscard]] inline constexpr bool is_arrnd_of_type()
    {
        return std::is_same_v<T, U>;
    }
    template <arrnd_type T, typename U>
    [[nodiscard]] inline constexpr bool is_arrnd_of_type()
    {
        return is_arrnd_of_type<typename T::value_type, U>();
    }

    template <typename T, typename U>
    concept arrnd_of_type = arrnd_type<T> && is_arrnd_of_type<T, U>();

    template <typename T, template <typename...> typename U>
    [[nodiscard]] inline constexpr bool is_arrnd_of_template_type()
    {
        return is_template_type_v<T, U>;
    }
    template <arrnd_type T, template <typename...> typename U>
    [[nodiscard]] inline constexpr bool is_arrnd_of_template_type()
    {
        return is_arrnd_of_template_type<typename T::value_type, U>();
    }

    template <typename T, template <typename...> typename U>
    concept arrnd_of_template_type = arrnd_type<T> && is_arrnd_of_template_type<T, U>();

    template <typename T, template <typename> typename Trait>
    [[nodiscard]] inline constexpr bool is_arrnd_with_trait()
    {
        return Trait<T>::value;
    }
    template <arrnd_type T, template <typename> typename Trait>
    [[nodiscard]] inline constexpr bool is_arrnd_with_trait()
    {
        return is_arrnd_with_trait<typename T::value_type, Trait>();
    }

    template <typename T, template <typename> typename Trait>
    concept arrnd_with_trait = arrnd_type<T> && is_arrnd_with_trait<T, Trait>();

    template <typename ArrndSrc, typename ArrndDst>
    concept arrnd_depths_match = arrnd_type<ArrndSrc> && arrnd_type<ArrndDst> && same_depth<ArrndSrc, ArrndDst>;

    template <typename T, std::int64_t Depth>
        requires(Depth >= 0 && Depth <= T::depth)
    struct arrnd_inner_impl {
        using type = arrnd_inner_impl<typename T::value_type, Depth - 1>::type;
    };
    template <typename T>
    struct arrnd_inner_impl<T, 0> {
        using type = T;
    };
    template <typename Arrnd, std::int64_t Level = Arrnd::depth>
        requires std::is_same_v<typename Arrnd::tag, arrnd_tag>
    using arrnd_inner = arrnd_inner_impl<Arrnd, Level>;
    template <typename Arrnd, std::int64_t Level = Arrnd::depth>
        requires std::is_same_v<typename Arrnd::tag, arrnd_tag>
    using arrnd_inner_t = arrnd_inner<Arrnd, Level>::type;

    template <typename Arrnd, std::int64_t Depth>
        requires(Depth >= 0)
    struct arrnd_nested {
        using type = typename Arrnd::template replaced_type<typename arrnd_nested<Arrnd, Depth - 1>::type>;
    };
    template <typename Arrnd>
    struct arrnd_nested<Arrnd, 0> {
        using type = Arrnd;
    };

    template <typename T>
    struct typed {
        using type = T;
    };

    template <typename T, typename R, std::int64_t Level>
    struct last_inner_replaced_types_tuple_impl {
        using type
            = std::tuple<T, typename last_inner_replaced_types_tuple_impl<typename T::value_type, R, Level - 1>::type>;
    };
    template <typename T, typename R>
    struct last_inner_replaced_types_tuple_impl<T, R, 0> {
        using type = typename T::template replaced_type<R>;
    };
    template <typename T, typename R, std::int64_t Level>
    using last_inner_replaced_types_tuple = std::conditional_t<Level == 0,
        typed<std::tuple<typename T::template replaced_type<R>>>, last_inner_replaced_types_tuple_impl<T, R, Level>>;
    template <typename T, typename R, std::int64_t Level>
    using last_inner_replaced_types_tuple_t = typename last_inner_replaced_types_tuple<T, R, Level>::type;

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

    template <typename Tuple, std::int64_t Index>
    struct folded_replaced_type_tuple {
        static constexpr std::size_t tsi = std::tuple_size_v<Tuple> - 1;
        using type = typename std::tuple_element_t<tsi - Index,
            Tuple>::template replaced_type<typename folded_replaced_type_tuple<Tuple, Index - 1>::type>;
    };
    template <typename Tuple>
    struct folded_replaced_type_tuple<Tuple, 1> {
        static constexpr std::size_t tsi = std::tuple_size_v<Tuple> - 1;
        using type =
            typename std::tuple_element_t<tsi - 1, Tuple>::template replaced_type<std::tuple_element_t<tsi - 0, Tuple>>;
    };
    template <typename Tuple>
    struct folded_replaced_type_tuple<Tuple, 0> {
        static constexpr int tsi = std::tuple_size_v<Tuple> - 1;
        using type = std::tuple_element_t<tsi - 0, Tuple>;
    };
    template <typename Tuple>
    using folded_replaced_type_tuple_t = folded_replaced_type_tuple<Tuple, std::tuple_size_v<Tuple> - 1>::type;

    template <typename T, typename R, std::int64_t Level>
    struct inner_replaced_type {
        using type = folded_replaced_type_tuple_t<flat_tuple_t<last_inner_replaced_types_tuple_t<T, R, Level>>>;
    };
    template <typename T, typename R, std::int64_t Level>
    using inner_replaced_type_t = inner_replaced_type<T, R, Level>::type;

    enum class arrnd_shape_preset { vector, row, column };

    template <arrnd_type Arrnd, typename Constraint>
    class arrnd_filter_proxy {
    public:
        constexpr arrnd_filter_proxy() = delete;

        explicit constexpr arrnd_filter_proxy(Arrnd arr_ref, Constraint constraint)
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

        constexpr arrnd_filter_proxy(arrnd_filter_proxy&& other) = default;
        template <arrnd_type OtherArrnd, typename OtherConstraint>
        constexpr arrnd_filter_proxy& operator=(arrnd_filter_proxy<OtherArrnd, OtherConstraint>&& other)
        {
            // copy array elements from one proxy to another according to mask

            if (arr_ref_.empty()) {
                return *this;
            }

            // the user is responsible that the number of elements in both constraints is the same

            auto other_filtered = static_cast<OtherArrnd>(other);

            other_filtered.copy_to(arr_ref_, constraint_);

            (void)arrnd_filter_proxy<OtherArrnd, OtherConstraint>(std::move(other));

            return *this;
        }

        constexpr arrnd_filter_proxy(const arrnd_filter_proxy& other) = delete;
        constexpr arrnd_filter_proxy& operator=(const arrnd_filter_proxy& other) = delete;

        template <arrnd_type Arrnd>
        constexpr arrnd_filter_proxy& operator=(const Arrnd& other) &&
        {
            other.copy_to(arr_ref_, constraint_);
            return *this;
        }

        template <arrnd_type Arrnd>
        constexpr arrnd_filter_proxy& operator+=(const Arrnd& other) &&
        {
            (arr_ref_.filter(constraint_) + other).copy_to(arr_ref_, constraint_);
            return *this;
        }

        template <arrnd_type Arrnd>
        constexpr arrnd_filter_proxy& operator-=(const Arrnd& other) &&
        {
            (arr_ref_.filter(constraint_) - other).copy_to(arr_ref_, constraint_);
            return *this;
        }

        template <arrnd_type Arrnd>
        constexpr arrnd_filter_proxy& operator*=(const Arrnd& other) &&
        {
            (arr_ref_.filter(constraint_) * other).copy_to(arr_ref_, constraint_);
            return *this;
        }

        template <arrnd_type Arrnd>
        constexpr arrnd_filter_proxy& operator/=(const Arrnd& other) &&
        {
            (arr_ref_.filter(constraint_) / other).copy_to(arr_ref_, constraint_);
            return *this;
        }

        template <arrnd_type Arrnd>
        constexpr arrnd_filter_proxy& operator%=(const Arrnd& other) &&
        {
            (arr_ref_.filter(constraint_) % other).copy_to(arr_ref_, constraint_);
            return *this;
        }

        template <arrnd_type Arrnd>
        constexpr arrnd_filter_proxy& operator^=(const Arrnd& other) &&
        {
            (arr_ref_.filter(constraint_) ^ other).copy_to(arr_ref_, constraint_);
            return *this;
        }

        template <arrnd_type Arrnd>
        constexpr arrnd_filter_proxy& operator&=(const Arrnd& other) &&
        {
            (arr_ref_.filter(constraint_) & other).copy_to(arr_ref_, constraint_);
            return *this;
        }

        template <arrnd_type Arrnd>
        constexpr arrnd_filter_proxy& operator|=(const Arrnd& other) &&
        {
            (arr_ref_.filter(constraint_) | other).copy_to(arr_ref_, constraint_);
            return *this;
        }

        template <arrnd_type Arrnd>
        constexpr arrnd_filter_proxy& operator<<=(const Arrnd& other) &&
        {
            (arr_ref_.filter(constraint_) << other).copy_to(arr_ref_, constraint_);
            return *this;
        }

        template <arrnd_type Arrnd>
        constexpr arrnd_filter_proxy& operator>>=(const Arrnd& other) &&
        {
            (arr_ref_.filter(constraint_) >> other).copy_to(arr_ref_, constraint_);
            return *this;
        }

        template <typename U>
            requires(!arrnd_type<U>)
        constexpr arrnd_filter_proxy& operator+=(const U& value) &&
        {
            *this = arrnd_filter_proxy(arr_ref_ + value, constraint_);
            return *this;
        }

        template <typename U>
            requires(!arrnd_type<U>)
        constexpr arrnd_filter_proxy& operator-=(const U& value) &&
        {
            *this = arrnd_filter_proxy(arr_ref_ - value, constraint_);
            return *this;
        }

        template <typename U>
            requires(!arrnd_type<U>)
        constexpr arrnd_filter_proxy& operator*=(const U& value) &&
        {
            *this = arrnd_filter_proxy(arr_ref_ * value, constraint_);
            return *this;
        }

        template <typename U>
            requires(!arrnd_type<U>)
        constexpr arrnd_filter_proxy& operator/=(const U& value) &&
        {
            *this = arrnd_filter_proxy(arr_ref_ / value, constraint_);
            return *this;
        }

        template <typename U>
            requires(!arrnd_type<U>)
        constexpr arrnd_filter_proxy& operator%=(const U& value) &&
        {
            *this = arrnd_filter_proxy(arr_ref_ % value, constraint_);
            return *this;
        }

        template <typename U>
            requires(!arrnd_type<U>)
        constexpr arrnd_filter_proxy& operator^=(const U& value) &&
        {
            *this = arrnd_filter_proxy(arr_ref_ ^ value, constraint_);
            return *this;
        }

        template <typename U>
            requires(!arrnd_type<U>)
        constexpr arrnd_filter_proxy& operator&=(const U& value) &&
        {
            *this = arrnd_filter_proxy(arr_ref_ & value, constraint_);
            return *this;
        }

        template <typename U>
            requires(!arrnd_type<U>)
        constexpr arrnd_filter_proxy& operator|=(const U& value) &&
        {
            *this = arrnd_filter_proxy(arr_ref_ | value, constraint_);
            return *this;
        }

        template <typename U>
            requires(!arrnd_type<U>)
        constexpr arrnd_filter_proxy& operator<<=(const U& value) &&
        {
            *this = arrnd_filter_proxy(arr_ref_ << value, constraint_);
            return *this;
        }

        template <typename U>
            requires(!arrnd_type<U>)
        constexpr arrnd_filter_proxy& operator>>=(const U& value) &&
        {
            *this = arrnd_filter_proxy(arr_ref_ >> value, constraint_);
            return *this;
        }

        template <arrnd_type Arrnd>
        constexpr arrnd_filter_proxy& operator++() &&
        {
            auto c = arr_ref_.clone();
            *this = arrnd_filter_proxy(++c, constraint_);
            return *this;
        }

        template <arrnd_type Arrnd>
        constexpr arrnd_filter_proxy& operator--() &&
        {
            auto c = arr_ref_.clone();
            *this = arrnd_filter_proxy(--c, constraint_);
            return *this;
        }

        template <typename U>
            requires(!arrnd_type<U>)
        constexpr arrnd_filter_proxy& operator=(const U& value) &&
        {
            if (arr_ref_.empty()) {
                return *this;
            }

            if constexpr (arrnd_type<Constraint>) {
                if constexpr (std::is_same_v<typename Constraint::value_type, bool>) {
                    assert(std::equal(std::begin(constraint_.header().dims()), std::end(constraint_.header().dims()),
                               std::begin(arr_ref_.header().dims()), std::end(arr_ref_.header().dims()))
                        && "boolean constraint considered as mask");

                    typename Arrnd::indexer_type gen(arr_ref_.header());
                    typename Constraint::indexer_type cgen(constraint_.header());

                    for (; gen && cgen; ++gen, ++cgen) {
                        if (constraint_[*cgen]) {
                            arr_ref_[*gen] = value;
                        }
                    }

                } else {
                    for (typename Constraint::indexer_type gen(constraint_.header()); gen; ++gen) {
                        arr_ref_[constraint_[*gen]] = value;
                    }
                }
            } else { // might be predicator type
                typename Arrnd::indexer_type gen(arr_ref_.header());

                for (; gen; ++gen) {
                    if (constraint_(arr_ref_[*gen])) {
                        arr_ref_[*gen] = value;
                    }
                }
            }

            return *this;
        }

        virtual constexpr ~arrnd_filter_proxy() = default;

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
        assert(ndims >= 2);

        auto eye_impl = [](typename Arrnd::size_type r, typename Arrnd::size_type c) {
            if (r == 0 || c == 0) {
                return Arrnd();
            }
            Arrnd res({r, c}, typename Arrnd::value_type{0});
            assert(ismatrix(res.header()));

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
            return eye_impl(page.header().dims().front(), page.header().dims().back());
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

    template <typename T, typename DataStorageInfo = simple_vector_traits<T>,
        typename DimsStorageInfo = simple_vector_traits<std::size_t>>
    class arrnd {
    public:
        using value_type = T;
        using size_type = typename DataStorageInfo::storage_type::size_type;
        using difference_type = typename DataStorageInfo::storage_type::difference_type;
        using reference = T&;
        using const_reference = const T&;
        using pointer = T*;
        using const_pointer = const T*;

        using tag = arrnd_tag;

        using data_storage_info = DataStorageInfo;
        using dims_storage_info = DimsStorageInfo;

        using storage_type = typename DataStorageInfo::storage_type;
        template <typename U>
        using shared_ref_allocator_type = typename data_storage_info::template allocator_type<U>;
        using header_type = arrnd_info<DimsStorageInfo>;
        using indexer_type = arrnd_indexer<header_type>;
        using ranger_type = arrnd_windows_slider<header_type>;

        using interval_type = typename header_type::boundary_type;

        using window_type = typename ranger_type::window_type;
        using window_interval_type = typename window_type::interval_type;

        using this_type = arrnd<T, DataStorageInfo, DimsStorageInfo>;
        template <typename U>
        using replaced_type = arrnd<U, typename DataStorageInfo::template replaced_type<U>, DimsStorageInfo>;

        template <typename U, std::int64_t Level>
        using inner_replaced_type = inner_replaced_type_t<this_type, U, Level>;
        template <std::int64_t Level>
        using inner_this_type = arrnd_inner_t<this_type, Level>;
        template <std::int64_t Level>
        using inner_value_type = typename inner_this_type<Level>::value_type;

        template <typename U>
        using shared_ref = U;
        template <typename U>
        using maybe_shared_ref = U;

        using iterator = arrnd_iterator<this_type>;
        using const_iterator = arrnd_const_iterator<this_type>;
        using reverse_iterator = arrnd_reverse_iterator<this_type>;
        using const_reverse_iterator = arrnd_const_reverse_iterator<this_type>;

        using slice_iterator = arrnd_slice_iterator<this_type>;
        using const_slice_iterator = arrnd_slice_const_iterator<this_type>;
        using reverse_slice_iterator = arrnd_slice_reverse_iterator<this_type>;
        using const_reverse_slice_iterator = arrnd_slice_reverse_const_iterator<this_type>;

        constexpr static std::int64_t depth = calc_arrnd_depth<T>();
        constexpr static bool is_flat = depth == 0;

        template <std::int64_t Depth>
        using nested = arrnd_nested<this_type, Depth>;
        template <std::int64_t Depth>
        using nested_t = nested<Depth>::type;

        template <typename U, std::int64_t Level = this_type::depth>
        using tol_type = decltype(inner_value_type<Level>{} - U{});
        template <arrnd_type Arrnd, std::int64_t Level = this_type::depth>
        using compliant_tol_type
            = decltype(inner_value_type<Level>{} - typename Arrnd::template inner_value_type<Level>{});

        constexpr arrnd() = default;

        constexpr arrnd(arrnd&& other) = default;
        template <arrnd_type Arrnd>
            requires arrnd_depths_match<arrnd, Arrnd>
        constexpr arrnd(Arrnd&& other)
        {
            other.set_to(*this);
            (void)Arrnd(std::move(other));
        }
        constexpr arrnd& operator=(arrnd&& other) & = default;
        constexpr arrnd& operator=(arrnd&& other) &&
        {
            if (&other == this) {
                return *this;
            }

            other.copy_to(*this);
            (void)arrnd(std::move(other));
            return *this;
        }
        template <arrnd_type Arrnd>
            requires arrnd_depths_match<arrnd, Arrnd>
        constexpr arrnd& operator=(Arrnd&& other) &
        {
            other.set_to(*this);
            (void)Arrnd(std::move(other));
            return *this;
        }
        template <arrnd_type Arrnd>
            requires arrnd_depths_match<arrnd, Arrnd>
        constexpr arrnd& operator=(Arrnd&& other) &&
        {
            other.copy_to(*this);
            (void)Arrnd(std::move(other));
            return *this;
        }

        constexpr arrnd(const arrnd& other) = default;
        template <arrnd_type Arrnd>
            requires arrnd_depths_match<arrnd, Arrnd>
        constexpr arrnd(const Arrnd& other)
        {
            other.set_to(*this);
        }
        constexpr arrnd& operator=(const arrnd& other) & = default;
        constexpr arrnd& operator=(const arrnd& other) &&
        {
            if (&other == this) {
                return *this;
            }

            other.copy_to(*this);
            return *this;
        }
        template <arrnd_type Arrnd>
            requires arrnd_depths_match<arrnd, Arrnd>
        constexpr arrnd& operator=(const Arrnd& other) &
        {
            other.set_to(*this);
            return *this;
        }
        template <arrnd_type Arrnd>
            requires arrnd_depths_match<arrnd, Arrnd>
        constexpr arrnd& operator=(const Arrnd& other) &&
        {
            other.copy_to(*this);
            return *this;
        }

        template <typename U>
            requires(!arrnd_type<U>)
        constexpr arrnd& operator=(const U& value)
        {
            if (empty()) {
                return *this;
            }

            for (indexer_type gen(hdr_); gen; ++gen) {
                (*this)[*gen] = value;
            }

            return *this;
        }

        virtual constexpr ~arrnd() = default;

        template <iterator_of_type_integral InputDimsIt, iterator_type InputDataIt>
        explicit constexpr arrnd(const InputDimsIt& first_dim, const InputDimsIt& last_dim,
            const InputDataIt& first_data, const InputDataIt& last_data)
            : hdr_(first_dim, last_dim)
            , buffsp_(oc::arrnd::empty(hdr_) ? nullptr
                                             : std::allocate_shared<storage_type>(
                                                 shared_ref_allocator_type<storage_type>(), first_data, last_data))
        {
            // in case that data buffer allocated, check the number of data elements is valid
            if (buffsp_) {
                assert(last_data - first_data == total(hdr_));
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
            : hdr_(first_dim, last_dim)
            , buffsp_(oc::arrnd::empty(hdr_)
                      ? nullptr
                      : std::allocate_shared<storage_type>(shared_ref_allocator_type<storage_type>(), total(hdr_)))
        { }
        template <iterable_of_type_integral Cont>
        explicit constexpr arrnd(const Cont& dims)
            : arrnd(std::begin(dims), std::end(dims))
        { }
        explicit constexpr arrnd(std::initializer_list<size_type> dims)
            : arrnd(dims.begin(), dims.end())
        { }

        template <iterator_of_type_integral InputDimsIt>
        explicit constexpr arrnd(const InputDimsIt& first_dim, const InputDimsIt& last_dim, const_reference value)
            : hdr_(first_dim, last_dim)
            , buffsp_(oc::arrnd::empty(hdr_)
                      ? nullptr
                      : std::allocate_shared<storage_type>(shared_ref_allocator_type<storage_type>(), total(hdr_)))
        {
            if (buffsp_) {
                std::fill(buffsp_->begin(), buffsp_->end(), value);
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
        explicit constexpr arrnd(const InputDimsIt& first_dim, const InputDimsIt& last_dim, const U& value)
            : hdr_(first_dim, last_dim)
            , buffsp_(oc::arrnd::empty(hdr_)
                      ? nullptr
                      : std::allocate_shared<storage_type>(shared_ref_allocator_type<storage_type>(), total(hdr_)))
        {
            if (buffsp_) {
                std::fill(buffsp_->begin(), buffsp_->end(), value);
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
            requires(invocable_no_arrnd<Func>)
        explicit constexpr arrnd(const InputDimsIt& first_dim, const InputDimsIt& last_dim, Func&& func)
            : hdr_(first_dim, last_dim)
            , buffsp_(oc::arrnd::empty(hdr_)
                      ? nullptr
                      : std::allocate_shared<storage_type>(shared_ref_allocator_type<storage_type>(), total(hdr_)))
        {
            if (buffsp_) {
                std::for_each(buffsp_->begin(), buffsp_->end(), [&func](auto& value) {
                    value = static_cast<value_type>(func());
                });
            }
        }
        template <iterable_of_type_integral Cont, typename Func>
            requires(invocable_no_arrnd<Func>)
        explicit constexpr arrnd(const Cont& dims, Func&& func)
            : arrnd(std::begin(dims), std::end(dims), std::forward<Func>(func))
        { }
        template <typename Func>
            requires(invocable_no_arrnd<Func>)
        explicit constexpr arrnd(std::initializer_list<size_type> dims, Func&& func)
            : arrnd(dims.begin(), dims.end(), std::forward<Func>(func))
        { }

        [[nodiscard]] constexpr const header_type& header() const noexcept
        {
            return hdr_;
        }

        [[nodiscard]] constexpr header_type& header() noexcept
        {
            return hdr_;
        }

        [[nodiscard]] constexpr const auto& storage() const noexcept
        {
            return buffsp_;
        }

        [[nodiscard]] constexpr auto& storage() noexcept
        {
            return buffsp_;
        }

        [[nodiscard]] constexpr const this_type* creator() const noexcept
        {
            return is_creator_valid_.expired() ? nullptr : creator_;
        }

        [[nodiscard]] explicit constexpr operator value_type() const noexcept
        {
            assert(isscalar(hdr_));
            return (*this)[hdr_.indices_boundary().start()];
        }

        [[nodiscard]] constexpr shared_ref<this_type> as_pages() const
        {
            auto res = *this;
            res.treat_as_pages_ = true;
            return res;
        }

        [[nodiscard]] constexpr bool is_as_pages() const noexcept
        {
            return treat_as_pages_;
        }

        [[nodiscard]] constexpr const_reference operator[](size_type index) const noexcept
        {
            assert(index >= hdr_.indices_boundary().start() && index <= hdr_.indices_boundary().stop() - 1);
            return buffsp_->data()[index];
        }
        [[nodiscard]] constexpr reference operator[](size_type index) noexcept
        {
            assert(index >= hdr_.indices_boundary().start() && index <= hdr_.indices_boundary().stop() - 1);
            return buffsp_->data()[index];
        }

        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr const_reference operator[](std::pair<InputIt, InputIt> subs) const noexcept
        {
            return buffsp_->data()[sub2ind(hdr_, subs.first, subs.second)];
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
            return buffsp_->data()[sub2ind(hdr_, subs.first, subs.second)];
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
        [[nodiscard]] constexpr shared_ref<this_type> operator[](std::pair<InputIt, InputIt> ranges) const&
        {
            this_type slice{};
            slice.hdr_ = oc::arrnd::slice(hdr_, ranges.first, ranges.second);
            slice.buffsp_ = buffsp_;
            slice.is_creator_valid_ = original_valid_creator_;
            slice.creator_ = this;
            return slice;
        }
        template <iterator_of_type_interval InputIt>
        [[nodiscard]] constexpr shared_ref<this_type> operator[](std::pair<InputIt, InputIt> ranges) const&&
        {
            this_type slice{};
            slice.hdr_ = oc::arrnd::slice(hdr_, ranges.first, ranges.second);
            slice.buffsp_ = buffsp_;
            return slice;
        }
        template <iterable_of_type_interval Cont>
        [[nodiscard]] constexpr shared_ref<this_type> operator[](const Cont& ranges) const&
        {
            return (*this)[std::make_pair(std::cbegin(ranges), std::cend(ranges))];
        }
        template <iterable_of_type_interval Cont>
        [[nodiscard]] constexpr shared_ref<this_type> operator[](const Cont& ranges) const&&
        {
            return std::move(*this)[std::make_pair(std::begin(ranges), std::end(ranges))];
        }
        [[nodiscard]] constexpr shared_ref<this_type> operator[](std::initializer_list<interval_type> ranges) const&
        {
            return (*this)[std::make_pair(ranges.begin(), ranges.end())];
        }
        [[nodiscard]] constexpr shared_ref<this_type> operator[](std::initializer_list<interval_type> ranges) const&&
        {
            return std::move(*this)[std::make_pair(ranges.begin(), ranges.end())];
        }

        [[nodiscard]] constexpr shared_ref<this_type> operator[](interval_type range) const&
        {
            this_type slice{};
            slice.hdr_ = oc::arrnd::squeeze(oc::arrnd::slice(hdr_, range, 0), arrnd_squeeze_type::left, 1);
            slice.buffsp_ = buffsp_;
            slice.is_creator_valid_ = original_valid_creator_;
            slice.creator_ = this;
            return slice;
        }
        [[nodiscard]] constexpr shared_ref<this_type> operator[](interval_type range) const&&
        {
            this_type slice{};
            slice.hdr_ = oc::arrnd::squeeze(oc::arrnd::slice(hdr_, range, 0), arrnd_squeeze_type::left, 1);
            slice.buffsp_ = buffsp_;
            return slice;
        }

        [[nodiscard]] constexpr shared_ref<this_type> operator()(interval_type range, size_type axis) const&
        {
            this_type slice{};
            slice.hdr_ = oc::arrnd::slice(hdr_, range, axis);
            slice.buffsp_ = buffsp_;
            slice.is_creator_valid_ = original_valid_creator_;
            slice.creator_ = this;
            return slice;
        }
        [[nodiscard]] constexpr shared_ref<this_type> operator()(interval_type range, size_type axis) const&&
        {
            this_type slice{};
            slice.hdr_ = oc::arrnd::slice(hdr_, range, axis);
            slice.buffsp_ = buffsp_;
            return slice;
        }

        // access relative array indices, might be slow for slices
        [[nodiscard]] constexpr const_reference operator()(size_type relative_index) const noexcept
        {
            assert(relative_index >= 0 && relative_index <= total(hdr_));
            return issliced(hdr_) ? buffsp_->data()[*(indexer_type(hdr_) + relative_index)]
                                  : buffsp_->data()[relative_index];
        }
        [[nodiscard]] constexpr reference operator()(size_type relative_index) noexcept
        {
            assert(relative_index >= 0 && relative_index <= total(hdr_));
            return issliced(hdr_) ? buffsp_->data()[*(indexer_type(hdr_) + relative_index)]
                                  : buffsp_->data()[relative_index];
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
            return arrnd_filter_proxy(*this, selector);
        }
        [[nodiscard]] constexpr auto operator()(std::initializer_list<size_type> indices) const
        {
            std::initializer_list<size_type> dims{std::size(indices)};
            return (*this)(replaced_type<size_type>(dims.begin(), dims.end(), indices.begin(), indices.end()));
        }

        [[nodiscard]] constexpr auto operator()(arrnd_shape_preset shape) const
        {
            return reshape(shape);
        }

        template <typename Pred>
            requires invocable_no_arrnd<Pred, value_type>
        [[nodiscard]] constexpr auto operator()(Pred&& pred) const
        {
            auto selector = [&pred](const value_type& value) {
                return pred(value);
            };

            return arrnd_filter_proxy(*this, selector);
        }

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return oc::arrnd::empty(hdr_) && (issliced(hdr_) || !buffsp_);
        }

        template <arrnd_type Arrnd>
        constexpr const this_type& copy_to(Arrnd&& dst) const
        {
            if (empty() || dst.empty()) {
                return *this;
            }

            indexer_type gen(hdr_);
            typename std::remove_cvref_t<Arrnd>::indexer_type dst_gen(dst.header());

            for (; gen && dst_gen; ++gen, ++dst_gen) {
                if constexpr (arrnd_type<value_type>) {
                    (*this)[*gen].copy_to(dst[*dst_gen]); // deep copying
                } else {
                    dst[*dst_gen] = (*this)[*gen];
                }
            }

            return *this;
        }

        template <arrnd_type Arrnd, iterator_of_type_integral InputIt>
        constexpr const this_type& copy_to(Arrnd&& dst, std::pair<InputIt, InputIt> indices) const
        {
            if (empty() || dst.empty() || std::distance(indices.first, indices.second) <= 0) {
                return *this;
            }

            indexer_type gen(hdr_);
            auto inds_it = indices.first;

            for (; gen && inds_it != indices.second; ++gen, ++inds_it) {
                if constexpr (arrnd_type<value_type>) {
                    (*this)[*gen].copy_to(dst[*inds_it]); // deep copying
                } else {
                    dst[*inds_it] = (*this)[*gen];
                }
            }

            return *this;
        }
        template <arrnd_type Arrnd, iterable_of_type_integral Cont>
            requires(!arrnd_type<Cont>)
        constexpr const this_type& copy_to(Arrnd&& dst, const Cont& indices) const
        {
            return copy_to(std::forward<Arrnd>(dst), std::make_pair(std::begin(indices), std::end(indices)));
        }
        template <arrnd_type Arrnd1, arrnd_type Arrnd2>
            requires(std::integral<typename Arrnd2::value_type>)
        constexpr const this_type& copy_to(Arrnd1&& dst, const Arrnd2& selector) const
        {
            // in case that indices isn't a vector treat it as a mask
            if constexpr (std::is_same_v<bool, typename Arrnd2::value_type>) {
                if (empty() || dst.empty() || selector.empty()) {
                    return *this;
                }

                assert(std::equal(std::begin(dst.header().dims()), std::end(dst.header().dims()),
                           std::begin(selector.header().dims()), std::end(selector.header().dims()))
                    && "boolean constraint considered as mask");

                indexer_type gen(hdr_);
                typename std::remove_cvref_t<Arrnd1>::indexer_type dst_gen(dst.header());
                typename Arrnd2::indexer_type slc_gen(selector.header());

                for (; gen && dst_gen && slc_gen; ++dst_gen, ++slc_gen) {
                    if (selector[*slc_gen]) {
                        if constexpr (arrnd_type<value_type>) {
                            (*this)[*gen].copy_to(dst[*dst_gen]); // deep copying
                        } else {
                            dst[*dst_gen] = (*this)[*gen];
                        }
                        ++gen;
                    }
                }

                return *this;
            } else {
                return copy_to(std::forward<Arrnd1>(dst), std::make_pair(std::begin(selector), std::end(selector)));
            }
        }
        template <arrnd_type Arrnd>
        constexpr const this_type& copy_to(Arrnd&& dst, std::initializer_list<size_type> indices) const
        {
            return copy_to(std::forward<Arrnd>(dst), std::make_pair(indices.begin(), indices.end()));
        }

        template <arrnd_type Arrnd, typename Pred>
            requires invocable_no_arrnd<Pred, value_type>
        constexpr const this_type& copy_to(Arrnd&& dst, Pred&& pred) const
        {
            if (empty() || dst.empty()) {
                return *this;
            }

            indexer_type gen(hdr_);
            typename std::remove_cvref_t<Arrnd>::indexer_type dst_gen(dst.header());

            for (; gen && dst_gen; ++dst_gen) {
                if (pred(dst[*dst_gen])) {
                    if constexpr (arrnd_type<value_type>) {
                        (*this)[*gen].copy_to(dst[*dst_gen]); // deep copying
                    } else {
                        dst[*dst_gen] = (*this)[*gen];
                    }
                    ++gen;
                }
            }

            return *this;
        }

        template <arrnd_type Arrnd, iterator_of_type_interval InputIt>
        constexpr const this_type& copy_to(Arrnd&& dst, const InputIt& first_range, const InputIt& last_range) const
        {
            copy_to(dst[std::make_pair(first_range, last_range)]);
            return *this;
        }
        template <arrnd_type Arrnd, iterable_of_type_interval Cont>
        constexpr const this_type& copy_to(Arrnd&& dst, const Cont& ranges) const
        {
            return copy_to(std::forward<Arrnd>(dst), std::begin(ranges), std::end(ranges));
        }
        template <arrnd_type Arrnd>
        constexpr const this_type& copy_to(Arrnd&& dst, std::initializer_list<interval_type> ranges) const
        {
            return copy_to(std::forward<Arrnd>(dst), ranges.begin(), ranges.end());
        }

        template <arrnd_type Arrnd>
        constexpr const this_type& set_to(Arrnd& dst) const
        {
            if (empty()) {
                dst = Arrnd{};
                return *this;
            }

            if (total(hdr_) == total(dst.header())) {
                if (!std::equal(std::begin(hdr_.dims()), std::end(hdr_.dims()), std::begin(dst.header().dims()),
                        std::end(dst.header().dims()))) {
                    dst.header() = header_type{hdr_.dims().cbegin(), hdr_.dims().cend()};
                }
                return copy_to(dst);
            }

            dst = Arrnd{hdr_.dims().cbegin(), hdr_.dims().cend()};
            if constexpr (arrnd_type<value_type>) {
                indexer_type gen(hdr_);
                typename std::remove_cvref_t<Arrnd>::indexer_type dst_gen(dst.header());

                for (; gen && dst_gen; ++gen, ++dst_gen) {
                    (*this)[*gen].set_to(dst[*dst_gen]);
                }
            }
            return copy_to(dst);
        }

        template <arrnd_type Arrnd = this_type>
        [[nodiscard]] constexpr Arrnd clone() const
        {
            Arrnd res;
            set_to(res);
            return res;
        }

        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr maybe_shared_ref<this_type> reshape(
            const InputIt& first_new_dim, const InputIt& last_new_dim) const
        {
            typename this_type::header_type new_header(first_new_dim, last_new_dim);
            assert(total(hdr_) == total(new_header));

            if (std::equal(std::begin(hdr_.dims()), std::end(hdr_.dims()), std::begin(new_header.dims()),
                    std::end(new_header.dims()))) {
                return *this;
            }

            if (issliced(hdr_)) {
                return clone().reshape(first_new_dim, last_new_dim);
            }

            this_type res(*this);
            res.hdr_ = std::move(new_header);

            return res;
        }

        template <iterable_of_type_integral Cont>
        [[nodiscard]] constexpr maybe_shared_ref<this_type> reshape(const Cont& new_dims) const
        {
            return reshape(std::begin(new_dims), std::end(new_dims));
        }
        [[nodiscard]] constexpr maybe_shared_ref<this_type> reshape(std::initializer_list<size_type> new_dims) const
        {
            return reshape(new_dims.begin(), new_dims.end());
        }

        [[nodiscard]] constexpr maybe_shared_ref<this_type> reshape(arrnd_shape_preset shape) const
        {
            if (empty()) {
                return *this;
            }

            switch (shape) {
            case arrnd_shape_preset::vector:
                return reshape({total(hdr_)});
            case arrnd_shape_preset::row:
                return reshape({size_type{1}, total(hdr_)});
            case arrnd_shape_preset::column:
                return reshape({total(hdr_), size_type{1}});
            default:
                assert(false && "unknown arrnd_shape_preset value");
                return this_type();
            }
        }

        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr maybe_shared_ref<this_type> resize(
            const InputIt& first_new_dim, const InputIt& last_new_dim) const
        {
            if (std::equal(hdr_.dims().cbegin(), hdr_.dims().cend(), first_new_dim, last_new_dim)) {
                return *this;
            }

            if (empty()) {
                return this_type(first_new_dim, last_new_dim);
            }

            this_type res(first_new_dim, last_new_dim);

            size_type n = std::min(
                {std::distance(first_new_dim, last_new_dim), static_cast<std::ptrdiff_t>(hdr_.dims().size())});

            typename header_type::storage_info_type::template replaced_type<interval_type>::storage_type prev_ranges(
                hdr_.dims().size());
            std::fill(prev_ranges.begin(), prev_ranges.end(), interval_type::full());

            typename header_type::storage_info_type::template replaced_type<interval_type>::storage_type new_ranges(
                std::distance(first_new_dim, last_new_dim));
            std::fill(new_ranges.begin(), new_ranges.end(), interval_type::full());

            size_type pi = hdr_.dims().size() - 1;
            size_type ni = std::distance(first_new_dim, last_new_dim) - 1;
            for (int i = n - 1; i >= 0; --i) {
                size_type prev_dim = *std::next(hdr_.dims().cbegin(), pi);
                size_type new_dim = *std::next(first_new_dim, ni);
                *std::next(prev_ranges.begin(), pi--) = interval_type::to(std::min({prev_dim, new_dim}));
                *std::next(new_ranges.begin(), ni--) = interval_type::to(std::min({prev_dim, new_dim}));
            }

            auto sthis = (*this)[prev_ranges];
            auto sres = res[new_ranges];

            indexer_type gen(sthis.header());
            indexer_type res_gen(sres.header());

            while (gen && res_gen) {
                sres[*res_gen] = sthis[*gen];
                ++gen;
                ++res_gen;
            }

            return res;
        }

        template <iterable_of_type_integral Cont>
        [[nodiscard]] constexpr maybe_shared_ref<this_type> resize(const Cont& new_dims) const
        {
            return resize(std::begin(new_dims), std::end(new_dims));
        }
        [[nodiscard]] constexpr maybe_shared_ref<this_type> resize(std::initializer_list<size_type> new_dims) const
        {
            return resize(new_dims.begin(), new_dims.end());
        }

        template <arrnd_type Arrnd>
        [[nodiscard]] constexpr maybe_shared_ref<this_type> push_back(const Arrnd& arr, size_type axis = 0) const
        {
            size_type ind = empty() ? size_type{0} : *std::next(hdr_.dims().cbegin(), axis);
            return insert<Arrnd>(arr, ind, axis);
        }

        template <arrnd_type Arrnd>
        [[nodiscard]] constexpr maybe_shared_ref<this_type> push_front(const Arrnd& arr, size_type axis = 0) const
        {
            size_type ind = empty() ? size_type{0} : *std::next(hdr_.dims().cbegin(), axis);
            return insert<Arrnd>(arr, 0, axis);
        }

        template <arrnd_type Arrnd>
            requires(arrnd_depths_match<this_type, Arrnd>)
        [[nodiscard]] constexpr maybe_shared_ref<this_type> insert(
            const Arrnd& arr, size_type ind, size_type axis = 0) const
        {
            if (empty() && arr.empty()) {
                assert(ind == 0);
                return *this;
            }

            if (empty()) {
                assert(ind == 0);
                return arr.template clone<this_type>();
            }

            if (arr.empty()) {
                assert(ind >= 0 && ind <= *std::next(hdr_.dims().cbegin(), axis));
                return *this;
            }

            assert(ind >= 0 && ind <= *std::next(hdr_.dims().cbegin(), axis));
            assert(std::ssize(hdr_.dims()) == std::ssize(arr.header().dims()));

            bool same_dims_except_at_axis = std::equal(hdr_.dims().cbegin(), std::next(hdr_.dims().cbegin(), axis),
                arr.header().dims().cbegin(), std::next(arr.header().dims().cbegin(), axis));
            same_dims_except_at_axis &= std::equal(std::next(hdr_.dims().cbegin(), axis + 1), hdr_.dims().cend(),
                std::next(arr.header().dims().cbegin(), axis + 1), arr.header().dims().cend());
            assert(same_dims_except_at_axis);

            this_type res({total(hdr_) + total(arr.header())});
            auto new_dims = hdr_.dims();
            new_dims[axis] += *std::next(std::begin(arr.header().dims()), axis);
            res.hdr_ = header_type(new_dims);

            indexer_type gen(move(hdr_, axis, 0));
            typename Arrnd::indexer_type arr_gen(move(arr.header(), axis, 0));
            indexer_type res_gen(move(res.hdr_, axis, 0));

            size_type cycle = ind
                * (std::reduce(res.hdr_.dims().begin(), res.hdr_.dims().end(), size_type{1}, std::multiplies<>{})
                    / *std::next(res.hdr_.dims().cbegin(), axis));

            auto ptr = storage()->data();
            auto res_ptr = res.storage()->data();
            auto arr_ptr = arr.storage()->data();

            for (; gen && res_gen && cycle; --cycle, ++gen, ++res_gen) {
                res_ptr[*res_gen] = ptr[*gen];
            }
            for (; arr_gen && res_gen; ++arr_gen, ++res_gen) {
                res_ptr[*res_gen] = arr_ptr[*arr_gen];
            }
            for (; gen && res_gen; ++gen, ++res_gen) {
                res_ptr[*res_gen] = ptr[*gen];
            }

            return res;
        }

        template <iterator_of_template_type<std::tuple> InputIt>
        [[nodiscard]] constexpr maybe_shared_ref<this_type> repeat(InputIt first_tuple, InputIt last_tuple) const
        {
            assert(std::distance(first_tuple, last_tuple) >= 0);

            auto res = *this;
            auto mid = res;

            std::for_each(first_tuple, last_tuple, [&res, &mid](const auto& tuple) {
                for (size_type i = 0; i < std::get<0>(tuple) - 1; ++i) {
                    res = res.push_back(mid, std::get<1>(tuple));
                }
                mid = res;
            });

            return res;
        }
        template <template_type<std::tuple> Tuple>
        [[nodiscard]] constexpr maybe_shared_ref<this_type> repeat(std::initializer_list<Tuple> count_axis_tuples) const
        {
            return repeat(count_axis_tuples.begin(), count_axis_tuples.end());
        }
        template <iterable_of_template_type<std::tuple> Cont>
        [[nodiscard]] constexpr maybe_shared_ref<this_type> repeat(const Cont& count_axis_tuples) const
        {
            return repeat(std::begin(count_axis_tuples), std::end(count_axis_tuples));
        }

        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr maybe_shared_ref<this_type> repeat(InputIt first_rep, InputIt last_rep) const
        {
            assert(std::distance(first_rep, last_rep) <= hdr_.dims().size());

            auto nreps = std::distance(first_rep, last_rep);

            auto z = zip(zipped_iterator(first_rep, last_rep));

            auto res = *this;
            auto mid = res;
            size_type axis = 0;
            std::for_each(z.begin(), z.end(), [&res, &mid, &axis](const auto& tuple) {
                for (size_type i = 0; i < std::get<0>(tuple) - 1; ++i) {
                    res = res.push_back(mid, axis);
                }
                ++axis;
                mid = res;
            });

            return res;
        }
        [[nodiscard]] constexpr maybe_shared_ref<this_type> repeat(std::initializer_list<size_type> reps) const
        {
            return repeat(reps.begin(), reps.end());
        }
        template <iterable_of_type_integral Cont>
        [[nodiscard]] constexpr maybe_shared_ref<this_type> repeat(const Cont& reps) const
        {
            return repeat(std::begin(reps), std::end(reps));
        }

        [[nodiscard]] constexpr maybe_shared_ref<this_type> erase(
            size_type count, size_type ind, size_type axis = 0) const
        {
            if (empty()) {
                assert(ind == 0 && count == 0);
                return *this;
            }

            assert(ind >= 0 && ind < *std::next(hdr_.dims().cbegin(), axis));
            assert(ind + count <= *std::next(hdr_.dims().cbegin(), axis));

            this_type res({total(hdr_) - (total(hdr_) / hdr_.dims()[axis]) * count});
            auto new_dims = hdr_.dims();
            new_dims[axis] -= count;
            res.hdr_ = header_type(new_dims);

            if (res.empty()) {
                return res;
            }

            indexer_type gen(move(hdr_, axis, 0));
            indexer_type res_gen(move(res.hdr_, axis, 0));

            size_type cycle = ind
                * (std::reduce(res.hdr_.dims().begin(), res.hdr_.dims().end(), size_type{1}, std::multiplies<>{})
                    / *std::next(res.hdr_.dims().cbegin(), axis));

            size_type removals = total(hdr_) - total(res.hdr_);

            auto ptr = storage()->data();
            auto res_ptr = res.storage()->data();

            for (; gen && res_gen && cycle; --cycle, ++gen, ++res_gen) {
                res_ptr[*res_gen] = ptr[*gen];
            }
            for (; gen && removals; --removals, ++gen) {
                //ptr[*gen] = arr_ptr[*arr_gen];
            }
            for (; gen && res_gen; ++gen, ++res_gen) {
                res_ptr[*res_gen] = ptr[*gen];
            }

            return res;
        }

        [[nodiscard]] constexpr maybe_shared_ref<this_type> pop_front(size_type count = 1, size_type axis = 0) const
        {
            return erase(empty() ? 0 : count, 0, axis);
        }

        [[nodiscard]] constexpr maybe_shared_ref<this_type> pop_back(size_type count = 1, size_type axis = 0) const
        {
            size_type fixed_count = empty() ? 0 : count;
            size_type ind = empty() ? size_type{0} : *std::next(hdr_.dims().cbegin(), axis) - fixed_count;
            return erase(fixed_count, ind, axis);
        }

        template <std::int64_t Level, typename Func>
            requires(Level > 0 && invocable_no_arrnd<Func, inner_value_type<Level>>)
        [[nodiscard]] constexpr auto transform(Func&& func) const
        {
            using transformed_type = inner_replaced_type<std::invoke_result_t<Func, inner_value_type<Level>>, Level>;

            if (empty()) {
                return transformed_type();
            }

            transformed_type res(hdr_.dims().cbegin(), hdr_.dims().cend());

            indexer_type gen(hdr_);
            typename transformed_type::indexer_type res_gen(res.header());

            for (; gen && res_gen; ++gen, ++res_gen) {
                res[*res_gen] = (*this)[*gen].template transform<Level - 1, Func>(std::forward<Func>(func));
            }

            return res;
        }

        template <std::int64_t Level, typename Func>
            requires(Level == 0 && invocable_no_arrnd<Func, inner_value_type<Level>>)
        [[nodiscard]] constexpr auto transform(Func&& func) const
        {
            using transformed_type = inner_replaced_type<std::invoke_result_t<Func, inner_value_type<Level>>, Level>;

            if (empty()) {
                return transformed_type();
            }

            transformed_type res(hdr_.dims().cbegin(), hdr_.dims().cend());

            indexer_type gen(hdr_);
            typename transformed_type::indexer_type res_gen(res.header());

            for (; gen && res_gen; ++gen, ++res_gen) {
                res[*res_gen] = func((*this)[*gen]);
            }

            return res;
        }

        template <typename Func>
            requires invocable_no_arrnd<Func, inner_value_type<this_type::depth>>
        [[nodiscard]] constexpr auto transform(Func&& func) const
        {
            return transform<this_type::depth, Func>(std::forward<Func>(func));
        }

        template <std::int64_t Level, arrnd_type Arrnd, typename Func>
            requires(Level > 0
                && invocable_no_arrnd<Func, inner_value_type<Level>, typename Arrnd::template inner_value_type<Level>>)
        [[nodiscard]] constexpr auto transform(const Arrnd& arr, Func&& func) const
        {
            using transformed_type = inner_replaced_type<
                std::invoke_result_t<Func, inner_value_type<Level>, typename Arrnd::template inner_value_type<Level>>,
                Level>;

            if (empty()) {
                return transformed_type();
            }

            assert(total(hdr_) == total(arr.header()));

            transformed_type res(hdr_.dims().cbegin(), hdr_.dims().cend());

            indexer_type gen(hdr_);
            typename transformed_type::indexer_type res_gen(res.header());
            typename transformed_type::indexer_type arr_gen(arr.header());

            for (; gen && res_gen && arr_gen; ++gen, ++res_gen, ++arr_gen) {
                res[*res_gen]
                    = (*this)[*gen]
                          .template transform<Level - 1, typename Arrnd::template inner_value_type<Level - 1>, Func>(
                              arr[*arr_gen], std::forward<Func>(func));
            }

            return res;
        }

        template <std::int64_t Level, arrnd_type Arrnd, typename Func>
            requires(Level == 0
                && invocable_no_arrnd<Func, inner_value_type<Level>, typename Arrnd::template inner_value_type<Level>>)
        [[nodiscard]] constexpr auto transform(const Arrnd& arr, Func&& func) const
        {
            using transformed_type = inner_replaced_type<
                std::invoke_result_t<Func, inner_value_type<Level>, typename Arrnd::template inner_value_type<Level>>,
                Level>;

            if (empty()) {
                return transformed_type();
            }

            if (isscalar(hdr_)) {
                auto val = (*this)(0);
                auto tfunc = [&](const auto& a) -> typename transformed_type::value_type {
                    return func(val, a);
                };
                return arr.template transform<Level>(tfunc);
            }

            if (isscalar(arr.header())) {
                auto val = arr(0);
                auto tfunc = [&](const auto& a) -> typename transformed_type::value_type {
                    return func(a, val);
                };
                return transform<Level>(tfunc);
            }

            auto arr1 = *this;
            auto arr2 = arr;

            auto is_reduced_dims = [](auto rdims, auto odims) {
                if (std::size(rdims) != std::size(odims)) {
                    return false;
                }

                return !std::equal(std::begin(rdims), std::end(rdims), std::begin(odims), std::end(odims))
                    && std::transform_reduce(std::begin(rdims), std::end(rdims), std::begin(odims), true,
                        std::logical_and<>{}, [](auto d1, auto d2) {
                            return d1 == d2 || d1 == 1;
                        });
            };

            auto complement_dims = [](auto rdims, auto odims) {
                assert(std::size(rdims) == std::size(odims));

                typename header_type::extent_storage_type comp_dims(std::size(rdims));

                std::transform(
                    std::begin(rdims), std::end(rdims), std::begin(odims), std::begin(comp_dims), [](auto rd, auto cd) {
                        return cd - rd + 1;
                    });

                return comp_dims;
            };

            if (is_reduced_dims(arr1.header().dims(), arr2.header().dims())) {
                auto reps = complement_dims(arr1.header().dims(), arr2.header().dims());
                arr1 = arr1.repeat(reps);
            } else if (is_reduced_dims(arr2.header().dims(), arr1.header().dims())) {
                auto reps = complement_dims(arr2.header().dims(), arr1.header().dims());
                arr2 = arr2.repeat(reps);
            }

            assert(total(arr1.header()) == total(arr2.header()));

            transformed_type res(arr1.header().dims().cbegin(), arr1.header().dims().cend());

            indexer_type gen(arr1.header());
            typename transformed_type::indexer_type res_gen(res.header());

            typename Arrnd::indexer_type arr_gen(arr2.header());

            for (; gen && arr_gen && res_gen; ++gen, ++arr_gen, ++res_gen) {
                res[*res_gen] = func(arr1[*gen], arr2[*arr_gen]);
            }

            return res;
        }

        template <arrnd_type Arrnd, typename Func>
            requires invocable_no_arrnd<Func, inner_value_type<this_type::depth>, typename Arrnd::value_type>
        [[nodiscard]] constexpr auto transform(const Arrnd& arr, Func&& func) const
        {
            return transform<this_type::depth, Arrnd, Func>(arr, std::forward<Func>(func));
        }

        template <std::int64_t Level, typename Func>
            requires(Level > 0 && invocable_no_arrnd<Func, inner_value_type<Level>>)
        constexpr this_type& apply(Func&& func)
        {
            if (empty()) {
                return *this;
            }

            for (indexer_type gen(hdr_); gen; ++gen) {
                (*this)[*gen].template apply<Level - 1, Func>(std::forward<Func>(func));
            }

            return *this;
        }

        template <std::int64_t Level, typename Func>
            requires(Level == 0 && invocable_no_arrnd<Func, inner_value_type<Level>>)
        constexpr this_type& apply(Func&& func)
        {
            if (empty()) {
                return *this;
            }

            constexpr bool is_void_func = std::is_same_v<std::invoke_result_t<Func, inner_value_type<Level>>, void>;

            for (indexer_type gen(hdr_); gen; ++gen) {
                if constexpr (is_void_func) {
                    func((*this)[*gen]);
                } else {
                    (*this)[*gen] = func((*this)[*gen]);
                }
            }

            return *this;
        }

        template <typename Func>
            requires invocable_no_arrnd<Func, inner_value_type<this_type::depth>>
        constexpr this_type& apply(Func&& func)
        {
            return apply<this_type::depth, Func>(std::forward<Func>(func));
        }

        template <std::int64_t Level, arrnd_type Arrnd, typename Func>
            requires(Level == 0
                && invocable_no_arrnd<Func, inner_value_type<Level>, typename Arrnd::template inner_value_type<Level>>)
        constexpr this_type& apply(const Arrnd& arr, Func&& func)
        {
            if (empty() || arr.empty()) {
                return *this;
            }

            if (isscalar(arr.header())) {
                auto val = arr(0);
                auto tfunc = [&](const auto& a) {
                    return func(a, val);
                };
                return apply<Level>(tfunc);
            }

            auto carr = arr;

            auto is_reduced_dims = [](auto rdims, auto odims) {
                if (std::size(rdims) != std::size(odims)) {
                    return false;
                }

                return !std::equal(std::begin(rdims), std::end(rdims), std::begin(odims), std::end(odims))
                    && std::transform_reduce(std::begin(rdims), std::end(rdims), std::begin(odims), true,
                        std::logical_and<>{}, [](auto d1, auto d2) {
                            return d1 == d2 || d1 == 1;
                        });
            };

            auto complement_dims = [](auto rdims, auto odims) {
                assert(std::size(rdims) == std::size(odims));

                typename header_type::extent_storage_type comp_dims(std::size(rdims));

                std::transform(
                    std::begin(rdims), std::end(rdims), std::begin(odims), std::begin(comp_dims), [](auto rd, auto cd) {
                        return cd - rd + 1;
                    });

                return comp_dims;
            };

            if (is_reduced_dims(arr.header().dims(), hdr_.dims())) {
                auto reps = complement_dims(arr.header().dims(), hdr_.dims());
                carr = arr.repeat(reps);
            }

            assert(total(hdr_) == total(carr.header()));

            indexer_type gen(hdr_);
            typename std::remove_cvref_t<Arrnd>::indexer_type arr_gen(carr.header());

            constexpr bool is_void_func = std::is_same_v<
                std::invoke_result_t<Func, inner_value_type<Level>, typename Arrnd::template inner_value_type<Level>>,
                void>;

            for (; gen && arr_gen; ++gen, ++arr_gen) {
                if constexpr (is_void_func) {
                    func((*this)[*gen], carr[*arr_gen]);
                } else {
                    (*this)[*gen] = func((*this)[*gen], carr[*arr_gen]);
                }
            }

            return *this;
        }

        template <std::int64_t Level, arrnd_type Arrnd, typename Func>
            requires(Level > 0
                && invocable_no_arrnd<Func, inner_value_type<Level>, typename Arrnd::template inner_value_type<Level>>)
        constexpr this_type& apply(const Arrnd& arr, Func&& func)
        {
            if (empty() || arr.empty()) {
                return *this;
            }

            assert(total(hdr_) == total(arr.header()));

            for (indexer_type gen(hdr_); gen; ++gen) {
                (*this)[*gen].template apply<Level - 1, typename Arrnd::template inner_value_type<Level - 1>, Func>(
                    arr, std::forward<Func>(func));
            }

            return *this;
        }

        template <arrnd_type Arrnd, typename Func>
            requires invocable_no_arrnd<Func, inner_value_type<this_type::depth>,
                typename Arrnd::template inner_value_type<Arrnd::depth>>
        constexpr this_type& apply(const Arrnd& arr, Func&& func)
        {
            return apply<this_type::depth, Arrnd, Func>(arr, std::forward<Func>(func));
        }

        template <std::int64_t Level, typename Func>
            requires(Level == 0 && invocable_no_arrnd<Func, inner_value_type<Level>, inner_value_type<Level>>)
        [[nodiscard]] constexpr auto reduce(Func&& func) const
        {
            using reduced_type = std::invoke_result_t<Func, inner_value_type<Level>, inner_value_type<Level>>;

            if (empty()) {
                return reduced_type();
            }

            indexer_type gen(hdr_);

            reduced_type res = (((*this)[*gen]));
            ++gen;

            while (gen) {
                res = func(std::forward<reduced_type>(res), (*this)[*gen]);
                ++gen;
            }

            return res;
        }
        template <std::int64_t Level, typename Func>
            requires(Level > 0 && invocable_no_arrnd<Func, inner_value_type<Level>, inner_value_type<Level>>)
        [[nodiscard]] constexpr auto reduce(Func&& func) const
        {
            using reduced_type
                = inner_replaced_type<std::invoke_result_t<Func, inner_value_type<Level>, inner_value_type<Level>>,
                    Level - 1>;

            if (empty()) {
                return reduced_type();
            }

            reduced_type res(hdr_.dims());

            indexer_type gen(hdr_);
            indexer_type res_gen(res.header());

            for (; gen && res_gen; ++gen, ++res_gen) {
                res[*res_gen] = (*this)[*gen].template reduce<Level - 1, Func>(std::forward<Func>(func));
            }

            return res;
        }
        template <typename Func>
            requires(invocable_no_arrnd<Func, inner_value_type<this_type::depth>, inner_value_type<this_type::depth>>)
        [[nodiscard]] constexpr auto reduce(Func&& func) const
        {
            return reduce<this_type::depth, Func>(std::forward<Func>(func));
        }

        template <std::int64_t Level, typename U, typename Func>
            requires(Level == 0 && invocable_no_arrnd<Func, U, inner_value_type<Level>>)
        [[nodiscard]] constexpr auto fold(const U& init, Func&& func) const
        {
            using folded_type = std::invoke_result_t<Func, U, inner_value_type<Level>>;

            if (empty()) {
                return init;
            }

            folded_type res = (init);
            for (indexer_type gen{hdr_}; gen; ++gen) {
                res = func(res, (*this)[*gen]);
            }

            return res;
        }
        template <std::int64_t Level, typename U, typename Func>
            requires(Level > 0 && invocable_no_arrnd<Func, U, inner_value_type<Level>>)
        [[nodiscard]] constexpr auto fold(const U& init, Func&& func) const
        {
            using folded_type = inner_replaced_type<std::invoke_result_t<Func, U, inner_value_type<Level>>, Level - 1>;

            if (empty()) {
                return folded_type();
            }

            folded_type res(hdr_.dims());

            indexer_type gen(hdr_);
            indexer_type res_gen(res.header());

            for (; gen && res_gen; ++gen, ++res_gen) {
                res[*res_gen] = (*this)[*gen].template fold<Level - 1, U, Func>(init, std::forward<Func>(func));
            }

            return res;
        }
        template <typename U, typename Func>
            requires(invocable_no_arrnd<Func, U, inner_value_type<this_type::depth>>)
        [[nodiscard]] constexpr auto fold(const U& init, Func&& func) const
        {
            return fold<this_type::depth, U, Func>(init, std::forward<Func>(func));
        }

        template <std::int64_t Level, typename Func>
            requires(Level == 0 && invocable_no_arrnd<Func, inner_value_type<Level>, inner_value_type<Level>>)
        [[nodiscard]] constexpr auto reduce(size_type axis, Func&& func) const
        {
            using reduced_type
                = replaced_type<std::invoke_result_t<Func, inner_value_type<Level>, inner_value_type<Level>>>;

            if (empty()) {
                return reduced_type();
            }

            auto new_dims = hdr_.dims();
            if (std::size(new_dims) > 1) {
                new_dims.erase(std::next(std::begin(new_dims), axis));
                new_dims.shrink_to_fit();
            } else if (std::size(new_dims) == 1) {
                new_dims[0] = 1;
            }
            auto new_header = header_type(new_dims);
            reduced_type res({total(new_header)});
            res.header() = std::move(new_header);

            indexer_type gen(move(hdr_, std::ssize(hdr_.dims()) - axis - 1, 0));
            indexer_type res_gen(res.header());

            const size_type reduction_iteration_cycle{hdr_.dims()[axis]};

            while (gen && res_gen) {
                typename reduced_type::value_type res_element = (((*this)[*gen]));
                ++gen;
                for (size_type i = 0; i < reduction_iteration_cycle - 1; ++i, ++gen) {
                    res_element = func(std::forward<typename reduced_type::value_type>(res_element), (*this)[*gen]);
                }
                res[*res_gen] = res_element;
                ++res_gen;
            }

            return res;
        }
        template <std::int64_t Level, typename Func>
            requires(Level > 0 && invocable_no_arrnd<Func, inner_value_type<Level>, inner_value_type<Level>>)
        [[nodiscard]] constexpr auto reduce(size_type axis, Func&& func) const
        {
            using reduced_type = inner_replaced_type<
                replaced_type<std::invoke_result_t<Func, inner_value_type<Level>, inner_value_type<Level>>>, Level - 1>;

            if (empty()) {
                return reduced_type();
            }

            reduced_type res(hdr_.dims());

            indexer_type gen(hdr_);
            indexer_type res_gen(res.header());

            for (; gen && res_gen; ++gen, ++res_gen) {
                res[*res_gen] = (*this)[*gen].template reduce<Level - 1, Func>(axis, std::forward<Func>(func));
            }

            return res;
        }
        template <typename Func>
            requires(invocable_no_arrnd<Func, inner_value_type<this_type::depth>, inner_value_type<this_type::depth>>)
        [[nodiscard]] constexpr auto reduce(size_type axis, Func&& func) const
        {
            return reduce<this_type::depth, Func>(axis, std::forward<Func>(func));
        }

        template <std::int64_t Level, arrnd_type Arrnd, typename Func>
            requires(Level == 0 && invocable_no_arrnd<Func, typename Arrnd::value_type, inner_value_type<Level>>)
        [[nodiscard]] constexpr auto fold(size_type axis, const Arrnd& inits, Func&& func) const
        {
            using folded_type
                = replaced_type<std::invoke_result_t<Func, typename Arrnd::value_type, inner_value_type<Level>>>;

            if (empty()) {
                return folded_type();
            }

            assert(inits.header().dims().size() == 1 && inits.header().dims()[0] == total(hdr_) / hdr_.dims()[axis]);

            auto new_dims = hdr_.dims();
            if (std::size(new_dims) > 1) {
                new_dims.erase(std::next(std::begin(new_dims), axis));
                new_dims.shrink_to_fit();
            } else if (std::size(new_dims) == 1) {
                new_dims[0] = 1;
            }
            auto new_header = header_type(new_dims);
            folded_type res({total(new_header)});
            res.header() = std::move(new_header);

            indexer_type gen(move(hdr_, std::ssize(hdr_.dims()) - axis - 1, 0));
            indexer_type res_gen(res.header());
            typename Arrnd::indexer_type init_gen(inits.header());

            const size_type reduction_iteration_cycle{hdr_.dims()[axis]};

            while (gen && res_gen && init_gen) {
                typename folded_type::value_type res_element = ((inits[*init_gen]));
                for (size_type i = 0; i < reduction_iteration_cycle; ++i, ++gen) {
                    res_element = func(res_element, (*this)[*gen]);
                }
                res[*res_gen] = std::move(res_element);
                ++res_gen;
                ++init_gen;
            }

            return res;
        }
        template <std::int64_t Level, arrnd_type Arrnd, typename Func>
            requires(Level > 0 && invocable_no_arrnd<Func, typename Arrnd::value_type, inner_value_type<Level>>)
        [[nodiscard]] constexpr auto fold(size_type axis, const Arrnd& inits, Func&& func) const
        {
            using folded_type = inner_replaced_type<
                replaced_type<std::invoke_result_t<Func, typename Arrnd::value_type, inner_value_type<Level>>>,
                Level - 1>;

            if (empty()) {
                return folded_type();
            }

            folded_type res(hdr_.dims());

            indexer_type gen(hdr_);
            indexer_type res_gen(res.header());

            for (; gen && res_gen; ++gen, ++res_gen) {
                res[*res_gen]
                    = (*this)[*gen].template fold<Level - 1, Arrnd, Func>(axis, inits, std::forward<Func>(func));
            }

            return res;
        }
        template <arrnd_type Arrnd, typename Func>
            requires(invocable_no_arrnd<Func, typename Arrnd::value_type, inner_value_type<this_type::depth>>)
        [[nodiscard]] constexpr auto fold(size_type axis, const Arrnd& inits, Func&& func) const
        {
            return fold<this_type::depth, Arrnd, Func>(axis, inits, std::forward<Func>(func));
        }

        template <std::int64_t Level, typename Pred>
            requires(Level == 0 && invocable_no_arrnd<Pred, inner_value_type<Level>>)
        [[nodiscard]] constexpr this_type filter(Pred&& pred) const
        {
            if (empty()) {
                return this_type();
            }

            this_type res({total(hdr_)});

            indexer_type gen(hdr_);
            indexer_type res_gen(res.hdr_);

            size_type res_count{0};

            while (gen && res_gen) {
                if (pred((*this)[*gen])) {
                    res[*res_gen] = (*this)[*gen];
                    ++res_count;
                    ++res_gen;
                }
                ++gen;
            }

            if (res_count == 0) {
                return this_type();
            }

            if (res_count < total(hdr_)) {
                return res.resize({res_count});
            }

            return res;
        }
        template <std::int64_t Level, typename Pred>
            requires(Level > 0 && invocable_no_arrnd<Pred, inner_value_type<Level>>)
        [[nodiscard]] constexpr this_type filter(Pred&& pred) const
        {
            if (empty()) {
                return this_type();
            }

            this_type res(hdr_.dims().cbegin(), hdr_.dims().cend());

            indexer_type gen(hdr_);
            indexer_type res_gen(res.header());

            for (; gen && res_gen; ++gen, ++res_gen) {
                res[*res_gen] = (*this)[*gen].template filter<Level - 1, Pred>(std::forward<Pred>(pred));
            }

            return res;
        }
        template <typename Pred>
            requires invocable_no_arrnd<Pred, inner_value_type<this_type::depth>>
        [[nodiscard]] constexpr this_type filter(Pred&& pred) const
        {
            return filter<this_type::depth, Pred>(std::forward<Pred>(pred));
        }

        template <std::int64_t Level, arrnd_type Arrnd>
            requires(Level == 0)
        [[nodiscard]] constexpr this_type filter(const Arrnd& selector) const
        {
            if (empty()) {
                return this_type();
            }

            if constexpr (std::is_same_v<bool, typename Arrnd::value_type>) {
                assert(std::equal(std::begin(hdr_.dims()), std::end(hdr_.dims()), std::begin(selector.header().dims()),
                    std::end(selector.header().dims())));

                this_type res({total(hdr_)});

                indexer_type gen(hdr_);
                typename Arrnd::indexer_type selector_gen(selector.header());

                indexer_type res_gen(res.hdr_);

                size_type res_count{0};

                while (gen && selector_gen && res_gen) {
                    if (selector[*selector_gen]) {
                        res[*res_gen] = (*this)[*gen];
                        ++res_count;
                        ++res_gen;
                    }
                    ++gen;
                    ++selector_gen;
                }

                if (res_count == 0) {
                    return this_type();
                }

                if (res_count < total(hdr_)) {
                    return res.resize({res_count});
                }

                return res;
            } else {
                this_type res(selector.header().dims());

                indexer_type res_gen(res.hdr_);
                typename Arrnd::indexer_type slc_gen(selector.header());

                for (; res_gen && slc_gen; ++res_gen, ++slc_gen) {
                    res[*res_gen] = (*this)[selector[*slc_gen]];
                }

                return res;
            }
        }
        template <std::int64_t Level, arrnd_type Arrnd>
            requires(Level > 0)
        [[nodiscard]] constexpr this_type filter(const Arrnd& selector) const
        {
            if (empty()) {
                return this_type();
            }

            this_type res(hdr_.dims().cbegin(), hdr_.dims().cend());

            indexer_type gen(hdr_);
            indexer_type res_gen(res.header());

            for (; gen && res_gen; ++gen, ++res_gen) {
                res[*res_gen] = (*this)[*gen].template filter<Level - 1, Arrnd>(selector);
            }

            return res;
        }
        template <arrnd_type Arrnd>
        [[nodiscard]] constexpr this_type filter(const Arrnd& selector) const
        {
            return filter<this_type::depth, Arrnd>(selector);
        }

        template <std::int64_t Level, iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto filter(InputIt first_ind, InputIt last_ind) const
        {
            return filter<Level>(replaced_type<size_type>({std::distance(first_ind, last_ind)}, first_ind, last_ind));
        }
        template <std::int64_t Level, iterable_of_type_integral Cont>
            requires(!arrnd_type<Cont>)
        [[nodiscard]] constexpr auto filter(const Cont& indices) const
        {
            return filter<Level>(
                replaced_type<size_type>({std::ssize(indices)}, std::begin(indices), std::end(indices)));
        }
        template <std::int64_t Level>
        [[nodiscard]] constexpr auto filter(std::initializer_list<size_type> indices) const
        {
            return filter<Level>(replaced_type<size_type>({std::ssize(indices)}, indices.begin(), indices.end()));
        }

        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto filter(InputIt first_ind, InputIt last_ind) const
        {
            return filter<this_type::depth>(
                replaced_type<size_type>({std::distance(first_ind, last_ind)}, first_ind, last_ind));
        }
        template <iterable_of_type_integral Cont>
            requires(!arrnd_type<Cont>)
        [[nodiscard]] constexpr auto filter(const Cont& indices) const
        {
            return filter<this_type::depth>(
                replaced_type<size_type>({std::ssize(indices)}, std::begin(indices), std::end(indices)));
        }
        [[nodiscard]] constexpr auto filter(std::initializer_list<size_type> indices) const
        {
            return filter<this_type::depth>(
                replaced_type<size_type>({std::ssize(indices)}, indices.begin(), indices.end()));
        }

        template <std::int64_t Level, typename Pred>
            requires(Level == 0 && invocable_no_arrnd<Pred, inner_value_type<Level>>)
        [[nodiscard]] constexpr auto find(Pred&& pred) const
        {
            using found_type = inner_replaced_type<size_type, Level>;

            if (empty()) {
                return found_type();
            }

            found_type res({total(hdr_)});

            indexer_type gen(hdr_);
            typename found_type::indexer_type res_gen(res.header());

            size_type res_count{0};

            while (gen && res_gen) {
                if (pred((*this)[*gen])) {
                    res[*res_gen] = *gen;
                    ++res_count;
                    ++res_gen;
                }
                ++gen;
            }

            if (res_count == 0) {
                return found_type();
            }

            if (res_count < total(hdr_)) {
                return res.resize({res_count});
            }

            return res;
        }
        template <std::int64_t Level, typename Pred>
            requires(Level > 0 && invocable_no_arrnd<Pred, inner_value_type<Level>>)
        [[nodiscard]] constexpr auto find(Pred&& pred) const
        {
            using found_type = inner_replaced_type<size_type, Level>;

            if (empty()) {
                return found_type();
            }

            found_type res(hdr_.dims().cbegin(), hdr_.dims().cend());

            indexer_type gen(hdr_);
            indexer_type res_gen(res.header());

            for (; gen && res_gen; ++gen, ++res_gen) {
                res[*res_gen] = (*this)[*gen].template find<Level - 1, Pred>(std::forward<Pred>(pred));
            }

            return res;
        }
        template <typename Pred>
            requires invocable_no_arrnd<Pred, inner_value_type<this_type::depth>>
        [[nodiscard]] constexpr auto find(Pred&& pred) const
        {
            return find<this_type::depth, Pred>(std::forward<Pred>(pred));
        }

        template <std::int64_t Level, arrnd_type Arrnd>
            requires(Level == 0)
        [[nodiscard]] constexpr auto find(const Arrnd& mask) const
        {
            using found_type = inner_replaced_type<size_type, Level>;

            if (empty()) {
                return found_type();
            }

            assert(std::equal(std::begin(hdr_.dims()), std::end(hdr_.dims()), std::begin(mask.header().dims()),
                std::end(mask.header().dims())));

            found_type res({total(hdr_)});

            indexer_type gen(hdr_);
            typename Arrnd::indexer_type mask_gen(mask.header());

            typename found_type::indexer_type res_gen(res.header());

            size_type res_count{0};

            while (gen && mask_gen && res_gen) {
                if (mask[*mask_gen]) {
                    res[*res_gen] = *gen;
                    ++res_count;
                    ++res_gen;
                }
                ++gen;
                ++mask_gen;
            }

            if (res_count == 0) {
                return found_type();
            }

            if (res_count < total(hdr_)) {
                return res.resize({res_count});
            }

            return res;
        }
        template <std::int64_t Level, arrnd_type Arrnd>
            requires(Level > 0)
        [[nodiscard]] constexpr auto find(const Arrnd& mask) const
        {
            using found_type = inner_replaced_type<size_type, Level>;

            if (empty()) {
                return found_type();
            }

            found_type res(hdr_.dims().cbegin(), hdr_.dims().cend());

            indexer_type gen(hdr_);
            indexer_type res_gen(res.header());

            for (; gen && res_gen; ++gen, ++res_gen) {
                res[*res_gen] = (*this)[*gen].template find<Level - 1, Arrnd>(mask);
            }

            return res;
        }
        template <arrnd_type Arrnd>
        [[nodiscard]] constexpr auto find(const Arrnd& mask) const
        {
            return find<this_type::depth, Arrnd>(mask);
        }

        template <arrnd_type Arrnd>
            requires(this_type::is_flat && Arrnd::is_flat)
        [[nodiscard]] constexpr auto dot(const Arrnd& arr) const
        {
            using ret_type = replaced_type<decltype(value_type{} * (typename Arrnd::value_type{}))>;

            assert(hdr_.dims().size() >= 2 && arr.header().dims().size() >= 2);

            auto impl = [](const auto& lhs, const auto& rhs) {
                assert(ismatrix(lhs.header()) && ismatrix(rhs.header()));
                assert(lhs.header().dims().back() == rhs.header().dims().front());

                ret_type res({lhs.header().dims().front(), rhs.header().dims().back()});

                size_type ind = 0;
                auto trhs = rhs.transpose({1, 0});
                std::for_each(lhs.cbegin(arrnd_returned_slice_iterator_tag{}),
                    lhs.cend(arrnd_returned_slice_iterator_tag{}), [&res, &trhs, &ind](const auto& row) {
                        std::for_each(trhs.cbegin(arrnd_returned_slice_iterator_tag{}),
                            trhs.cend(arrnd_returned_slice_iterator_tag{}), [&res, &ind, &row](const auto& col) {
                                res[ind++] = (row * col).template reduce<0>(std::plus<>{});
                            });
                    });

                return res;
            };

            if (ismatrix(hdr_) && ismatrix(arr.header())) {
                return impl(*this, arr);
            }

            if (ismatrix(arr.header())) {
                return browse(2, [&arr, impl](auto page) {
                    return impl(page, arr);
                });
            } else {
                size_type lhs_num_pages
                    = total(hdr_) / ((*std::next(hdr_.dims().cbegin(), hdr_.dims().size() - 2)) * hdr_.dims().back());
                size_type rhs_num_pages = total(arr.header())
                    / ((*std::next(arr.header().dims().cbegin(), arr.header().dims().size() - 2))
                        * arr.header().dims().back());
                assert(lhs_num_pages == rhs_num_pages);

                auto arr_pages = arr.pages(2);
                typename decltype(arr_pages)::indexer_type arr_pages_gen(arr_pages.header());

                return browse(2, [&arr_pages, &arr_pages_gen, &impl](auto page) {
                    return impl(page, arr_pages[*(arr_pages_gen++)]);
                });
            }
        }

        [[nodiscard]] constexpr auto det() const
            requires(this_type::is_flat)
        {
            assert(hdr_.dims().size() >= 2);

            std::function<value_type(this_type)> det_impl;

            det_impl = [&](this_type arr) {
                assert(ismatrix(arr.header()));
                assert(arr.header().dims().front() == arr.header().dims().back());
                size_type n = arr.header().dims().front();

                if (n == 1) {
                    return arr(0);
                }

                if (n == 2) {
                    return arr(0) * arr(3) - arr(1) * arr(2);
                }

                value_type sign{1};
                value_type d{0};

                for (size_type j = 0; j < n; ++j) {
                    value_type p{arr[{0, j}]};
                    if (p != value_type{0}) {
                        d += sign * p
                            * det_impl(arr.exclude({0}, {0})
                                           .transform<0>([j](const auto& val) {
                                               return val.exclude({1}, {j});
                                           })
                                           .merge()
                                           .merge());
                    }
                    sign *= value_type{-1};
                }
                return d;
            };

            if (ismatrix(hdr_)) {
                return this_type({1}, det_impl(*this));
            }

            return browse(2, [det_impl](auto page) {
                return det_impl(page);
            });
        }

        [[nodiscard]] constexpr auto inv() const
            requires(this_type::is_flat)
        {
            assert(hdr_.dims().size() >= 2);

            std::function<this_type(this_type)> inv_impl;

            inv_impl = [&](this_type arr) {
                assert(ismatrix(arr.header()));
                assert(arr.header().dims().front() == arr.header().dims().back());

                value_type d = arr.det()(0);
                assert(d != value_type{0});
                size_type n = arr.header().dims().front();

                this_type res(arr.header().dims());

                for (size_type i = 0; i < n; ++i) {
                    value_type sign = (i + 1) % 2 == 0 ? value_type{-1} : value_type{1};
                    for (size_type j = 0; j < n; ++j) {

                        res[{i, j}] = sign
                            * arr.exclude({0}, {i})
                                  .transform<0>([j](const auto& val) {
                                      return val.exclude({1}, {j});
                                  })
                                  .merge()
                                  .merge()
                                  .det()(0);
                        sign *= value_type{-1};
                    }
                }

                return (value_type{1} / d) * res.transpose({1, 0});
            };

            if (ismatrix(hdr_)) {
                return inv_impl(*this);
            }

            return browse(2, [inv_impl](auto page) {
                return inv_impl(page);
            });
        }

        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr this_type transpose(const InputIt& first_order, const InputIt& last_order) const
        {
            if (empty()) {
                return this_type();
            }

            header_type new_header = oc::arrnd::transpose(header_type(hdr_.dims()), first_order, last_order);
            if (oc::arrnd::empty(new_header)) {
                return this_type();
            }

            this_type res({total(hdr_)});
            res.hdr_ = std::move(new_header);

            indexer_type gen(oc::arrnd::transpose(hdr_, first_order, last_order));
            indexer_type res_gen(res.hdr_);

            while (gen && res_gen) {
                res[*res_gen] = (*this)[*gen];
                ++gen;
                ++res_gen;
            }

            return res;
        }

        template <iterable_of_type_integral Cont>
        [[nodiscard]] constexpr this_type transpose(const Cont& order) const
        {
            return transpose(std::begin(order), std::end(order));
        }
        [[nodiscard]] constexpr this_type transpose(std::initializer_list<size_type> order) const
        {
            return transpose(order.begin(), order.end());
        }

        [[nodiscard]] constexpr this_type transpose() const
        {
            if (empty()) {
                return this_type();
            }

            this_type::template replaced_type<size_type> order({static_cast<size_type>(hdr_.dims().size())});
            std::iota(order.begin(), order.end(), size_type{0});

            if (total(order.header()) > 1) {
                std::swap(order[total(order.header()) - 1], order[total(order.header()) - 2]);
            }

            return transpose(order);
        }

        [[nodiscard]] constexpr auto expand(size_type axis, size_type division = 0,
            bool find_closest_axis_dim_bigger_than_one_to_the_left = false) const
        {
            using expanded_type = inner_replaced_type<inner_this_type<0>, 0>;

            if (empty()) {
                return expanded_type();
            }

            assert(axis >= 0 && axis < hdr_.dims().size());

            auto fixed_axis = axis;
            if (find_closest_axis_dim_bigger_than_one_to_the_left) {
                if (*std::next(hdr_.dims().cbegin(), fixed_axis) == 1) {
                    for (difference_type i = axis - 1; i >= 0; --i) {
                        if (*std::next(hdr_.dims().cbegin(), i) > 1) {
                            fixed_axis = i;
                            break;
                        }
                    }
                }
            }

            auto axis_dim = *std::next(hdr_.dims().cbegin(), fixed_axis);

            assert(division <= axis_dim);
            auto fixed_div = division > 0 ? std::min(axis_dim, division) : axis_dim;

            typename expanded_type::header_type::extent_storage_type new_dims(hdr_.dims().size());
            std::fill(new_dims.begin(), new_dims.end(), 1);
            *std::next(new_dims.begin(), fixed_axis) = fixed_div;

            expanded_type res(new_dims);
            typename expanded_type::indexer_type res_gen(res.header());

            auto curr_div = fixed_div;
            auto curr_axis_dim = axis_dim;
            auto curr_ival_width = static_cast<size_type>(std::ceil(curr_axis_dim / static_cast<double>(curr_div)));

            size_type count = 0;

            ranger_type rgr(
                hdr_, fixed_axis, window_type(window_interval_type(0, curr_ival_width), arrnd_window_type::partial));

            while (curr_div > 0) {
                res[*res_gen] = (*this)[std::make_pair((*rgr).cbegin(), (*rgr).cend())];

                rgr += curr_ival_width;
                ++res_gen;

                --curr_div;
                curr_axis_dim -= curr_ival_width;

                if (curr_div > 0) {
                    curr_ival_width = static_cast<size_type>(std::ceil(curr_axis_dim / static_cast<double>(curr_div)));
                    rgr.modify_window(
                        fixed_axis, window_type(window_interval_type(0, curr_ival_width), arrnd_window_type::partial));
                }

                ++count;
            }

            if (count != fixed_div) {
                return res.resize({count});
            }
            return res;
        }

        // collapse should only used on valid expanded arrays
        [[nodiscard]] constexpr auto collapse() const
            requires(!this_type::is_flat)
        {
            using collapsed_type = value_type;

            if (empty()) {
                return collapsed_type();
            }

            // from array creator

            bool all_nested_values_have_the_same_creator
                = std::adjacent_find(cbegin(), cend(),
                      [](const collapsed_type& vt1, const collapsed_type& vt2) {
                          return !vt1.creator() || !vt2.creator() || vt1.creator() != vt2.creator();
                      })
                == cend();

            if (all_nested_values_have_the_same_creator && (*this)(0).creator() != nullptr) {
                return *((*this)(0).creator());
            }

            // from one collapsed array

            if (total(hdr_) == 1) {
                return (*this)(0);
            }

            // from assumed axis hint

            assert(std::count(hdr_.dims().cbegin(), hdr_.dims().cend(), 1) == hdr_.dims().size() - 1);

            auto axis_dim_it = std::find(hdr_.dims().cbegin(), hdr_.dims().cend(), total(hdr_));
            auto assumed_axis = axis_dim_it - hdr_.dims().cbegin();

            indexer_type gen(hdr_);

            collapsed_type res = (*this)[*gen];
            ++gen;

            while (gen) {
                res = res.push_back((*this)[*gen], assumed_axis);
                ++gen;
            }

            return res;
        }

        template <typename Func>
            requires(invocable_no_arrnd<Func, inner_this_type<0>>)
        [[nodiscard]] constexpr auto slide(size_type axis, window_interval_type window, bool bounded, Func&& func) const
        {
            using func_res_type = std::invoke_result_t<Func, this_type>;
            using slide_type = replaced_type<func_res_type>;

            if (empty()) {
                return slide_type();
            }

            assert(axis >= 0 && axis < hdr_.dims().size());

            size_type axis_dim = *std::next(hdr_.dims().cbegin(), axis);

            ranger_type rgr(
                hdr_, axis, window_type(window, bounded ? arrnd_window_type::complete : arrnd_window_type::partial));

            size_type res_numel = bounded ? axis_dim - window.stop() + window.start() + 1 : axis_dim;

            slide_type res({res_numel});
            typename slide_type::indexer_type res_gen(res.header());

            for (; rgr && res_gen; ++rgr, ++res_gen) {
                res[*res_gen] = func((*this)[std::make_pair((*rgr).cbegin(), (*rgr).cend())]);
            }

            return res;
        }

        template <typename ReduceFunc, typename TransformFunc>
            requires(invocable_no_arrnd<TransformFunc, inner_this_type<0>>
                && invocable_no_arrnd<ReduceFunc, std::invoke_result_t<TransformFunc, inner_this_type<0>>,
                    std::invoke_result_t<TransformFunc, inner_this_type<0>>>)
        [[nodiscard]] constexpr auto accumulate(
            size_type axis, window_interval_type window, bool bounded, ReduceFunc&& rfunc, TransformFunc&& tfunc) const
        {
            using trans_res_type = std::invoke_result_t<TransformFunc, this_type>;
            using acc_res_type = std::invoke_result_t<ReduceFunc, trans_res_type, trans_res_type>;
            using accumulate_type = replaced_type<acc_res_type>;

            if (empty()) {
                return accumulate_type();
            }

            assert(axis >= 0 && axis < hdr_.dims().size());

            size_type axis_dim = *std::next(hdr_.dims().cbegin(), axis);

            ranger_type rgr(
                hdr_, axis, window_type(window, bounded ? arrnd_window_type::complete : arrnd_window_type::partial));

            size_type res_numel = bounded ? axis_dim - window.stop() + window.start() + 1 : axis_dim;

            accumulate_type res({res_numel});
            typename accumulate_type::indexer_type res_gen(res.header());

            if (res.empty()) {
                return res;
            }

            res[*res_gen] = tfunc((*this)[std::make_pair((*rgr).cbegin(), (*rgr).cend())]);
            auto prev = res[*res_gen];
            ++res_gen;
            ++rgr;

            for (; rgr && res_gen; ++rgr, ++res_gen) {
                res[*res_gen] = rfunc(prev, tfunc((*this)[std::make_pair((*rgr).cbegin(), (*rgr).cend())]));
                prev = res[*res_gen];
            }

            return res;
        }

        template <typename Func>
            requires(invocable_no_arrnd<Func, this_type>)
        constexpr auto browse(size_type page_size, Func&& func) const
        {
            constexpr bool is_void_func = std::is_same_v<std::invoke_result_t<Func, this_type>, void>;
            using func_res_type = std::conditional_t<is_void_func, this_type, std::invoke_result_t<Func, this_type>>;
            using returned_type = std::conditional_t<arrnd_type<func_res_type> && same_depth<func_res_type, this_type>,
                func_res_type, replaced_type<func_res_type>>;

            auto invoke_func = [&func](auto page) {
                if constexpr (arrnd_type<func_res_type> && same_depth<func_res_type, this_type>) {
                    if constexpr (is_void_func) {
                        func(page);
                        return page;
                    } else {
                        return func(page);
                    }
                } else { // in case that the returned type of func is not arrnd_type, then it should not be void returned type
                    return returned_type({1}, {func(page)});
                }
            };

            if (empty()) {
                return returned_type{};
            }

            assert(hdr_.dims().size() >= page_size);

            if (hdr_.dims().size() == page_size) {
                if constexpr (is_void_func) {
                    invoke_func(*this);
                    return *this;
                } else {

                    return invoke_func(*this);
                }
            }

            auto expanded = pages(page_size);

            using trans_expanded_type = typename decltype(expanded)::template replaced_type<returned_type>;

            trans_expanded_type trans_expanded{};
            if constexpr (std::is_same_v<decltype(expanded), trans_expanded_type>) {
                trans_expanded = expanded;
            } else {
                trans_expanded = trans_expanded_type(expanded.header().dims());
            }

            typename decltype(expanded)::indexer_type exp_gen(expanded.header());
            typename trans_expanded_type::indexer_type trs_gen(trans_expanded.header());

            for (; exp_gen && trs_gen; ++exp_gen, ++trs_gen) {
                auto page = expanded[*exp_gen];

                auto processed = invoke_func(page);

                trans_expanded[*trs_gen] = processed;
            }

            if constexpr (is_void_func) {
                return *this;
            } else {

                return trans_expanded.book();
            }
        }

        [[nodiscard]] constexpr typename this_type::template replaced_type<this_type> pages(
            size_type page_ndims = 2) const
        {
            assert(page_ndims > 0 && page_ndims <= std::ssize(hdr_.dims()));

            if (page_ndims == std::ssize(hdr_.dims())) {
                return typename this_type::template replaced_type<this_type>({1}, {*this});
            }

            typename this_type::template replaced_type<this_type> pages =
                typename this_type::template replaced_type<this_type>(
                    hdr_.dims().cbegin(), std::next(hdr_.dims().cbegin(), std::ssize(hdr_.dims()) - page_ndims));

            for (auto gen = decltype(pages)::indexer_type(pages.header()); gen; ++gen) {
                auto page = *this;

                const auto& subs = gen.subs();
                for (size_type axis = 0; axis < std::size(hdr_.dims()) - page_ndims; ++axis) {
                    page = page[interval_type::at(*std::next(subs.cbegin(), axis))];
                }

                pages[*gen] = page;
            }

            return pages;
        }

        [[nodiscard]] constexpr value_type book() const
        {
            assert(!this_type::is_flat);

            if (empty()) {
                return value_type{};
            }

            bool all_pages_source_known = std::all_of(cbegin(), cend(), [](const auto& page) {
                return page.creator() != nullptr;
            });

            bool all_pages_from_same_source = std::adjacent_find(cbegin(), cend(),
                                                  [](const auto& page1, const auto& page2) {
                                                      return page1.creator() != page2.creator();
                                                  })
                == cend();

            if (all_pages_source_known && all_pages_from_same_source) {
                return *(*this)[0].creator();
            }

            bool all_pages_with_same_dimensions
                = std::adjacent_find(cbegin(), cend(),
                      [](const auto& page1, const auto& page2) {
                          return !std::equal(page1.header().dims().cbegin(), page1.header().dims().cend(),
                              page2.header().dims().cbegin(), page2.header().dims().cend());
                      })
                == cend();

            assert(all_pages_with_same_dimensions);

            size_type this_ndims = std::ssize(hdr_.dims());
            size_type page_ndims = std::ssize((*this)[0].header().dims());

            typename header_type::extent_storage_type new_dims;
            new_dims = typename header_type::extent_storage_type(this_ndims + page_ndims);
            const auto& this_dims = hdr_.dims();
            const auto& page_dims = (*this)[0].header().dims();
            std::copy(this_dims.cbegin(), this_dims.cend(), new_dims.begin());
            std::copy(page_dims.cbegin(), page_dims.cend(), std::next(new_dims.begin(), this_ndims));

            value_type res(new_dims);
            indexer_type res_gen(res.header());

            for (const auto& page : *this) {
                for (indexer_type page_gen(page.header()); page_gen && res_gen; ++page_gen, ++res_gen) {
                    res[*res_gen] = page[*page_gen];
                }
            }

            return res;
        }

        template <iterator_of_type_integral AxesIt>
        [[nodiscard]] constexpr auto split(AxesIt first_axis, AxesIt last_axis, size_type division) const
        {
            using split_type = replaced_type<this_type>;

            if (empty()) {
                return split_type();
            }

            assert(std::is_sorted(first_axis, last_axis));
            assert(std::adjacent_find(first_axis, last_axis) == last_axis);
            assert(std::distance(first_axis, last_axis) >= 0
                && std::distance(first_axis, last_axis) <= hdr_.dims().size());
            assert(std::all_of(first_axis, last_axis, [&](size_type axis) {
                return axis >= 0 && axis < hdr_.dims().size();
            }));

            assert(std::all_of(hdr_.dims().cbegin(), hdr_.dims().cend(), [division](size_type d) {
                return division > 0 && division <= d;
            }));

            // calculate dimensions according to number of slices in each dimension

            typename header_type::extent_storage_type new_dims(hdr_.dims().size());
            if (std::distance(first_axis, last_axis) == 0) {
                std::fill(new_dims.begin(), new_dims.end(), division);
            } else {
                std::fill(new_dims.begin(), new_dims.end(), 1);
                std::for_each(first_axis, last_axis, [&](size_type axis) {
                    *std::next(new_dims.begin(), axis) = division;
                });
            }

            // --------------------------------------------------------------------

            split_type slices(new_dims);
            typename split_type::indexer_type slc_gen(slices.header());

            size_type actual_num_slices = 0;

            std::function<void(this_type, size_type)> split_impl;

            split_impl = [&](this_type arr, size_type current_depth) -> void {
                if (arr.empty()) {
                    return;
                }

                if (current_depth == 0) {
                    assert(static_cast<bool>(slc_gen));

                    slices[*slc_gen] = arr;
                    ++slc_gen;
                    ++actual_num_slices;
                    return;
                }

                if (std::distance(first_axis, last_axis) > 0
                    && std::find(first_axis, last_axis, arr.header().dims().size() - current_depth) == last_axis) {
                    split_impl(
                        arr(interval_type::full(), arr.header().dims().size() - current_depth), current_depth - 1);
                    return;
                }

                size_type current_dim
                    = *std::next(arr.header().dims().cbegin(), arr.header().dims().size() - current_depth);

                auto exp = arr.expand(arr.header().dims().size() - current_depth, division);
                typename decltype(exp)::indexer_type exp_gen(exp.header());

                for (; exp_gen; ++exp_gen) {
                    split_impl(exp[*exp_gen], current_depth - 1);
                }
            };

            split_impl(*this, hdr_.dims().size());

            assert(actual_num_slices == total(slices.header()));

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
            using split_type = replaced_type<this_type>;

            if (empty()) {
                return split_type();
            }

            assert(std::is_sorted(first_ind, last_ind));
            assert(std::adjacent_find(first_ind, last_ind) == last_ind);

            assert(std::is_sorted(first_axis, last_axis));
            assert(std::adjacent_find(first_axis, last_axis) == last_axis);
            assert(std::distance(first_axis, last_axis) >= 0
                && std::distance(first_axis, last_axis) <= hdr_.dims().size());
            assert(std::all_of(first_axis, last_axis, [&](size_type axis) {
                return axis >= 0 && axis < hdr_.dims().size();
            }));

            // calculate dimensions according to number of slices in each dimension

            typename header_type::extent_storage_type new_dims(hdr_.dims().size());
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
                    *std::next(new_dims.begin(), axis) = calc_num_slices_at_dim(first_ind, last_ind);
                });
            }

            // --------------------------------------------------------------------

            split_type slices(new_dims);
            typename split_type::indexer_type slc_gen(slices.header());

            size_type actual_num_slices = 0;

            std::function<void(this_type, size_type)> split_impl;

            split_impl = [&](this_type arr, size_type current_depth) -> void {
                if (arr.empty()) {
                    return;
                }

                if (current_depth == 0) {
                    assert(static_cast<bool>(slc_gen));

                    slices[*slc_gen] = arr;
                    ++slc_gen;
                    ++actual_num_slices;
                    return;
                }

                if (std::distance(first_axis, last_axis) > 0
                    && std::find(first_axis, last_axis, arr.header().dims().size() - current_depth) == last_axis) {
                    split_impl(
                        arr(interval_type::full(), arr.header().dims().size() - current_depth), current_depth - 1);
                    return;
                }

                size_type current_dim
                    = *std::next(arr.header().dims().cbegin(), arr.header().dims().size() - current_depth);

                assert(std::distance(first_ind, last_ind) > 0 && std::distance(first_ind, last_ind) <= current_dim);
                assert(std::all_of(first_ind, last_ind, [current_dim](size_type ind) {
                    return ind >= 0 && ind < current_dim;
                }));

                size_type prev_ind = *first_ind;
                auto ind_it = first_ind;
                ++ind_it;
                if (prev_ind > 0) {
                    split_impl(
                        arr(interval_type{0, prev_ind}, arr.header().dims().size() - current_depth), current_depth - 1);
                }
                size_type current_ind = prev_ind;
                for (; ind_it != last_ind; ++ind_it) {
                    current_ind = *ind_it;
                    if (prev_ind < current_ind) {
                        split_impl(
                            arr(interval_type{prev_ind, current_ind}, arr.header().dims().size() - current_depth),
                            current_depth - 1);
                    }
                    prev_ind = current_ind;
                }
                if (current_ind < current_dim) {
                    split_impl(arr(interval_type{current_ind, current_dim}, arr.header().dims().size() - current_depth),
                        current_depth - 1);
                }
            };

            split_impl(*this, hdr_.dims().size());

            assert(actual_num_slices == total(slices.header()));

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
            using exclude_type = replaced_type<this_type>;

            if (empty()) {
                return exclude_type();
            }

            assert(std::is_sorted(first_ind, last_ind));
            assert(std::adjacent_find(first_ind, last_ind) == last_ind);

            assert(std::is_sorted(first_axis, last_axis));
            assert(std::adjacent_find(first_axis, last_axis) == last_axis);
            assert(std::distance(first_axis, last_axis) >= 0
                && std::distance(first_axis, last_axis) <= hdr_.dims().size());
            assert(std::all_of(first_axis, last_axis, [&](size_type axis) {
                return axis >= 0 && axis < hdr_.dims().size();
            }));

            // calculate dimensions according to number of slices in each dimension

            typename header_type::extent_storage_type new_dims(hdr_.dims().size());
            std::fill(new_dims.begin(), new_dims.end(), 1);
            auto calc_num_slices_at_dim = [&](auto indf, auto indl, size_type dim) {
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
            if (std::distance(first_axis, last_axis) == 0) {
                std::transform(hdr_.dims().cbegin(), hdr_.dims().cend(), new_dims.begin(), [&](size_type dim) {
                    return calc_num_slices_at_dim(first_ind, last_ind, dim);
                });
            } else {
                std::for_each(first_axis, last_axis, [&](size_type axis) {
                    *std::next(new_dims.begin(), axis)
                        = calc_num_slices_at_dim(first_ind, last_ind, *std::next(hdr_.dims().cbegin(), axis));
                });
            }

            // --------------------------------------------------------------------

            exclude_type slices(new_dims);
            typename exclude_type::indexer_type slc_gen(slices.header());

            size_type actual_num_slices = 0;

            std::function<void(this_type, size_type)> exclude_impl;

            exclude_impl = [&](this_type arr, size_type current_depth) -> void {
                if (arr.empty()) {
                    return;
                }

                if (current_depth == 0) {
                    assert(static_cast<bool>(slc_gen));

                    slices[*slc_gen] = arr;
                    ++slc_gen;
                    ++actual_num_slices;

                    return;
                }

                if (std::distance(first_axis, last_axis) > 0
                    && std::find(first_axis, last_axis, arr.header().dims().size() - current_depth) == last_axis) {
                    exclude_impl(
                        arr(interval_type::full(), arr.header().dims().size() - current_depth), current_depth - 1);
                    return;
                }

                size_type current_dim
                    = *std::next(arr.header().dims().cbegin(), arr.header().dims().size() - current_depth);

                assert(std::distance(first_ind, last_ind) > 0 && std::distance(first_ind, last_ind) <= current_dim);
                assert(std::all_of(first_ind, last_ind, [current_dim](size_type ind) {
                    return ind >= 0 && ind < current_dim;
                }));

                size_type prev_ind = *first_ind;
                auto ind_it = first_ind;
                ++ind_it;
                if (prev_ind > 0) {
                    exclude_impl(
                        arr(interval_type{0, prev_ind}, arr.header().dims().size() - current_depth), current_depth - 1);
                }
                size_type current_ind = prev_ind;
                for (; ind_it != last_ind; ++ind_it) {
                    current_ind = *ind_it;
                    if (prev_ind + 1 < current_ind) {
                        exclude_impl(
                            arr(interval_type{prev_ind + 1, current_ind}, arr.header().dims().size() - current_depth),
                            current_depth - 1);
                    }
                    prev_ind = current_ind;
                }
                if (current_ind + 1 < current_dim) {
                    exclude_impl(
                        arr(interval_type{current_ind + 1, current_dim}, arr.header().dims().size() - current_depth),
                        current_depth - 1);
                }
            };

            exclude_impl(*this, hdr_.dims().size());

            assert(actual_num_slices == total(slices.header()));

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

        [[nodiscard]] constexpr value_type merge() const
            requires(!this_type::is_flat)
        {
            if (empty()) {
                return value_type();
            }

            this_type res = reduce<0>(hdr_.dims().size() - 1, [&](const value_type& a, const value_type& b) {
                return a.push_back(b, hdr_.dims().size() - 1);
            });

            for (difference_type axis = hdr_.dims().size() - 2; axis >= 0; --axis) {
                res = res.reduce<0>(axis, [axis](const value_type& a, const value_type& b) {
                    return a.push_back(b, axis);
                });
            }

            assert(total(res.header()) == 1);

            return res(0);
        }

        [[nodiscard]] constexpr this_type squeeze() const
        {
            this_type squeezed{};
            squeezed.hdr_ = oc::arrnd::squeeze(hdr_, arrnd_squeeze_type::full);
            squeezed.buffsp_ = buffsp_;
            squeezed.is_creator_valid_ = original_valid_creator_;
            squeezed.creator_ = this;
            return squeezed;
        }

        [[nodiscard]] constexpr this_type zeros() const
        {
            return oc::arrnd::details::zeros<this_type>(hdr_.dims());
        }

        [[nodiscard]] constexpr this_type eye() const
        {
            return oc::arrnd::details::eye<this_type>(hdr_.dims());
        }

        template <typename Comp>
            requires(invocable_no_arrnd<Comp, inner_value_type<0>, inner_value_type<0>>)
        [[nodiscard]] constexpr this_type sort(Comp&& comp) const
        {
            if (empty()) {
                return this_type();
            }

            this_type res = clone();

            std::sort(res.begin(), res.end(), [&comp](const auto& lhs, const auto& rhs) {
                return comp(lhs, rhs);
            });

            return res;
        }

        template <typename Comp>
            requires(invocable_no_arrnd<Comp, inner_this_type<0>, inner_this_type<0>>)
        [[nodiscard]] constexpr this_type sort(size_type axis, Comp&& comp) const
        {
            if (empty()) {
                return this_type();
            }

            assert(axis >= 0 && axis < hdr_.dims().size());

            auto expanded = expand(axis);

            auto sorted = expanded.sort(std::forward<Comp>(comp));

            auto reduced = sorted.template reduce<0>([axis](const auto& acc, const auto& cur) {
                return acc.push_back(cur, axis);
            });

            return reduced.reshape(hdr_.dims());
        }

        template <typename Comp>
            requires(invocable_no_arrnd<Comp, inner_value_type<0>, inner_value_type<0>>)
        [[nodiscard]] constexpr bool is_sorted(Comp&& comp) const
        {
            if (empty()) {
                return true;
            }

            return std::is_sorted(cbegin(), cend(), [&comp](const auto& lhs, const auto& rhs) {
                return comp(lhs, rhs);
            });
        }

        template <typename Comp>
            requires(invocable_no_arrnd<Comp, inner_this_type<0>, inner_this_type<0>>)
        [[nodiscard]] constexpr bool is_sorted(size_type axis, Comp&& comp) const
        {
            if (empty()) {
                return true;
            }

            assert(axis >= 0 && axis < hdr_.dims().size());

            auto expanded = expand(axis);

            return expanded.is_sorted(std::forward<Comp>(comp));
        }

        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr this_type reorder(const InputIt& first_order, const InputIt& last_order) const
        {
            if (empty()) {
                return this_type();
            }

            assert(std::distance(first_order, last_order) == total(hdr_));

            std::initializer_list<size_type> res_dims{static_cast<size_type>(std::distance(first_order, last_order))};
            replaced_type<size_type> order(res_dims.begin(), res_dims.end(), first_order, last_order);

            auto reordered = clone();

            auto z = zip(zipped_container(order), zipped_container(reordered));
            std::sort(z.begin(), z.end(), [](const auto& t1, const auto& t2) {
                return std::get<0>(t1) < std::get<0>(t2);
            });

            return reordered;
        }

        template <iterable_of_type_integral Cont>
        [[nodiscard]] constexpr auto reorder(const Cont& order) const
        {
            return reorder(std::begin(order), std::end(order));
        }
        [[nodiscard]] constexpr auto reorder(std::initializer_list<size_type> order) const
        {
            return reorder(order.begin(), order.end());
        }

        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr this_type reorder(
            size_type axis, const InputIt& first_order, const InputIt& last_order) const
        {
            if (empty()) {
                return this_type();
            }

            assert(axis >= 0 && axis < hdr_.dims().size());
            assert(std::distance(first_order, last_order) == *std::next(hdr_.dims().cbegin(), axis));

            std::initializer_list<size_type> res_dims{static_cast<size_type>(std::distance(first_order, last_order))};
            replaced_type<size_type> order(res_dims.begin(), res_dims.end(), first_order, last_order);

            auto expanded = expand(axis);

            auto z = zip(zipped_container(order), zipped_container(expanded));
            std::sort(z.begin(), z.end(), [](const auto& t1, const auto& t2) {
                return std::get<0>(t1) < std::get<0>(t2);
            });

            auto reordered = expanded.template reduce<0>([axis](const auto& acc, const auto& cur) {
                return acc.push_back(cur, axis);
            });

            return reordered;
        }

        template <iterable_of_type_integral Cont>
        [[nodiscard]] constexpr auto reorder(size_type axis, const Cont& order) const
        {
            return reorder(axis, std::begin(order), std::end(order));
        }
        [[nodiscard]] constexpr auto reorder(size_type axis, std::initializer_list<size_type> order) const
        {
            return reorder(axis, order.begin(), order.end());
        }

        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto find_adjacents(
            const InputIt& first_sub, const InputIt& last_sub, difference_type offset = 1) const
        {
            using returned_type = replaced_type<size_type>;

            if (empty()) {
                return returned_type();
            }

            assert(std::distance(first_sub, last_sub) == std::ssize(hdr_.dims()));
            assert(offset > 0);

            auto compute_num_adj = [](size_type ndims, size_type offset) {
                size_type base1 = 3 + 2 * (offset - 1);
                size_type base2 = 3 + 2 * (offset - 2);
                return static_cast<size_type>(std::pow(base1, ndims) - std::pow(base2, ndims));
            };

            returned_type res({compute_num_adj(std::ssize(hdr_.dims()), offset)});
            size_type actual_num_adj = 0;

            std::function<void(returned_type, size_type, bool)> impl;

            impl = [&](returned_type subs, size_type perm_pos, bool used_offset) {
                if (perm_pos == std::ssize(hdr_.dims())) {
                    return;
                }

                for (difference_type i = -offset; i <= offset; ++i) {
                    size_type abs_i = static_cast<size_type>(std::abs(i));

                    if (abs_i > 0 && abs_i <= offset) {
                        auto new_subs = subs.clone();
                        new_subs[perm_pos] += i;
                        if (new_subs[perm_pos] >= 0
                            && new_subs[perm_pos] < *std::next(hdr_.dims().cbegin(), perm_pos)) {
                            if (used_offset || abs_i == offset) {
                                res[actual_num_adj++] = sub2ind(hdr_, new_subs.cbegin(), new_subs.cend());
                            }
                            impl(new_subs, perm_pos + 1, abs_i == offset);
                        }
                    } else {
                        impl(subs, perm_pos + 1, used_offset);
                    }
                }
            };

            std::initializer_list<size_type> res_dims{static_cast<size_type>(std::distance(first_sub, last_sub))};
            impl(returned_type(res_dims.begin(), res_dims.end(), first_sub, last_sub), 0, false);

            if (total(res.header()) > actual_num_adj) {
                return res.resize({actual_num_adj});
            }
            return res;
        }

        template <iterable_of_type_integral Cont>
        [[nodiscard]] constexpr auto find_adjacents(const Cont& subs, difference_type offset = 1) const
        {
            return find_adjacents(std::begin(subs), std::end(subs), offset);
        }
        [[nodiscard]] constexpr auto find_adjacents(
            std::initializer_list<size_type> subs, difference_type offset = 1) const
        {
            return find_adjacents(subs.begin(), subs.end(), offset);
        }

        template <std::int64_t Level, typename Pred>
            requires(Level == 0 && invocable_no_arrnd<Pred, inner_value_type<Level>>)
        [[nodiscard]] constexpr bool all(Pred&& pred) const
        {
            if (empty()) {
                return true;
            }

            for (indexer_type gen(hdr_); gen; ++gen) {
                if (!pred((*this)[*gen])) {
                    return false;
                }
            }

            return true;
        }

        template <std::int64_t Level, typename Pred>
            requires(Level > 0 && invocable_no_arrnd<Pred, inner_value_type<Level>>)
        [[nodiscard]] constexpr bool all(Pred&& pred) const
        {
            if (empty()) {
                return true;
            }

            for (indexer_type gen(hdr_); gen; ++gen) {
                if (!(*this)[*gen].template all<Level - 1, Pred>(std::forward<Pred>(pred))) {
                    return false;
                }
            }

            return true;
        }

        template <typename Pred>
            requires invocable_no_arrnd<Pred, inner_value_type<this_type::depth>>
        [[nodiscard]] constexpr bool all(Pred&& pred) const
        {
            return all<this_type::depth, Pred>(std::forward<Pred>(pred));
        }

        template <std::int64_t Level = this_type::depth>
        [[nodiscard]] constexpr bool all() const
        {
            return all<this_type::depth>([](const auto& value) {
                return static_cast<bool>(value);
            });
        }

        template <std::int64_t Level, arrnd_type Arrnd, typename Pred>
            requires(Level == 0
                && invocable_no_arrnd<Pred, inner_value_type<Level>, typename Arrnd::template inner_value_type<Level>>)
        [[nodiscard]] constexpr bool all_match(const Arrnd& arr, Pred&& pred) const
        {
            if (empty() && arr.empty()) {
                return true;
            }

            if (empty() || arr.empty()) {
                return false;
            }

            if (!std::equal(std::begin(hdr_.dims()), std::end(hdr_.dims()), std::begin(arr.header().dims()),
                    std::end(arr.header().dims()))) {
                return false;
            }

            indexer_type gen(hdr_);
            typename Arrnd::indexer_type arr_gen(arr.header());

            for (; gen && arr_gen; ++gen, ++arr_gen) {
                if (!pred((*this)[*gen], arr[*arr_gen])) {
                    return false;
                }
            }

            return true;
        }

        template <std::int64_t Level, arrnd_type Arrnd, typename Pred>
            requires(Level > 0
                && invocable_no_arrnd<Pred, inner_value_type<Level>, typename Arrnd::template inner_value_type<Level>>)
        [[nodiscard]] constexpr bool all_match(const Arrnd& arr, Pred&& pred) const
        {
            if (empty() && arr.empty()) {
                return true;
            }

            if (empty() || arr.empty()) {
                return false;
            }

            if (!std::equal(std::begin(hdr_.dims()), std::end(hdr_.dims()), std::begin(arr.header().dims()),
                    std::end(arr.header().dims()))) {
                return false;
            }

            indexer_type gen(hdr_);
            typename Arrnd::indexer_type arr_gen(arr.header());

            for (; gen && arr_gen; ++gen, ++arr_gen) {
                if (!(*this)[*gen].template all_match<Level - 1, typename Arrnd::value_type, Pred>(
                        arr[*arr_gen], std::forward<Pred>(pred))) {
                    return false;
                }
            }

            return true;
        }

        template <arrnd_type Arrnd, typename Pred>
            requires invocable_no_arrnd<Pred, inner_value_type<this_type::depth>,
                typename Arrnd::template inner_value_type<Arrnd::depth>>
        [[nodiscard]] constexpr bool all_match(const Arrnd& arr, Pred&& pred) const
        {
            return all_match<this_type::depth, Pred, Arrnd>(arr, std::forward<Pred>(pred));
        }

        template <std::int64_t Level, arrnd_type Arrnd>
        [[nodiscard]] constexpr bool all_match(const Arrnd& arr) const
        {
            return all_match<Level, Arrnd>(arr, [](const auto& a, const auto& b) {
                if constexpr (arrnd_type<decltype(a)> && arrnd_type<decltype(b)>) {
                    return (a == b).template all<this_type::depth>();
                } else {
                    return a == b;
                }
            });
        }

        template <arrnd_type Arrnd>
        [[nodiscard]] constexpr bool all_match(const Arrnd& arr) const
        {
            return all_match<this_type::depth, Arrnd>(arr);
        }

        template <std::int64_t Level, typename U, typename Pred>
            requires(!arrnd_type<U> && invocable_no_arrnd<Pred, inner_value_type<Level>, U>)
        [[nodiscard]] constexpr bool all_match(const U& u, Pred&& pred) const
        {
            return all<Level>([&u, &pred](const auto& a) {
                return pred(a, u);
            });
        }

        template <typename U, typename Pred>
            requires(!arrnd_type<U> && invocable_no_arrnd<Pred, inner_value_type<this_type::depth>, U>)
        [[nodiscard]] constexpr bool all_match(const U& u, Pred&& pred) const
        {
            return all_match<this_type::depth, U, Pred>(u, std::forward<Pred>(pred));
        }

        template <std::int64_t Level, typename U>
            requires(!arrnd_type<U>)
        [[nodiscard]] constexpr bool all_match(const U& u) const
        {
            return all<Level>([&u](const auto& a) {
                if constexpr (arrnd_type<decltype(a)>) {
                    return (a == u).template all<this_type::depth>();
                } else {
                    return a == u;
                }
            });
        }

        template <typename U>
            requires(!arrnd_type<U>)
        [[nodiscard]] constexpr bool all_match(const U& u) const
        {
            return all_match<this_type::depth, U>(u);
        }

        template <std::int64_t Level, typename Pred>
            requires(Level == 0 && invocable_no_arrnd<Pred, inner_value_type<Level>>)
        [[nodiscard]] constexpr bool any(Pred&& pred) const
        {
            if (empty()) {
                return true;
            }

            for (indexer_type gen(hdr_); gen; ++gen) {
                if (pred((*this)[*gen])) {
                    return true;
                }
            }

            return false;
        }

        template <std::int64_t Level, typename Pred>
            requires(Level > 0 && invocable_no_arrnd<Pred, inner_value_type<Level>>)
        [[nodiscard]] constexpr bool any(Pred&& pred) const
        {
            if (empty()) {
                return true;
            }

            for (indexer_type gen(hdr_); gen; ++gen) {
                if ((*this)[*gen].template any<Level - 1, Pred>(std::forward<Pred>(pred))) {
                    return true;
                }
            }

            return false;
        }

        template <typename Pred>
            requires invocable_no_arrnd<Pred, inner_value_type<this_type::depth>>
        [[nodiscard]] constexpr bool any(Pred&& pred) const
        {
            return any<this_type::depth, Pred>(std::forward<Pred>(pred));
        }

        template <std::int64_t Level = this_type::depth>
        [[nodiscard]] constexpr bool any() const
        {
            return any<this_type::depth>([](const auto& value) {
                return static_cast<bool>(value);
            });
        }

        template <std::int64_t Level, arrnd_type Arrnd, typename Pred>
            requires(Level == 0
                && invocable_no_arrnd<Pred, inner_value_type<Level>, typename Arrnd::template inner_value_type<Level>>)
        [[nodiscard]] constexpr bool any_match(const Arrnd& arr, Pred&& pred) const
        {
            if (empty() && arr.empty()) {
                return true;
            }

            if (empty() || arr.empty()) {
                return false;
            }

            if (!std::equal(std::begin(hdr_.dims()), std::end(hdr_.dims()), std::begin(arr.header().dims()),
                    std::end(arr.header().dims()))) {
                return false;
            }

            indexer_type gen(hdr_);
            typename Arrnd::indexer_type arr_gen(arr.header());

            for (; gen && arr_gen; ++gen, ++arr_gen) {
                if (pred((*this)[*gen], arr[*arr_gen])) {
                    return true;
                }
            }

            return false;
        }

        template <std::int64_t Level, arrnd_type Arrnd, typename Pred>
            requires(Level > 0
                && invocable_no_arrnd<Pred, inner_value_type<Level>, typename Arrnd::template inner_value_type<Level>>)
        [[nodiscard]] constexpr bool any_match(const Arrnd& arr, Pred&& pred) const
        {
            if (empty() && arr.empty()) {
                return true;
            }

            if (empty() || arr.empty()) {
                return false;
            }

            if (!std::equal(std::begin(hdr_.dims()), std::end(hdr_.dims()), std::begin(arr.header().dims()),
                    std::end(arr.header().dims()))) {
                return false;
            }

            indexer_type gen(hdr_);
            typename Arrnd::indexer_type arr_gen(arr.header());

            for (; gen && arr_gen; ++gen, ++arr_gen) {
                if ((*this)[*gen].template any_match<Level - 1, typename Arrnd::value_type, Pred>(
                        arr[*arr_gen], std::forward<Pred>(pred))) {
                    return true;
                }
            }

            return false;
        }

        template <arrnd_type Arrnd, typename Pred>
            requires invocable_no_arrnd<Pred, inner_value_type<this_type::depth>,
                typename Arrnd::template inner_value_type<Arrnd::depth>>
        [[nodiscard]] constexpr bool any_match(const Arrnd& arr, Pred&& pred) const
        {
            return any_match<this_type::depth, Pred, Arrnd>(arr, std::forward<Pred>(pred));
        }

        template <std::int64_t Level, arrnd_type Arrnd>
        [[nodiscard]] constexpr bool any_match(const Arrnd& arr) const
        {
            return any_match<Level, Arrnd>(arr, [](const auto& a, const auto& b) {
                if constexpr (arrnd_type<decltype(a)> && arrnd_type<decltype(b)>) {
                    return (a == b).template any<this_type::depth>();
                } else {
                    return a == b;
                }
            });
        }

        template <arrnd_type Arrnd>
        [[nodiscard]] constexpr bool any_match(const Arrnd& arr) const
        {
            return any_match<this_type::depth, Arrnd>(arr);
        }

        template <std::int64_t Level, typename U, typename Pred>
            requires(!arrnd_type<U> && invocable_no_arrnd<Pred, inner_value_type<Level>, U>)
        [[nodiscard]] constexpr bool any_match(const U& u, Pred&& pred) const
        {
            return any<Level>([&u, &pred](const auto& a) {
                return pred(a, u);
            });
        }

        template <typename U, typename Pred>
            requires(!arrnd_type<U> && invocable_no_arrnd<Pred, inner_value_type<this_type::depth>, U>)
        [[nodiscard]] constexpr bool any_match(const U& u, Pred&& pred) const
        {
            return any_match<this_type::depth, U, Pred>(u, std::forward<Pred>(pred));
        }

        template <std::int64_t Level, typename U>
            requires(!arrnd_type<U>)
        [[nodiscard]] constexpr bool any_match(const U& u) const
        {
            return any<Level>([&u](const auto& a) {
                if constexpr (arrnd_type<decltype(a)>) {
                    return (a == u).template any<this_type::depth>();
                } else {
                    return a == u;
                }
            });
        }

        template <typename U>
            requires(!arrnd_type<U>)
        [[nodiscard]] constexpr bool any_match(const U& u) const
        {
            return any_match<this_type::depth, U>(u);
        }

        template <std::int64_t Level, typename Pred>
            requires(invocable_no_arrnd<Pred, inner_value_type<Level>>)
        [[nodiscard]] constexpr replaced_type<bool> all(size_type axis, Pred&& pred) const
        {
            return reduce<Level>(axis, [&pred](const auto& a, const auto& b) {
                return a && pred(b);
            });
        }

        template <typename Pred>
            requires(invocable_no_arrnd<Pred, inner_value_type<this_type::depth>>)
        [[nodiscard]] constexpr replaced_type<bool> all(size_type axis, Pred&& pred) const
        {
            return all<this_type::depth>(axis, std::forward<Pred>(pred));
        }

        template <std::int64_t Level = this_type::depth>
        [[nodiscard]] constexpr replaced_type<bool> all(size_type axis) const
        {
            return all(axis, [](const auto& a) {
                return a;
            });
        }

        template <std::int64_t Level, typename Pred>
            requires(invocable_no_arrnd<Pred, inner_value_type<Level>>)
        [[nodiscard]] constexpr replaced_type<bool> any(size_type axis, Pred&& pred) const
        {
            return reduce<Level>(axis, [&pred](const auto& a, const auto& b) {
                return a || pred(b);
            });
        }

        template <typename Pred>
            requires(invocable_no_arrnd<Pred, inner_value_type<this_type::depth>>)
        [[nodiscard]] constexpr replaced_type<bool> any(size_type axis, Pred&& pred) const
        {
            return any<this_type::depth>(axis, std::forward<Pred>(pred));
        }

        template <std::int64_t Level = this_type::depth>
        [[nodiscard]] constexpr replaced_type<bool> any(size_type axis) const
        {
            return any(axis, [](const auto& a) {
                return a;
            });
        }

        template <std::int64_t Level = this_type::depth>
        [[nodiscard]] constexpr auto sum() const
        {
            return reduce<Level>([](const auto& a, const auto& b) {
                return a + b;
            });
        }

        template <std::int64_t Level = this_type::depth>
        [[nodiscard]] constexpr auto sum(size_type axis) const
        {
            return reduce<Level>(axis, [](const auto& a, const auto& b) {
                return a + b;
            });
        }

        template <std::int64_t Level = this_type::depth>
        [[nodiscard]] constexpr auto min() const
        {
            return reduce<Level>([](const auto& a, const auto& b) {
                using std::min;
                return min(a, b);
            });
        }

        template <std::int64_t Level = this_type::depth>
        [[nodiscard]] constexpr auto min(size_type axis) const
        {
            return reduce<Level>(axis, [](const auto& a, const auto& b) {
                using std::min;
                return min(a, b);
            });
        }

        template <std::int64_t Level = this_type::depth>
        [[nodiscard]] constexpr auto max() const
        {
            return reduce<Level>([](const auto& a, const auto& b) {
                using std::max;
                return max(a, b);
            });
        }

        template <std::int64_t Level = this_type::depth>
        [[nodiscard]] constexpr auto max(size_type axis) const
        {
            return reduce<Level>(axis, [](const auto& a, const auto& b) {
                using std::max;
                return max(a, b);
            });
        }

        template <std::int64_t Level = this_type::depth>
        [[nodiscard]] constexpr auto prod() const
        {
            return reduce<Level>([](const auto& a, const auto& b) {
                return a * b;
            });
        }

        template <std::int64_t Level = this_type::depth>
        [[nodiscard]] constexpr auto prod(size_type axis) const
        {
            return reduce<Level>(axis, [](const auto& a, const auto& b) {
                return a * b;
            });
        }

        template <arrnd_type Arrnd>
        [[nodiscard]] constexpr replaced_type<bool> close(const Arrnd& arr,
            const compliant_tol_type<Arrnd, this_type::depth>& atol
            = default_atol<compliant_tol_type<Arrnd, this_type::depth>>(),
            const compliant_tol_type<Arrnd, this_type::depth>& rtol
            = default_rtol<compliant_tol_type<Arrnd, this_type::depth>>()) const
        {
            return transform<this_type::depth>(arr, [&atol, &rtol](const auto& a, const auto& b) {
                return details::close(a, b, atol, rtol);
            });
        }

        template <typename U>
            requires(!arrnd_type<U>)
        [[nodiscard]] constexpr replaced_type<bool> close(const U& value,
            const tol_type<U, this_type::depth>& atol = default_atol<tol_type<U, this_type::depth>>(),
            const tol_type<U, this_type::depth>& rtol = default_rtol<tol_type<U, this_type::depth>>()) const
        {
            return transform<this_type::depth>([&atol, &rtol, &value](const auto& a) {
                return details::close(a, value, atol, rtol);
            });
        }

        template <arrnd_type Arrnd>
        [[nodiscard]] constexpr bool all_equal(const Arrnd& arr) const
        {
            return all_match<this_type::depth>(arr);
        }

        template <typename U>
        [[nodiscard]] constexpr bool all_equal(const U& u) const
        {
            return all<this_type::depth>([&u](const auto& a) {
                return a == u;
            });
        }

        template <arrnd_type Arrnd>
        [[nodiscard]] constexpr bool all_close(const Arrnd& arr,
            const compliant_tol_type<Arrnd, this_type::depth>& atol
            = default_atol<compliant_tol_type<Arrnd, this_type::depth>>(),
            const compliant_tol_type<Arrnd, this_type::depth>& rtol
            = default_rtol<compliant_tol_type<Arrnd, this_type::depth>>()) const
        {
            return all_match<this_type::depth>(arr, [&atol, &rtol](const auto& a, const auto& b) {
                return details::close(a, b, atol, rtol);
            });
        }

        template <typename U>
            requires(!arrnd_type<U>)
        [[nodiscard]] constexpr bool all_close(const U& u,
            const tol_type<U, this_type::depth>& atol = default_atol<tol_type<U, this_type::depth>>(),
            const tol_type<U, this_type::depth>& rtol = default_rtol<tol_type<U, this_type::depth>>()) const
        {
            return all<this_type::depth>([&u, &atol, &rtol](const auto& a) {
                return details::close(a, u, atol, rtol);
            });
        }

        template <arrnd_type Arrnd>
        [[nodiscard]] constexpr bool any_equal(const Arrnd& arr) const
        {
            return any_match<this_type::depth>(arr);
        }

        template <typename U>
        [[nodiscard]] constexpr bool any_equal(const U& u) const
        {
            return any<this_type::depth>([&u](const auto& a) {
                return a == u;
            });
        }

        template <arrnd_type Arrnd>
        [[nodiscard]] constexpr bool any_close(const Arrnd& arr,
            const compliant_tol_type<Arrnd, this_type::depth>& atol
            = default_atol<compliant_tol_type<Arrnd, this_type::depth>>(),
            const compliant_tol_type<Arrnd, this_type::depth>& rtol
            = default_rtol<compliant_tol_type<Arrnd, this_type::depth>>()) const
        {
            return any_match<this_type::depth>(arr, [&atol, &rtol](const auto& a, const auto& b) {
                return details::close(a, b, atol, rtol);
            });
        }

        template <typename U>
            requires(!arrnd_type<U>)
        [[nodiscard]] constexpr bool any_close(const U& u,
            const tol_type<U, this_type::depth>& atol = default_atol<tol_type<U, this_type::depth>>(),
            const tol_type<U, this_type::depth>& rtol = default_rtol<tol_type<U, this_type::depth>>()) const
        {
            return any<this_type::depth>([&u, &atol, &rtol](const auto& a) {
                return details::close(a, u, atol, rtol);
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
                : iterator(buffsp_->data(), indexer_type(move(hdr_, axis, 0), arrnd_iterator_position::begin));
        }
        [[nodiscard]] constexpr auto begin(size_type axis, arrnd_returned_element_iterator_tag) const
        {
            return empty()
                ? iterator()
                : iterator(buffsp_->data(), indexer_type(move(hdr_, axis, 0), arrnd_iterator_position::begin));
        }
        [[nodiscard]] constexpr auto cbegin(size_type axis, arrnd_returned_element_iterator_tag) const
        {
            return empty()
                ? const_iterator()
                : const_iterator(buffsp_->data(), indexer_type(move(hdr_, axis, 0), arrnd_iterator_position::begin));
        }
        [[nodiscard]] constexpr auto end(size_type axis, arrnd_returned_element_iterator_tag)
        {
            return empty() ? iterator()
                           : iterator(buffsp_->data(), indexer_type(move(hdr_, axis, 0), arrnd_iterator_position::end));
        }
        [[nodiscard]] constexpr auto end(size_type axis, arrnd_returned_element_iterator_tag) const
        {
            return empty() ? iterator()
                           : iterator(buffsp_->data(), indexer_type(move(hdr_, axis, 0), arrnd_iterator_position::end));
        }
        [[nodiscard]] constexpr auto cend(size_type axis, arrnd_returned_element_iterator_tag) const
        {
            return empty()
                ? const_iterator()
                : const_iterator(buffsp_->data(), indexer_type(move(hdr_, axis, 0), arrnd_iterator_position::end));
        }
        [[nodiscard]] constexpr auto rbegin(size_type axis, arrnd_returned_element_iterator_tag)
        {
            return empty()
                ? reverse_iterator()
                : reverse_iterator(buffsp_->data(), indexer_type(move(hdr_, axis, 0), arrnd_iterator_position::rbegin));
        }
        [[nodiscard]] constexpr auto rbegin(size_type axis, arrnd_returned_element_iterator_tag) const
        {
            return empty()
                ? reverse_iterator()
                : reverse_iterator(buffsp_->data(), indexer_type(move(hdr_, axis, 0), arrnd_iterator_position::rbegin));
        }
        [[nodiscard]] constexpr auto crbegin(size_type axis, arrnd_returned_element_iterator_tag) const
        {
            return empty() ? const_reverse_iterator()
                           : const_reverse_iterator(
                               buffsp_->data(), indexer_type(move(hdr_, axis, 0), arrnd_iterator_position::rbegin));
        }
        [[nodiscard]] constexpr auto rend(size_type axis, arrnd_returned_element_iterator_tag)
        {
            return empty()
                ? reverse_iterator()
                : reverse_iterator(buffsp_->data(), indexer_type(move(hdr_, axis, 0), arrnd_iterator_position::rend));
        }
        [[nodiscard]] constexpr auto rend(size_type axis, arrnd_returned_element_iterator_tag) const
        {
            return empty()
                ? reverse_iterator()
                : reverse_iterator(buffsp_->data(), indexer_type(move(hdr_, axis, 0), arrnd_iterator_position::rend));
        }
        [[nodiscard]] constexpr auto crend(size_type axis, arrnd_returned_element_iterator_tag) const
        {
            return empty() ? const_reverse_iterator()
                           : const_reverse_iterator(
                               buffsp_->data(), indexer_type(move(hdr_, axis, 0), arrnd_iterator_position::rend));
        }

        // by order iterator functions

        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto begin(const InputIt& first_order, const InputIt& last_order)
        {
            return empty()
                ? iterator()
                : iterator(buffsp_->data(), indexer_type(oc::arrnd::transpose(hdr_, first_order, last_order)));
        }
        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto begin(const InputIt& first_order, const InputIt& last_order) const
        {
            return empty()
                ? iterator()
                : iterator(buffsp_->data(), indexer_type(oc::arrnd::transpose(hdr_, first_order, last_order)));
        }
        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto cbegin(const InputIt& first_order, const InputIt& last_order) const
        {
            return empty()
                ? const_iterator()
                : const_iterator(buffsp_->data(), indexer_type(oc::arrnd::transpose(hdr_, first_order, last_order)));
        }
        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto end(const InputIt& first_order, const InputIt& last_order)
        {
            return empty()
                ? iterator()
                : iterator(buffsp_->data(),
                    indexer_type(oc::arrnd::transpose(hdr_, first_order, last_order), arrnd_iterator_position::end));
        }
        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto end(const InputIt& first_order, const InputIt& last_order) const
        {
            return empty()
                ? iterator()
                : iterator(buffsp_->data(),
                    indexer_type(oc::arrnd::transpose(hdr_, first_order, last_order), arrnd_iterator_position::end));
        }
        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto cend(const InputIt& first_order, const InputIt& last_order) const
        {
            return empty()
                ? const_iterator()
                : const_iterator(buffsp_->data(),
                    indexer_type(oc::arrnd::transpose(hdr_, first_order, last_order), arrnd_iterator_position::end));
        }
        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto rbegin(const InputIt& first_order, const InputIt& last_order)
        {
            return empty()
                ? reverse_iterator()
                : reverse_iterator(buffsp_->data(),
                    indexer_type(oc::arrnd::transpose(hdr_, first_order, last_order), arrnd_iterator_position::rbegin));
        }
        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto rbegin(const InputIt& first_order, const InputIt& last_order) const
        {
            return empty()
                ? reverse_iterator()
                : reverse_iterator(buffsp_->data(),
                    indexer_type(oc::arrnd::transpose(hdr_, first_order, last_order), arrnd_iterator_position::rbegin));
        }
        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto crbegin(const InputIt& first_order, const InputIt& last_order) const
        {
            return empty()
                ? const_reverse_iterator()
                : const_reverse_iterator(buffsp_->data(),
                    indexer_type(oc::arrnd::transpose(hdr_, first_order, last_order), arrnd_iterator_position::rbegin));
        }
        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto rend(const InputIt& first_order, const InputIt& last_order)
        {
            return empty()
                ? reverse_iterator()
                : reverse_iterator(buffsp_->data(),
                    indexer_type(oc::arrnd::transpose(hdr_, first_order, last_order), arrnd_iterator_position::rend));
        }
        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto rend(const InputIt& first_order, const InputIt& last_order) const
        {
            return empty()
                ? reverse_iterator()
                : reverse_iterator(buffsp_->data(),
                    indexer_type(oc::arrnd::transpose(hdr_, first_order, last_order), arrnd_iterator_position::rend));
        }
        template <iterator_of_type_integral InputIt>
        [[nodiscard]] constexpr auto crend(const InputIt& first_order, const InputIt& last_order) const
        {
            return empty()
                ? const_reverse_iterator()
                : const_reverse_iterator(buffsp_->data(),
                    indexer_type(oc::arrnd::transpose(hdr_, first_order, last_order), arrnd_iterator_position::rend));
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
            return empty()
                ? slice_iterator()
                : slice_iterator(*this,
                    ranger_type(hdr_, 0, window_type(window_interval_type{0, 1}), arrnd_iterator_position::begin));
        }
        [[nodiscard]] constexpr auto begin(arrnd_returned_slice_iterator_tag) const
        {
            return empty()
                ? slice_iterator()
                : slice_iterator(*this,
                    ranger_type(hdr_, 0, window_type(window_interval_type{0, 1}), arrnd_iterator_position::begin));
        }
        [[nodiscard]] constexpr auto cbegin(arrnd_returned_slice_iterator_tag) const
        {
            return empty()
                ? const_slice_iterator()
                : const_slice_iterator(*this,
                    ranger_type(hdr_, 0, window_type(window_interval_type{0, 1}), arrnd_iterator_position::begin));
        }
        [[nodiscard]] constexpr auto end(arrnd_returned_slice_iterator_tag)
        {
            return empty()
                ? slice_iterator()
                : slice_iterator(
                    *this, ranger_type(hdr_, 0, window_type(window_interval_type{0, 1}), arrnd_iterator_position::end));
        }
        [[nodiscard]] constexpr auto end(arrnd_returned_slice_iterator_tag) const
        {
            return empty()
                ? slice_iterator()
                : slice_iterator(
                    *this, ranger_type(hdr_, 0, window_type(window_interval_type{0, 1}), arrnd_iterator_position::end));
        }
        [[nodiscard]] constexpr auto cend(arrnd_returned_slice_iterator_tag) const
        {
            return empty()
                ? const_slice_iterator()
                : const_slice_iterator(
                    *this, ranger_type(hdr_, 0, window_type(window_interval_type{0, 1}), arrnd_iterator_position::end));
        }
        [[nodiscard]] constexpr auto rbegin(arrnd_returned_slice_iterator_tag)
        {
            return empty() ? reverse_slice_iterator()
                           : reverse_slice_iterator(*this,
                               ranger_type(hdr_, 0, window_type(window_interval_type{0, 1}, arrnd_window_type::partial),
                                   arrnd_iterator_position::rbegin));
        }
        [[nodiscard]] constexpr auto rbegin(arrnd_returned_slice_iterator_tag) const
        {
            return empty() ? reverse_slice_iterator()
                           : reverse_slice_iterator(*this,
                               ranger_type(hdr_, 0, window_type(window_interval_type{0, 1}, arrnd_window_type::partial),
                                   arrnd_iterator_position::rbegin));
        }
        [[nodiscard]] constexpr auto crbegin(arrnd_returned_slice_iterator_tag) const
        {
            return empty() ? const_reverse_slice_iterator()
                           : const_reverse_slice_iterator(*this,
                               ranger_type(hdr_, 0, window_type(window_interval_type{0, 1}, arrnd_window_type::partial),
                                   arrnd_iterator_position::rbegin));
        }
        [[nodiscard]] constexpr auto rend(arrnd_returned_slice_iterator_tag)
        {
            return empty() ? reverse_slice_iterator()
                           : reverse_slice_iterator(*this,
                               ranger_type(hdr_, 0, window_type(window_interval_type{0, 1}, arrnd_window_type::partial),
                                   arrnd_iterator_position::rend));
        }
        [[nodiscard]] constexpr auto rend(arrnd_returned_slice_iterator_tag) const
        {
            return empty() ? reverse_slice_iterator()
                           : reverse_slice_iterator(*this,
                               ranger_type(hdr_, 0, window_type(window_interval_type{0, 1}, arrnd_window_type::partial),
                                   arrnd_iterator_position::rend));
        }
        [[nodiscard]] constexpr auto crend(arrnd_returned_slice_iterator_tag) const
        {
            return empty() ? const_reverse_slice_iterator()
                           : const_reverse_slice_iterator(*this,
                               ranger_type(hdr_, 0, window_type(window_interval_type{0, 1}, arrnd_window_type::partial),
                                   arrnd_iterator_position::rend));
        }

        [[nodiscard]] constexpr auto begin(size_type axis, arrnd_returned_slice_iterator_tag)
        {
            return empty()
                ? slice_iterator()
                : slice_iterator(*this,
                    ranger_type(hdr_, axis, window_type(window_interval_type{0, 1}), arrnd_iterator_position::begin));
        }
        [[nodiscard]] constexpr auto begin(size_type axis, arrnd_returned_slice_iterator_tag) const
        {
            return empty()
                ? slice_iterator()
                : slice_iterator(*this,
                    ranger_type(hdr_, axis, window_type(window_interval_type{0, 1}), arrnd_iterator_position::begin));
        }
        [[nodiscard]] constexpr auto cbegin(size_type axis, arrnd_returned_slice_iterator_tag) const
        {
            return empty()
                ? const_slice_iterator()
                : const_slice_iterator(*this,
                    ranger_type(hdr_, axis, window_type(window_interval_type{0, 1}), arrnd_iterator_position::begin));
        }
        [[nodiscard]] constexpr auto end(size_type axis, arrnd_returned_slice_iterator_tag)
        {
            return empty()
                ? slice_iterator()
                : slice_iterator(*this,
                    ranger_type(hdr_, axis, window_type(window_interval_type{0, 1}), arrnd_iterator_position::end));
        }
        [[nodiscard]] constexpr auto end(size_type axis, arrnd_returned_slice_iterator_tag) const
        {
            return empty()
                ? slice_iterator()
                : slice_iterator(*this,
                    ranger_type(hdr_, axis, window_type(window_interval_type{0, 1}), arrnd_iterator_position::end));
        }
        [[nodiscard]] constexpr auto cend(size_type axis, arrnd_returned_slice_iterator_tag) const
        {
            return empty()
                ? const_slice_iterator()
                : const_slice_iterator(*this,
                    ranger_type(hdr_, axis, window_type(window_interval_type{0, 1}), arrnd_iterator_position::end));
        }
        [[nodiscard]] constexpr auto rbegin(size_type axis, arrnd_returned_slice_iterator_tag)
        {
            return empty()
                ? reverse_slice_iterator()
                : reverse_slice_iterator(*this,
                    ranger_type(hdr_, axis, window_type(window_interval_type{0, 1}), arrnd_iterator_position::rbegin));
        }
        [[nodiscard]] constexpr auto rbegin(size_type axis, arrnd_returned_slice_iterator_tag) const
        {
            return empty()
                ? reverse_slice_iterator()
                : reverse_slice_iterator(*this,
                    ranger_type(hdr_, axis, window_type(window_interval_type{0, 1}), arrnd_iterator_position::rbegin));
        }
        [[nodiscard]] constexpr auto crbegin(size_type axis, arrnd_returned_slice_iterator_tag) const
        {
            return empty()
                ? const_reverse_slice_iterator()
                : const_reverse_slice_iterator(*this,
                    ranger_type(hdr_, axis, window_type(window_interval_type{0, 1}), arrnd_iterator_position::rbegin));
        }
        [[nodiscard]] constexpr auto rend(size_type axis, arrnd_returned_slice_iterator_tag)
        {
            return empty()
                ? reverse_slice_iterator()
                : reverse_slice_iterator(*this,
                    ranger_type(hdr_, axis, window_type(window_interval_type{0, 1}), arrnd_iterator_position::rend));
        }
        [[nodiscard]] constexpr auto rend(size_type axis, arrnd_returned_slice_iterator_tag) const
        {
            return empty()
                ? reverse_slice_iterator()
                : reverse_slice_iterator(*this,
                    ranger_type(hdr_, axis, window_type(window_interval_type{0, 1}), arrnd_iterator_position::rend));
        }
        [[nodiscard]] constexpr auto crend(size_type axis, arrnd_returned_slice_iterator_tag) const
        {
            return empty()
                ? const_reverse_slice_iterator()
                : const_reverse_slice_iterator(*this,
                    ranger_type(hdr_, axis, window_type(window_interval_type{0, 1}), arrnd_iterator_position::rend));
        }

        [[nodiscard]] constexpr auto abs() const
        {
            return transform([](const auto& a) {
                using std::abs;
                return abs(a);
            });
        }

        [[nodiscard]] constexpr auto acos() const
        {
            return transform([](const auto& a) {
                using std::acos;
                return acos(a);
            });
        }

        [[nodiscard]] constexpr auto acosh() const
        {
            return transform([](const auto& a) {
                using std::acosh;
                return acosh(a);
            });
        }

        [[nodiscard]] constexpr auto asin() const
        {
            return transform([](const auto& a) {
                using std::asin;
                return asin(a);
            });
        }

        [[nodiscard]] constexpr auto asinh() const
        {
            return transform([](const auto& a) {
                using std::asinh;
                return asinh(a);
            });
        }

        [[nodiscard]] constexpr auto atan() const
        {
            return transform([](const auto& a) {
                using std::atan;
                return atan(a);
            });
        }

        [[nodiscard]] constexpr auto atanh() const
        {
            return transform([](const auto& a) {
                using std::atanh;
                return atanh(a);
            });
        }

        [[nodiscard]] constexpr auto cos() const
        {
            return transform([](const auto& a) {
                using std::cos;
                return cos(a);
            });
        }

        [[nodiscard]] constexpr auto cosh() const
        {
            return transform([](const auto& a) {
                using std::cosh;
                return cosh(a);
            });
        }

        [[nodiscard]] constexpr auto exp() const
        {
            return transform([](const auto& a) {
                using std::exp;
                return exp(a);
            });
        }

        [[nodiscard]] constexpr auto log() const
        {
            return transform([](const auto& a) {
                using std::log;
                return log(a);
            });
        }

        [[nodiscard]] constexpr auto log10() const
        {
            return transform([](const auto& a) {
                using std::log10;
                return log10(a);
            });
        }

        template <typename U>
            requires(!arrnd_type<U>)
        [[nodiscard]] constexpr auto pow(const U& value) const
        {
            return transform([&value](const auto& a) {
                using std::pow;
                return pow(a, value);
            });
        }

        template <arrnd_type Arrnd>
        [[nodiscard]] constexpr auto pow(const Arrnd& arr) const
        {
            return transform(arr, [](const auto& a, const auto& b) {
                using std::pow;
                return pow(a, b);
            });
        }

        [[nodiscard]] constexpr auto sin() const
        {
            return transform([](const auto& a) {
                using std::sin;
                return sin(a);
            });
        }

        [[nodiscard]] constexpr auto sinh() const
        {
            return transform([](const auto& a) {
                using std::sinh;
                return sinh(a);
            });
        }

        [[nodiscard]] constexpr auto sqrt() const
        {
            return transform([](const auto& a) {
                using std::sqrt;
                return sqrt(a);
            });
        }

        [[nodiscard]] constexpr auto tan() const
        {
            return transform([](const auto& a) {
                using std::tan;
                return tan(a);
            });
        }

        [[nodiscard]] constexpr auto tanh() const
        {
            return transform([](const auto& a) {
                using std::tanh;
                return tanh(a);
            });
        }

        [[nodiscard]] constexpr auto round() const
        {
            return transform([](const auto& a) {
                using std::round;
                return round(a);
            });
        }

        [[nodiscard]] constexpr auto ceil() const
        {
            return transform([](const auto& a) {
                using std::ceil;
                return ceil(a);
            });
        }

        [[nodiscard]] constexpr auto floor() const
        {
            return transform([](const auto& a) {
                using std::floor;
                return floor(a);
            });
        }

        [[nodiscard]] constexpr auto real() const
        {
            return transform([](const auto& a) {
                using std::real;
                return real(a);
            });
        }

        [[nodiscard]] constexpr auto imag() const
        {
            return transform([](const auto& a) {
                using std::imag;
                return imag(a);
            });
        }

        [[nodiscard]] constexpr auto arg() const
        {
            return transform([](const auto& a) {
                using std::arg;
                return arg(a);
            });
        }

        [[nodiscard]] constexpr auto norm() const
        {
            return transform([](const auto& a) {
                using std::norm;
                return norm(a);
            });
        }

        [[nodiscard]] constexpr auto conj() const
        {
            return transform([](const auto& a) {
                using std::conj;
                return conj(a);
            });
        }

        [[nodiscard]] constexpr auto proj() const
        {
            return transform([](const auto& a) {
                using std::proj;
                return proj(a);
            });
        }

        [[nodiscard]] constexpr auto polar() const
        {
            return transform([](const auto& r) {
                using std::polar;
                return polar(r);
            });
        }

        template <arrnd_type Arrnd>
        [[nodiscard]] constexpr auto polar(const Arrnd& thetas) const
        {
            return transform(thetas, [](const auto& r, const auto& theta) {
                using std::polar;
                return polar(r, theta);
            });
        }

        [[nodiscard]] constexpr auto sign() const
        {
            return transform([](const auto& a) {
                return details::sign(a);
            });
        }

    private:
        header_type hdr_{};
        std::shared_ptr<storage_type> buffsp_{nullptr};
        std::shared_ptr<bool> original_valid_creator_ = std::allocate_shared<bool>(shared_ref_allocator_type<bool>());
        std::weak_ptr<bool> is_creator_valid_{};
        const this_type* creator_ = nullptr;
        bool treat_as_pages_
            = false; // useful for specific operations e.g. for operator* to decide between element wise and matrix multiplication
    };

    // arrnd type deduction by constructors

    template <iterator_of_type_integral InputDimsIt, iterator_type InputDataIt,
        typename DataStorageInfo = simple_vector_traits<iterator_value_t<InputDataIt>>,
        typename DimsStorageInfo = simple_vector_traits<std::size_t>>
    arrnd(const InputDimsIt&, const InputDimsIt&, const InputDataIt&, const InputDataIt&)
        -> arrnd<iterator_value_t<InputDataIt>, DataStorageInfo, DimsStorageInfo>;

    template <typename U, typename DataStorageInfo = simple_vector_traits<U>,
        typename DimsStorageInfo = simple_vector_traits<std::size_t>>
    arrnd(std::initializer_list<typename DataStorageInfo::storage_type::size_type>, std::initializer_list<U>)
        -> arrnd<U, DataStorageInfo, DimsStorageInfo>;

    template <iterable_of_type_integral Cont, iterable_type DataCont,
        typename DataStorageInfo = simple_vector_traits<iterable_value_t<DataCont>>,
        typename DimsStorageInfo = simple_vector_traits<std::size_t>>
    arrnd(const Cont&, const DataCont&) -> arrnd<iterable_value_t<DataCont>, DataStorageInfo, DimsStorageInfo>;

    template <typename Func, typename DataStorageInfo = simple_vector_traits<std::invoke_result_t<Func>>,
        typename DimsStorageInfo = simple_vector_traits<std::size_t>, iterator_of_type_integral InputDimsIt>
        requires(invocable_no_arrnd<Func>)
    arrnd(const InputDimsIt&, const InputDimsIt&, Func&&)
        -> arrnd<std::invoke_result_t<Func>, DataStorageInfo, DimsStorageInfo>;
    template <typename Func, typename DataStorageInfo = simple_vector_traits<std::invoke_result_t<Func>>,
        typename DimsStorageInfo = simple_vector_traits<std::size_t>, iterable_of_type_integral Cont>
        requires(invocable_no_arrnd<Func>)
    arrnd(const Cont&, Func&&) -> arrnd<std::invoke_result_t<Func>, DataStorageInfo, DimsStorageInfo>;
    template <typename Func, typename DataStorageInfo = simple_vector_traits<std::invoke_result_t<Func>>,
        typename DimsStorageInfo = simple_vector_traits<std::size_t>>
        requires(invocable_no_arrnd<Func>)
    arrnd(std::initializer_list<typename DataStorageInfo::storage_type::size_type>, Func&&)
        -> arrnd<std::invoke_result_t<Func>, DataStorageInfo, DimsStorageInfo>;

    // free arrnd iterator functions

    template <arrnd_type Arrnd, typename... Args>
    inline constexpr auto begin(Arrnd& c, Args&&... args)
    {
        return c.begin(std::forward<Args>(args)...);
    }
    template <arrnd_type Arrnd, typename... Args>
    inline constexpr auto begin(const Arrnd& c, Args&&... args)
    {
        return c.begin(std::forward<Args>(args)...);
    }
    template <arrnd_type Arrnd, typename... Args>
    inline constexpr auto cbegin(const Arrnd& c, Args&&... args)
    {
        return c.cbegin(std::forward<Args>(args)...);
    }

    template <arrnd_type Arrnd, typename... Args>
    inline constexpr auto end(Arrnd& c, Args&&... args)
    {
        return c.end(std::forward<Args>(args)...);
    }
    template <arrnd_type Arrnd, typename... Args>
    inline constexpr auto end(const Arrnd& c, Args&&... args)
    {
        return c.end(std::forward<Args>(args)...);
    }
    template <arrnd_type Arrnd, typename... Args>
    inline constexpr auto cend(const Arrnd& c, Args&&... args)
    {
        return c.cend(std::forward<Args>(args)...);
    }

    template <arrnd_type Arrnd, typename... Args>
    inline constexpr auto rbegin(Arrnd& c, Args&&... args)
    {
        return c.rbegin(std::forward<Args>(args)...);
    }
    template <arrnd_type Arrnd, typename... Args>
    inline constexpr auto rbegin(const Arrnd& c, Args&&... args)
    {
        return c.rbegin(std::forward<Args>(args)...);
    }
    template <arrnd_type Arrnd, typename... Args>
    inline constexpr auto crbegin(const Arrnd& c, Args&&... args)
    {
        return c.crbegin(std::forward<Args>(args)...);
    }

    template <arrnd_type Arrnd, typename... Args>
    inline constexpr auto rend(Arrnd& c, Args&&... args)
    {
        return c.rend(std::forward<Args>(args)...);
    }
    template <arrnd_type Arrnd, typename... Args>
    inline constexpr auto rend(const Arrnd& c, Args&&... args)
    {
        return c.rend(std::forward<Args>(args)...);
    }
    template <arrnd_type Arrnd, typename... Args>
    inline constexpr auto crend(const Arrnd& c, Args&&... args)
    {
        return c.crend(std::forward<Args>(args)...);
    }

    template <arrnd_type First, arrnd_type Second>
    [[nodiscard]] inline constexpr First concat(const First& first, const Second& second)
    {
        return first.push_back(second);
    }

    template <arrnd_type First, arrnd_type Second, typename Third, typename... Others>
        requires(arrnd_type<Third> || std::signed_integral<Third>)
    [[nodiscard]] inline constexpr First
        concat(const First& first, const Second& second, const Third& third, Others&&... others)
    {
        if constexpr (sizeof...(Others) == 0) {
            if constexpr (std::signed_integral<Third>) {
                return first.push_back(second, third);
            } else {
                return first.push_back(second).push_back(third);
            }
        } else {
            if constexpr (std::signed_integral<Third>) {
                return concat(first.push_back(second, third), std::forward<Others>(others)...);
            } else {
                return concat(first.push_back(second), third, std::forward<Others>(others)...);
            }
        }
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2, arrnd_type... Arrnds>
    [[nodiscard]] inline constexpr auto dot(const Arrnd1& arr1, const Arrnd2& arr2, Arrnds&&... others)
    {
        if constexpr (sizeof...(others) == 0) {
            return arr1.dot(arr2);
        } else {
            return dot(arr1.dot(arr2), std::forward<Arrnds>(others)...);
        }
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto det(const Arrnd& arr)
    {
        return arr.det();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto inv(const Arrnd& arr)
    {
        return arr.inv();
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr auto insert(
        const Arrnd1& lhs, const Arrnd2& rhs, typename Arrnd1::size_type ind, typename Arrnd1::size_type axis = 0)
    {
        return lhs.insert(rhs, ind, axis);
    }

    template <std::int64_t Level, arrnd_type Arrnd, typename Func>
    [[nodiscard]] inline constexpr auto reduce(const Arrnd& arr, Func&& func)
    {
        return arr.template reduce<Level>(std::forward<Func>(func));
    }
    template <arrnd_type Arrnd, typename Func>
    [[nodiscard]] inline constexpr auto reduce(const Arrnd& arr, Func&& func)
    {
        return reduce<Arrnd::depth>(arr, std::forward<Func>(func));
    }

    template <std::int64_t Level, arrnd_type Arrnd, typename T, typename Func>
    [[nodiscard]] inline constexpr auto fold(const Arrnd& arr, const T& init, Func&& func)
    {
        return arr.template fold<Level>(init, std::forward<Func>(func));
    }
    template <arrnd_type Arrnd, typename T, typename Func>
    [[nodiscard]] inline constexpr auto fold(const Arrnd& arr, const T& init, Func&& func)
    {
        return fold<Arrnd::depth>(arr, init, std::forward<Func>(func));
    }

    template <std::int64_t Level, arrnd_type Arrnd, typename Func>
    [[nodiscard]] inline constexpr auto reduce(const Arrnd& arr, typename Arrnd::size_type axis, Func&& func)
    {
        return arr.template reduce<Level>(axis, std::forward<Func>(func));
    }
    template <arrnd_type Arrnd, typename Func>
    [[nodiscard]] inline constexpr auto reduce(const Arrnd& arr, typename Arrnd::size_type axis, Func&& func)
    {
        return reduce<Arrnd::depth>(arr, axis, std::forward<Func>(func));
    }

    template <std::int64_t Level, arrnd_type Arrnd1, arrnd_type Arrnd2, typename Func>
    [[nodiscard]] inline constexpr auto fold(
        const Arrnd1& arr, typename Arrnd1::size_type axis, const Arrnd2& inits, Func&& func)
    {
        return arr.template fold<Level>(axis, inits, std::forward<Func>(func));
    }
    template <arrnd_type Arrnd1, arrnd_type Arrnd2, typename Func>
    [[nodiscard]] inline constexpr auto fold(
        const Arrnd1& arr, typename Arrnd1::size_type axis, const Arrnd2& inits, Func&& func)
    {
        return fold<Arrnd1::depth>(arr, axis, inits, std::forward<Func>(func));
    }

    template <std::int64_t Level, arrnd_type Arrnd, typename Pred>
        requires(invocable_no_arrnd<Pred, typename Arrnd::template inner_value_type<Level>>)
    [[nodiscard]] inline constexpr auto all(const Arrnd& arr, typename Arrnd::size_type axis, Pred&& pred)
    {
        return arr.template all<Level>(axis, std::forward<Pred>(pred));
    }

    template <arrnd_type Arrnd, typename Pred>
        requires(invocable_no_arrnd<Pred, typename Arrnd::template inner_value_type<Arrnd::depth>>)
    [[nodiscard]] inline constexpr auto all(const Arrnd& arr, typename Arrnd::size_type axis, Pred&& pred)
    {
        return all<Arrnd::depth>(arr, axis, std::forward<Pred>(pred));
    }

    template <std::int64_t Level, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto all(const Arrnd& arr, typename Arrnd::size_type axis)
    {
        return arr.template all<Level>(axis);
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto all(const Arrnd& arr, typename Arrnd::size_type axis)
    {
        return all<Arrnd::depth>(arr, axis);
    }

    template <std::int64_t Level, arrnd_type Arrnd, typename Pred>
        requires(invocable_no_arrnd<Pred, typename Arrnd::template inner_value_type<Level>>)
    [[nodiscard]] inline constexpr auto any(const Arrnd& arr, typename Arrnd::size_type axis, Pred&& pred)
    {
        return arr.template any<Level>(axis, std::forward<Pred>(pred));
    }

    template <arrnd_type Arrnd, typename Pred>
        requires(invocable_no_arrnd<Pred, typename Arrnd::template inner_value_type<Arrnd::depth>>)
    [[nodiscard]] inline constexpr auto any(const Arrnd& arr, typename Arrnd::size_type axis, Pred&& pred)
    {
        return any<Arrnd::depth>(arr, axis, std::forward<Pred>(pred));
    }

    template <std::int64_t Level, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto any(const Arrnd& arr, typename Arrnd::size_type axis)
    {
        return arr.template any<Level>(axis);
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto any(const Arrnd& arr, typename Arrnd::size_type axis)
    {
        return any<Arrnd::depth>(arr, axis);
    }

    template <std::int64_t Level, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto sum(const Arrnd& arr)
    {
        return arr.template sum<Level>();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto sum(const Arrnd& arr)
    {
        return sum<Arrnd::depth>(arr);
    }

    template <std::int64_t Level, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto sum(const Arrnd& arr, typename Arrnd::size_type axis)
    {
        return arr.template sum<Level>(axis);
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto sum(const Arrnd& arr, typename Arrnd::size_type axis)
    {
        return sum<Arrnd::depth>(arr, axis);
    }

    template <std::int64_t Level, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto min(const Arrnd& arr)
    {
        return arr.template min<Level>();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto min(const Arrnd& arr)
    {
        return min<Arrnd::depth>(arr);
    }

    template <std::int64_t Level, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto min(const Arrnd& arr, typename Arrnd::size_type axis)
    {
        return arr.template min<Level>(axis);
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto min(const Arrnd& arr, typename Arrnd::size_type axis)
    {
        return min<Arrnd::depth>(arr, axis);
    }

    template <std::int64_t Level, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto max(const Arrnd& arr)
    {
        return arr.template max<Level>();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto max(const Arrnd& arr)
    {
        return max<Arrnd::depth>(arr);
    }

    template <std::int64_t Level, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto max(const Arrnd& arr, typename Arrnd::size_type axis)
    {
        return arr.template max<Level>(axis);
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto max(const Arrnd& arr, typename Arrnd::size_type axis)
    {
        return max<Arrnd::depth>(arr, axis);
    }

    template <std::int64_t Level, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto prod(const Arrnd& arr)
    {
        return arr.template prod<Level>();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto prod(const Arrnd& arr)
    {
        return prod<Arrnd::depth>(arr);
    }

    template <std::int64_t Level, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto prod(const Arrnd& arr, typename Arrnd::size_type axis)
    {
        return arr.template prod<Level>(axis);
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto prod(const Arrnd& arr, typename Arrnd::size_type axis)
    {
        return prod<Arrnd::depth>(arr, axis);
    }

    template <std::int64_t Level, arrnd_type Arrnd, typename Func>
    [[nodiscard]] inline constexpr auto transform(const Arrnd& arr, Func&& func)
    {
        return arr.template transform<Level>(std::forward<Func>(func));
    }

    template <std::int64_t Level, arrnd_type Arrnd, typename U, typename Func>
        requires arrnd_type<U>
    [[nodiscard]] inline constexpr auto transform(const Arrnd& lhs, const U& rhs, Func&& func)
    {
        return lhs.template transform<Level>(rhs, std::forward<Func>(func));
    }

    template <arrnd_type Arrnd, typename Func>
    [[nodiscard]] inline constexpr auto transform(const Arrnd& arr, Func&& func)
    {
        return transform<Arrnd::depth>(arr, std::forward<Func>(func));
    }

    template <arrnd_type Arrnd, typename U, typename Func>
        requires arrnd_type<U>
    [[nodiscard]] inline constexpr auto transform(const Arrnd& lhs, const U& rhs, Func&& func)
    {
        return transform<Arrnd::depth>(lhs, rhs, std::forward<Func>(func));
    }

    template <std::int64_t Level, arrnd_type Arrnd, typename Func>
    inline constexpr auto& apply(Arrnd&& arr, Func&& func)
    {
        return arr.template apply<Level>(std::forward<Func>(func));
    }

    template <std::int64_t Level, arrnd_type Arrnd, typename U, typename Func>
        requires arrnd_type<U>
    inline constexpr auto& apply(Arrnd&& lhs, const U& rhs, Func&& func)
    {
        return lhs.template apply<Level>(rhs, std::forward<Func>(func));
    }

    template <arrnd_type Arrnd, typename Func>
    inline constexpr auto& apply(Arrnd&& arr, Func&& func)
    {
        return apply<std::remove_cvref_t<Arrnd>::depth>(std::forward<Arrnd>(arr), std::forward<Func>(func));
    }

    template <arrnd_type Arrnd, typename U, typename Func>
        requires arrnd_type<U>
    inline constexpr auto& apply(Arrnd&& lhs, const U& rhs, Func&& func)
    {
        return apply<std::remove_cvref_t<Arrnd>::depth>(std::forward<Arrnd>(lhs), rhs, std::forward<Func>(func));
    }

    template <std::int64_t Level, arrnd_type Arrnd, typename Func>
    inline constexpr auto& apply(Arrnd& arr, Func&& func)
    {
        return arr.template apply<Level>(std::forward<Func>(func));
    }

    template <std::int64_t Level, arrnd_type Arrnd, typename U, typename Func>
        requires arrnd_type<U>
    inline constexpr auto& apply(Arrnd& lhs, const U& rhs, Func&& func)
    {
        return lhs.template apply<Level>(rhs, std::forward<Func>(func));
    }

    template <arrnd_type Arrnd, typename Func>
    inline constexpr auto& apply(Arrnd& arr, Func&& func)
    {
        return apply<std::remove_cvref_t<Arrnd>::depth>(arr, std::forward<Func>(func));
    }

    template <arrnd_type Arrnd, typename U, typename Func>
        requires arrnd_type<U>
    inline constexpr auto& apply(Arrnd& lhs, const U& rhs, Func&& func)
    {
        return apply<std::remove_cvref_t<Arrnd>::depth>(lhs, rhs, std::forward<Func>(func));
    }

    template <std::int64_t Level, arrnd_type Arrnd, typename Pred>
    [[nodiscard]] inline constexpr auto filter(const Arrnd& arr, Pred&& pred)
    {
        return arr.template filter<Level>(std::forward<Pred>(pred));
    }
    template <arrnd_type Arrnd, typename Pred>
    [[nodiscard]] inline constexpr auto filter(const Arrnd& arr, Pred&& pred)
    {
        return filter<Arrnd::depth>(arr, std::forward<Pred>(pred));
    }

    template <std::int64_t Level, arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr auto filter(const Arrnd1& arr, const Arrnd2& selector)
    {
        return arr.template filter<Level>(selector);
    }
    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr auto filter(const Arrnd1& arr, const Arrnd2& selector)
    {
        return filter<Arrnd1::depth>(arr, selector);
    }

    template <std::int64_t Level, arrnd_type Arrnd, typename Pred>
    [[nodiscard]] inline constexpr auto find(const Arrnd& arr, Pred&& pred)
    {
        return arr.template find<Level>(std::forward<Pred>(pred));
    }
    template <arrnd_type Arrnd, typename Pred>
    [[nodiscard]] inline constexpr auto find(const Arrnd& arr, Pred&& pred)
    {
        return find<Arrnd::depth>(arr, std::forward<Pred>(pred));
    }

    template <std::int64_t Level, arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr auto find(const Arrnd1& arr, const Arrnd2& mask)
    {
        return arr.template find<Level>(mask);
    }
    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr auto find(const Arrnd1& arr, const Arrnd2& mask)
    {
        return find<Arrnd1::depth>(arr, mask);
    }

    template <std::int64_t Level, arrnd_type Arrnd, iterator_of_type_integral InputIt>
    [[nodiscard]] inline constexpr auto filter(const Arrnd& arr, InputIt first_ind, InputIt last_ind)
    {
        return filter<Level>(arr,
            typename Arrnd::template replaced_type<typename Arrnd::size_type>(
                {std::distance(first_ind, last_ind)}, first_ind, last_ind));
    }
    template <std::int64_t Level, arrnd_type Arrnd, iterable_of_type_integral Cont>
        requires(!arrnd_type<Cont>)
    [[nodiscard]] inline constexpr auto filter(const Arrnd& arr, const Cont& indices)
    {
        return filter<Level>(arr,
            typename Arrnd::template replaced_type<typename Arrnd::size_type>(
                {std::ssize(indices)}, std::begin(indices), std::end(indices)));
    }
    template <std::int64_t Level, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto filter(
        const Arrnd& arr, std::initializer_list<typename Arrnd::size_type> indices)
    {
        return filter<Level>(arr,
            typename Arrnd::template replaced_type<typename Arrnd::size_type>(
                {std::ssize(indices)}, indices.begin(), indices.end()));
    }

    template <arrnd_type Arrnd, iterator_of_type_integral InputIt>
    [[nodiscard]] inline constexpr auto filter(const Arrnd& arr, InputIt first_ind, InputIt last_ind)
    {
        return filter<Arrnd::depth>(arr,
            typename Arrnd::template replaced_type<typename Arrnd::size_type>(
                {std::distance(first_ind, last_ind)}, first_ind, last_ind));
    }
    template <arrnd_type Arrnd, iterable_of_type_integral Cont>
        requires(!arrnd_type<Cont>)
    [[nodiscard]] inline constexpr auto filter(const Arrnd& arr, const Cont& indices)
    {
        return filter<Arrnd::depth>(arr,
            typename Arrnd::template replaced_type<typename Arrnd::size_type>(
                {std::ssize(indices)}, std::begin(indices), std::end(indices)));
    }
    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto filter(
        const Arrnd& arr, std::initializer_list<typename Arrnd::size_type> indices)
    {
        std::initializer_list<typename Arrnd::size_type> dims{std::size(indices)};
        return filter<Arrnd::depth>(arr,
            typename Arrnd::template replaced_type<typename Arrnd::size_type>(
                dims.begin(), dims.end(), indices.begin(), indices.end()));
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto transpose(const Arrnd& arr)
    {
        return arr.transpose();
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
        const typename Arrnd1::template compliant_tol_type<Arrnd2>& atol
        = default_atol<typename Arrnd1::template compliant_tol_type<Arrnd2>>(),
        const typename Arrnd1::template compliant_tol_type<Arrnd2>& rtol
        = default_rtol<typename Arrnd1::template compliant_tol_type<Arrnd2>>())
    {
        return lhs.close(rhs, atol, rtol);
    }

    template <arrnd_type Arrnd, typename T>
        requires(!arrnd_type<T>)
    [[nodiscard]] inline constexpr auto close(const Arrnd& lhs, const T& rhs,
        const typename Arrnd::template tol_type<T>& atol = default_atol<typename Arrnd::template tol_type<T>>(),
        const typename Arrnd::template tol_type<T>& rtol = default_rtol<typename Arrnd::template tol_type<T>>())
    {
        return lhs.close(rhs, atol, rtol);
    }

    template <typename T, arrnd_type Arrnd>
        requires(!arrnd_type<T>)
    [[nodiscard]] inline constexpr auto close(const T& lhs, const Arrnd& rhs,
        const typename Arrnd::template tol_type<T>& atol = default_atol<typename Arrnd::template tol_type<T>>(),
        const typename Arrnd::template tol_type<T>& rtol = default_rtol<typename Arrnd::template tol_type<T>>())
    {
        return rhs.close(lhs, atol, rtol);
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
        assert(lhs.is_as_pages() == rhs.is_as_pages());

        if (lhs.is_as_pages() && rhs.is_as_pages()) {
            return lhs.dot(rhs);
        }

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
        assert(lhs.is_as_pages() == rhs.is_as_pages());

        if (lhs.is_as_pages() && rhs.is_as_pages()) {
            lhs = lhs.dot(rhs);
            return lhs;
        }

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
        return arr.abs();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto acos(const Arrnd& arr)
    {
        return arr.acos();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto acosh(const Arrnd& arr)
    {
        return arr.acosh();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto asin(const Arrnd& arr)
    {
        return arr.asin();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto asinh(const Arrnd& arr)
    {
        return arr.asinh();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto atan(const Arrnd& arr)
    {
        return arr.atan();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto atanh(const Arrnd& arr)
    {
        return arr.atanh();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto cos(const Arrnd& arr)
    {
        return arr.cos();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto cosh(const Arrnd& arr)
    {
        return arr.cosh();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto exp(const Arrnd& arr)
    {
        return arr.exp();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto log(const Arrnd& arr)
    {
        return arr.log();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto log10(const Arrnd& arr)
    {
        return arr.log10();
    }

    template <arrnd_type Arrnd, typename U>
    [[nodiscard]] inline constexpr auto pow(const Arrnd& arr, const U& value)
    {
        return arr.pow(value);
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto sin(const Arrnd& arr)
    {
        return arr.sin();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto sinh(const Arrnd& arr)
    {
        return arr.sinh();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto sqrt(const Arrnd& arr)
    {
        return arr.sqrt();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto tan(const Arrnd& arr)
    {
        return arr.tan();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto tanh(const Arrnd& arr)
    {
        return arr.tanh();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto round(const Arrnd& arr)
    {
        return arr.round();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto ceil(const Arrnd& arr)
    {
        return arr.ceil();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto floor(const Arrnd& arr)
    {
        return arr.floor();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto real(const Arrnd& arr)
    {
        return arr.real();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto imag(const Arrnd& arr)
    {
        return arr.imag();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto arg(const Arrnd& arr)
    {
        return arr.arg();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto norm(const Arrnd& arr)
    {
        return arr.norm();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto conj(const Arrnd& arr)
    {
        return arr.conj();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto proj(const Arrnd& arr)
    {
        return arr.proj();
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto polar(const Arrnd& rs)
    {
        return rs.polar();
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr auto polar(const Arrnd1& rs, const Arrnd2& thetas)
    {
        return rs.polar(thetas);
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr auto sign(const Arrnd& arr)
    {
        return arr.sign();
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

        for (typename Arrnd::indexer_type gen(arr.header()); gen; ++gen) {
            ++arr[*gen];
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

        for (typename Arrnd::indexer_type gen(arr.header()); gen; ++gen) {
            --arr[*gen];
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

    template <arrnd_type Arrnd, typename Func>
    [[nodiscard]] inline constexpr auto slide(const Arrnd& arr, typename Arrnd::size_type axis,
        typename Arrnd::window_interval_type window, bool bounded, Func&& func)
    {
        return arr.slide(axis, window, bounded, std::forward<Func>(func));
    }

    template <arrnd_type Arrnd, typename ReduceFunc, typename TransformFunc>
    [[nodiscard]] inline constexpr auto accumulate(const Arrnd& arr, typename Arrnd::size_type axis,
        typename Arrnd::window_interval_type window, bool bounded, ReduceFunc&& rfunc, TransformFunc&& tfunc)
    {
        return arr.accumulate(
            axis, window, bounded, std::forward<ReduceFunc>(rfunc), std::forward<TransformFunc>(tfunc));
    }

    template <arrnd_type Arrnd, typename Func>
    inline constexpr auto browse(const Arrnd& arr, typename Arrnd::size_type page_size, Func&& func)
    {
        return arr.browse(page_size, std::forward<Func>(func));
    }

    template <std::int64_t Level, arrnd_type Arrnd, typename Pred>
        requires(invocable_no_arrnd<Pred, typename Arrnd::template inner_value_type<Level>>)
    [[nodiscard]] inline constexpr bool all(const Arrnd& arr, Pred&& pred)
    {
        return arr.template all<Level>(std::forward<Pred>(pred));
    }

    template <std::int64_t Level, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr bool all(const Arrnd& arr)
    {
        return arr.template all<Level>();
    }

    template <std::int64_t Level, arrnd_type Arrnd, typename U, typename Pred>
    [[nodiscard]] inline constexpr bool all_match(const Arrnd& lhs, const U& rhs, Pred&& pred)
    {
        return lhs.template all_match<Level>(rhs, std::forward<Pred>(pred));
    }

    template <std::int64_t Level, arrnd_type Arrnd, typename U>
    [[nodiscard]] inline constexpr bool all_match(const Arrnd& lhs, const U& rhs)
    {
        return lhs.template all_match<Level>(rhs);
    }

    template <std::int64_t Level, typename U, arrnd_type Arrnd, typename Pred>
        requires(!arrnd_type<U>)
    [[nodiscard]] inline constexpr bool all_match(const U& lhs, const Arrnd& rhs, Pred&& pred)
    {
        return rhs.template all_match<Level>(lhs, [&pred](const auto& a, const auto& b) {
            return pred(b, a);
        });
    }

    template <std::int64_t Level, typename U, arrnd_type Arrnd>
        requires(!arrnd_type<U>)
    [[nodiscard]] inline constexpr bool all_match(const U& lhs, const Arrnd& rhs)
    {
        return rhs.template all_match<Level>(lhs);
    }

    template <arrnd_type Arrnd, typename Pred>
        requires(invocable_no_arrnd<Pred, typename Arrnd::template inner_value_type<Arrnd::depth>>)
    [[nodiscard]] inline constexpr bool all(const Arrnd& arr, Pred&& pred)
    {
        return all<Arrnd::depth>(arr, std::forward<Pred>(pred));
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr bool all(const Arrnd& arr)
    {
        return all<Arrnd::depth>(arr);
    }

    template <arrnd_type Arrnd, typename U, typename Pred>
    [[nodiscard]] inline constexpr bool all_match(const Arrnd& lhs, const U& rhs, Pred&& pred)
    {
        return all_match<Arrnd::depth>(lhs, rhs, std::forward<Pred>(pred));
    }

    template <arrnd_type Arrnd, typename U>
    [[nodiscard]] inline constexpr bool all_match(const Arrnd& lhs, const U& rhs)
    {
        return all_match<Arrnd::depth>(lhs, rhs);
    }

    template <typename U, arrnd_type Arrnd, typename Pred>
        requires(!arrnd_type<U>)
    [[nodiscard]] inline constexpr bool all_match(const U& lhs, const Arrnd& rhs, Pred&& pred)
    {
        return all_match<Arrnd::depth>(lhs, rhs, std::forward<Pred>(pred));
    }

    template <typename U, arrnd_type Arrnd>
        requires(!arrnd_type<U>)
    [[nodiscard]] inline constexpr bool all_match(const U& lhs, const Arrnd& rhs)
    {
        return all_match<Arrnd::depth>(lhs, rhs);
    }

    template <std::int64_t Level, arrnd_type Arrnd, typename Pred>
        requires(invocable_no_arrnd<Pred, typename Arrnd::template inner_value_type<Level>>)
    [[nodiscard]] inline constexpr bool any(const Arrnd& arr, Pred&& pred)
    {
        return arr.template any<Level>(std::forward<Pred>(pred));
    }

    template <std::int64_t Level, arrnd_type Arrnd>
    [[nodiscard]] inline constexpr bool any(const Arrnd& arr)
    {
        return arr.template any<Level>();
    }

    template <std::int64_t Level, arrnd_type Arrnd, typename U, typename Pred>
        requires arrnd_type<U>
    [[nodiscard]] inline constexpr bool any_match(const Arrnd& lhs, const U& rhs, Pred&& pred)
    {
        return lhs.template any_match<Level>(rhs, std::forward<Pred>(pred));
    }

    template <std::int64_t Level, arrnd_type Arrnd, typename U>
        requires arrnd_type<U>
    [[nodiscard]] inline constexpr bool any_match(const Arrnd& lhs, const U& rhs)
    {
        return lhs.template any_match<Level>(rhs);
    }

    template <std::int64_t Level, typename U, arrnd_type Arrnd, typename Pred>
        requires(!arrnd_type<U>)
    [[nodiscard]] inline constexpr bool any_match(const U& lhs, const Arrnd& rhs, Pred&& pred)
    {
        return rhs.template any_match<Level>(lhs, [&pred](const auto& a, const auto& b) {
            return pred(b, a);
        });
    }

    template <std::int64_t Level, typename U, arrnd_type Arrnd>
        requires(!arrnd_type<U>)
    [[nodiscard]] inline constexpr bool any_match(const U& lhs, const Arrnd& rhs)
    {
        return rhs.template any_match<Level>(lhs);
    }

    template <arrnd_type Arrnd, typename Pred>
        requires(invocable_no_arrnd<Pred, typename Arrnd::template inner_value_type<Arrnd::depth>>)
    [[nodiscard]] inline constexpr bool any(const Arrnd& arr, Pred&& pred)
    {
        return any<Arrnd::depth>(arr, std::forward<Pred>(pred));
    }

    template <arrnd_type Arrnd>
    [[nodiscard]] inline constexpr bool any(const Arrnd& arr)
    {
        return any<Arrnd::depth>(arr);
    }

    template <arrnd_type Arrnd, typename U, typename Pred>
        requires arrnd_type<U>
    [[nodiscard]] inline constexpr bool any_match(const Arrnd& lhs, const U& rhs, Pred&& pred)
    {
        return any_match<Arrnd::depth>(lhs, rhs, std::forward<Pred>(pred));
    }

    template <arrnd_type Arrnd, typename U>
        requires arrnd_type<U>
    [[nodiscard]] inline constexpr bool any_match(const Arrnd& lhs, const U& rhs)
    {
        return any_match<Arrnd::depth>(lhs, rhs);
    }

    template <typename U, arrnd_type Arrnd, typename Pred>
        requires(!arrnd_type<U>)
    [[nodiscard]] inline constexpr bool any_match(const U& lhs, const Arrnd& rhs, Pred&& pred)
    {
        return any_match<Arrnd::depth>(lhs, rhs, std::forward<Pred>(pred));
    }

    template <typename U, arrnd_type Arrnd>
        requires(!arrnd_type<U>)
    [[nodiscard]] inline constexpr bool any_match(const U& lhs, const Arrnd& rhs)
    {
        return any_match<Arrnd::depth>(lhs, rhs);
    }

    template <arrnd_type Arrnd, typename T>
    [[nodiscard]] inline constexpr bool all_equal(const Arrnd& lhs, const T& rhs)
    {
        return lhs.all_equal(rhs);
    }

    template <typename T, arrnd_type Arrnd>
        requires(!arrnd_type<T>)
    [[nodiscard]] inline constexpr bool all_equal(const T& lhs, const Arrnd& rhs)
    {
        return rhs.all_equal(lhs);
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr bool all_close(const Arrnd1& lhs, const Arrnd2& rhs,
        const typename Arrnd1::template compliant_tol_type<Arrnd2, Arrnd1::depth>& atol
        = default_atol<typename Arrnd1::template compliant_tol_type<Arrnd2, Arrnd1::depth>>(),
        const typename Arrnd1::template compliant_tol_type<Arrnd2, Arrnd1::depth>& rtol
        = default_rtol<typename Arrnd1::template compliant_tol_type<Arrnd2, Arrnd1::depth>>())
    {
        return lhs.all_close(rhs, atol, rtol);
    }

    template <arrnd_type Arrnd, typename T>
        requires(!arrnd_type<T>)
    [[nodiscard]] inline constexpr bool all_close(const Arrnd& lhs, const T& rhs,
        const typename Arrnd::template tol_type<T, Arrnd::depth>& atol
        = default_atol<typename Arrnd::template tol_type<T, Arrnd::depth>>(),
        const typename Arrnd::template tol_type<T, Arrnd::depth>& rtol
        = default_rtol<typename Arrnd::template tol_type<T, Arrnd::depth>>())
    {
        return lhs.all_close(rhs, atol, rtol);
    }

    template <typename T, arrnd_type Arrnd>
        requires(!arrnd_type<T>)
    [[nodiscard]] inline constexpr bool all_close(const T& lhs, const Arrnd& rhs,
        const typename Arrnd::template tol_type<T, Arrnd::depth>& atol
        = default_atol<typename Arrnd::template tol_type<T, Arrnd::depth>>(),
        const typename Arrnd::template tol_type<T, Arrnd::depth>& rtol
        = default_rtol<typename Arrnd::template tol_type<T, Arrnd::depth>>())
    {
        return rhs.all_close(lhs, atol, rtol);
    }

    template <arrnd_type Arrnd, typename T>
    [[nodiscard]] inline constexpr bool any_equal(const Arrnd& lhs, const T& rhs)
    {
        return lhs.any_equal(rhs);
    }

    template <typename T, arrnd_type Arrnd>
        requires(!arrnd_type<T>)
    [[nodiscard]] inline constexpr bool any_equal(const T& lhs, const Arrnd& rhs)
    {
        return rhs.any_equal(lhs);
    }

    template <arrnd_type Arrnd1, arrnd_type Arrnd2>
    [[nodiscard]] inline constexpr bool any_close(const Arrnd1& lhs, const Arrnd2& rhs,
        const typename Arrnd1::template compliant_tol_type<Arrnd2>& atol
        = default_atol<typename Arrnd1::template compliant_tol_type<Arrnd2>>(),
        const typename Arrnd1::template compliant_tol_type<Arrnd2>& rtol
        = default_rtol<typename Arrnd1::template compliant_tol_type<Arrnd2>>())
    {
        return lhs.any_close(rhs, atol, rtol);
    }

    template <arrnd_type Arrnd, typename T>
        requires(!arrnd_type<T>)
    [[nodiscard]] inline constexpr bool any_close(const Arrnd& lhs, const T& rhs,
        const typename Arrnd::template tol_type<T>& atol = default_atol<typename Arrnd::template tol_type<T>>(),
        const typename Arrnd::template tol_type<T>& rtol = default_rtol<typename Arrnd::template tol_type<T>>())
    {
        return lhs.any_close(rhs, atol, rtol);
    }

    template <typename T, arrnd_type Arrnd>
        requires(!arrnd_type<T>)
    [[nodiscard]] inline constexpr bool any_close(const T& lhs, const Arrnd& rhs,
        const typename Arrnd::template tol_type<T>& atol = default_atol<typename Arrnd::template tol_type<T>>(),
        const typename Arrnd::template tol_type<T>& rtol = default_rtol<typename Arrnd::template tol_type<T>>())
    {
        return rhs.any_close(lhs, atol, rtol);
    }

    template <arrnd_type Arrnd>
    std::ostream& ostream_operator_recursive(std::ostream& os, const Arrnd& arco,
        typename Arrnd::size_type nvectical_spaces, typename Arrnd::size_type ndepth_spaces)
    {
        constexpr auto block_start_char = Arrnd::depth > 0 ? '{' : '[';
        constexpr auto block_stop_char = Arrnd::depth > 0 ? '}' : ']';

        if (arco.empty()) {
            os << block_start_char << block_stop_char;
            return os;
        }

        if constexpr (Arrnd::is_flat) {
            if (std::ssize(arco.header().dims()) > 1) {
                os << block_start_char;
                for (typename Arrnd::size_type i = 0; i < arco.header().dims()[0]; ++i) {
                    if (i > 0) {
                        for (typename Arrnd::size_type i = 0; i < ndepth_spaces + nvectical_spaces + 1; ++i) {
                            os << ' ';
                        }
                    }
                    ostream_operator_recursive(
                        os, arco[typename Arrnd::interval_type{i, i + 1}], nvectical_spaces + 1, ndepth_spaces);
                    if (i < arco.header().dims()[0] - 1) {
                        os << '\n';
                    }
                }
                os << block_stop_char;
                return os;
            }

            os << block_start_char;
            typename Arrnd::indexer_type gen(arco.header());
            os << arco[*gen];
            ++gen;
            for (; gen; ++gen) {
                os << ' ' << arco[*gen];
            }
            os << block_stop_char;
        } else {
            os << block_start_char;
            typename Arrnd::indexer_type gen(arco.header());
            typename Arrnd::size_type inner_count = 0;
            for (; gen; ++gen) {
                ostream_operator_recursive(os, arco[*gen], nvectical_spaces, ndepth_spaces + 1);
                if (++inner_count < total(arco.header())) {
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
    inline constexpr std::ostream& operator<<(std::ostream& os, const Arrnd& arco)
    {
        typename Arrnd::size_type nvectical_spaces = 0;
        typename Arrnd::size_type ndepth_spaces = 0;
        return ostream_operator_recursive(os, arco, nvectical_spaces, ndepth_spaces);
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
        friend std::ostream& operator<<(const arrnd_json_manip& ajm, const Arrnd& arco)
        {
            typename Arrnd::size_type nvertical_spaces = 4;
            ajm.os_ << "{\n";
            ajm.os_ << std::string(nvertical_spaces, ' ') << "\"base_type\": \""
                    << type_name<typename Arrnd::template inner_value_type<Arrnd::depth>>() << "\"\n";
            to_json(ajm.os_, arco, nvertical_spaces);
            ajm.os_ << "}";
            return ajm.os_;
        }

    private:
        template <arrnd_type Arrnd>
        static std::ostream& to_json(std::ostream& os, const Arrnd& arco, typename Arrnd::size_type nvertical_spaces)
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

            if (arco.empty()) {
                os << std::string(nvertical_spaces, ' ') << "\"header\": \"empty\",\n";
                os << std::string(nvertical_spaces, ' ') << "\"values\": \"empty\"\n";
                return os;
            }

            if constexpr (Arrnd::is_flat) {
                // header
                {
                    std::stringstream ss;
                    ss << arco.header();
                    os << std::string(nvertical_spaces, ' ') << "\"header\": \"" << replace_newlines(ss.str())
                       << "\",\n";
                }
                // array
                {
                    std::stringstream ss;
                    ss << arco;
                    os << std::string(nvertical_spaces, ' ') << "\"values\": \"" << replace_newlines(ss.str())
                       << "\"\n";
                }
            } else {
                // header
                {
                    std::stringstream ss;
                    ss << arco.header();
                    os << std::string(nvertical_spaces, ' ') << "\"header\": \"" << replace_newlines(ss.str())
                       << "\",\n";
                }
                // arrays
                os << std::string(nvertical_spaces, ' ') << "\"arrays\": [\n";
                typename Arrnd::indexer_type gen(arco.header());
                for (typename Arrnd::size_type i = 0; gen; ++gen, ++i) {
                    os << std::string(nvertical_spaces + 4, ' ') << "{\n";
                    to_json(os, arco[*gen], nvertical_spaces + 8);
                    os << std::string(nvertical_spaces + 4, ' ') << '}';
                    if (i < total(arco.header()) - 1) {
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
using details::arrnd_of_type;
using details::arrnd_of_template_type;
using details::arrnd_with_trait;
using details::arrnd_json;

using details::arrnd_inner;
using details::arrnd_inner_t;

using details::arrnd_shape_preset;
using details::arrnd_filter_proxy;
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

using details::slide;
using details::accumulate;
using details::browse;
using details::all;
using details::any;
using details::all_match;
using details::any_match;
using details::transform;
using details::apply;
using details::reduce;
using details::fold;
using details::filter;
using details::find;
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