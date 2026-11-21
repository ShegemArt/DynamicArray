#pragma once

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

// Динамический массив, память выделяется через malloc и освобождается через free
template<typename T>
class Array final
{
    // malloc гарантирует выравнивание только до max_align_t
    static_assert(alignof(T) <= alignof(std::max_align_t),
        "malloc() не гарантирует нужное выравнивание для типа T");

public:
    // Создает массив с емкостью по умолчанию
    Array() : Array(kDefaultCapacity)
    {
    }

    // Создает массив с заданной емкостью
    explicit Array(int capacity) : m_capacity(capacity > 0 ? capacity : 1)
    {
        m_data = allocate(m_capacity);
    }

    // Деструктор
    ~Array()
    {
        if (m_data != nullptr)
        {
            std::destroy_n(m_data, m_size);
        }

        std::free(m_data);
    }

    // Копирует элементы в новый блок памяти
    Array(const Array& other) : m_capacity(other.m_capacity)
    {
        m_data = allocate(m_capacity);

        if (other.m_size > 0) {
            try
            {
                std::uninitialized_copy_n(other.m_data, other.m_size, m_data);
            }
            catch (...)
            {
                std::free(m_data);
                m_data = nullptr;
                m_capacity = 0;
                throw;
            }
        }

        m_size = other.m_size;
    }

    // Забирает блок памяти у other, после этого other пуст
    Array(Array&& other) noexcept
        : m_data(std::exchange(other.m_data, nullptr)),
        m_size(std::exchange(other.m_size, 0)),
        m_capacity(std::exchange(other.m_capacity, 0))
    {
    }

    // Итератор для обычного и константного массива, создать его можно только через Array
    template<bool IsConst>
    class BasicIterator final
    {
    public:
        // Тип ссылки, которую возвращает operator*
        using reference = std::conditional_t<IsConst, const T&, T&>;

        // Значение в текущей позиции
        const T& get() const
        {
            assert(hasNext());
            return (*m_array)[m_index];
        }

        // Записывает значение в текущую позицию
        void set(const T& value) requires (!IsConst)
        {
            assert(hasNext());
            (*m_array)[m_index] = value;
        }

        // Переходит к следующему элементу в направлении обхода
        void next()
        {
            assert(hasNext());
            m_index += m_step;
        }

        // true, пока итератор указывает на существующий элемент
        bool hasNext() const
        {
            return m_index >= 0 && m_index < m_array->size();
        }

        // Ссылка на элемент в текущей позиции
        reference operator*() const
        {
            assert(hasNext());
            return (*m_array)[m_index];
        }

        // Префиксный инкремент
        BasicIterator& operator++()
        {
            next();
            return *this;
        }

        // Постфиксный инкремент
        BasicIterator operator++(int)
        {
            BasicIterator old = *this;
            next();
            return old;
        }

    private:
        // Тип указателя на массив, константный или обычный
        using ArrayPtr = std::conditional_t<IsConst, const Array*, Array*>;

        // Массив, по которому идет обход
        ArrayPtr m_array;

        // Индекс текущего элемента, индекс остается верным после перевыделения памяти
        int m_index;

        // Шаг обхода, 1 для прямого порядка и -1 для обратного
        int m_step;

        // Создает итератор, вызвать его может только массив
        BasicIterator(ArrayPtr array, int index, int step)
            : m_array(array), m_index(index), m_step(step)
        {
        }

        // Разрешает массиву вызывать закрытый конструктор
        friend Array;
    };

    // Итератор для обычного массива
    using Iterator = BasicIterator<false>;

    // Итератор для константного массива
    using ConstIterator = BasicIterator<true>;

    // Копирующее присваивание через копию и swap
    Array& operator=(const Array& other)
    {
        if (this != &other)
        {
            Array tmp(other);
            swap(tmp);
        }
        return *this;
    }

    // Перемещающее присваивание через временный объект и swap
    Array& operator=(Array&& other) noexcept
    {
        Array tmp(std::move(other));
        swap(tmp);
        return *this;
    }

    // Меняет содержимое двух массивов местами
    void swap(Array& other) noexcept
    {
        std::swap(m_data, other.m_data);
        std::swap(m_size, other.m_size);
        std::swap(m_capacity, other.m_capacity);
    }

    // Вставляет value в конец и возвращает индекс элемента
    int insert(const T& value)
    {
        return insert(m_size, value);
    }

    // Вставляет value в позицию index и возвращает index
    // Значение копируется заранее, так как оно может быть ссылкой на элемент массива
    int insert(int index, const T& value)
    {
        assert(index >= 0 && index <= m_size);

        T tmp(value);

        if (m_size == m_capacity)
        {
            reallocate(m_capacity > 0 ? m_capacity * 2 : kDefaultCapacity);
        }

        for (int i = m_size; i > index; --i)
        {
            std::construct_at(m_data + i, std::move_if_noexcept(m_data[i - 1]));
            std::destroy_at(m_data + i - 1);
        }

        std::construct_at(m_data + index, std::move_if_noexcept(tmp));
        ++m_size;

        return index;
    }

    // Удаляет элемент в позиции index, память не освобождается
    void remove(int index)
    {
        assert(index >= 0 && index < m_size);

        std::destroy_at(m_data + index);

        for (int i = index; i < m_size - 1; ++i)
        {
            std::construct_at(m_data + i, std::move_if_noexcept(m_data[i + 1]));
            std::destroy_at(m_data + i + 1);
        }

        --m_size;
    }

    // Элемент по индексу для чтения
    const T& operator[](int index) const
    {
        assert(index >= 0 && index < m_size);
        return m_data[index];
    }

    // Элемент по индексу для чтения и записи
    T& operator[](int index)
    {
        assert(index >= 0 && index < m_size);
        return m_data[index];
    }

    // Количество существующих элементов
    int size() const
    {
        return m_size;
    }

    // Количество элементов, которое помещается в текущий блок памяти
    int capacity() const
    {
        return m_capacity;
    }

    // Итератор на первый элемент, обход в прямом порядке
    Iterator iterator()
    {
        return Iterator(this, 0, 1);
    }

    // Константный итератор на первый элемент, обход в прямом порядке
    ConstIterator iterator() const
    {
        return ConstIterator(this, 0, 1);
    }

    // Итератор на последний элемент, обход в обратном порядке
    Iterator reverseIterator()
    {
        return Iterator(this, m_size - 1, -1);
    }

    // Константный итератор на последний элемент, обход в обратном порядке
    ConstIterator reverseIterator() const
    {
        return ConstIterator(this, m_size - 1, -1);
    }

    // Указатель на первый элемент для range-for
    T* begin()
    {
        return m_data;
    }

    // Указатель на место после последнего элемента для range-for
    T* end()
    {
        return m_size == 0 ? m_data : m_data + m_size;
    }

    // Константный указатель на первый элемент для range-for
    const T* begin() const
    {
        return m_data;
    }

    // Константный указатель на место после последнего элемента для range-for
    const T* end() const
    {
        return m_size == 0 ? m_data : m_data + m_size;
    }

    // Константный указатель на первый элемент
    const T* cbegin() const
    {
        return m_data;
    }

    // Константный указатель на место после последнего элемента
    const T* cend() const
    {
        return m_size == 0 ? m_data : m_data + m_size;
    }

private:
    // Емкость по умолчанию
    static constexpr int kDefaultCapacity = 16;

    // Блок памяти с элементами
    T* m_data = nullptr;

    // Количество существующих элементов
    int m_size = 0;

    // Количество элементов, которое помещается в блок памяти
    int m_capacity = 0;

    // Выделяет память под capacity элементов без создания самих элементов
    static T* allocate(int capacity)
    {
        if (capacity == 0)
        {
            return nullptr;
        }

        void* p = std::malloc(sizeof(T) * static_cast<std::size_t>(capacity));
        if (p == nullptr)
        {
            throw std::bad_alloc();
        }

        return static_cast<T*>(p);
    }

    // Переносит элементы в новый блок памяти и освобождает старый
    void reallocate(int newCapacity)
    {
        T* newData = allocate(newCapacity);

        int constructed = 0;

        try
        {
            for (; constructed < m_size; ++constructed)
            {
                std::construct_at(newData + constructed, std::move_if_noexcept(m_data[constructed]));
            }
        }
        catch (...)
        {
            std::destroy_n(newData, constructed);
            std::free(newData);
            throw;
        }

        if (m_size > 0)
        {
            std::destroy_n(m_data, m_size);
        }

        std::free(m_data);

        m_data = newData;
        m_capacity = newCapacity;
    }
};