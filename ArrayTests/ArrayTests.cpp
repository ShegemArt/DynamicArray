#include <gtest/gtest.h>

#include <initializer_list>
#include <random>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "Array.h"

namespace
{

    using Ints = std::vector<int>;
    using Strings = std::vector<std::string>;

    // Копирует содержимое массива в vector, использует только size() и operator[]
    template<typename T>
    std::vector<T> toVector(const Array<T>& array)
    {
        std::vector<T> result;

        for (int i = 0; i < array.size(); ++i)
        {
            result.push_back(array[i]);
        }

        return result;
    }

    // Добавляет значения в конец массива по порядку
    template<typename T>
    void fillArray(Array<T>& array, std::type_identity_t<std::initializer_list<T>> values)
    {
        for (const T& value : values)
        {
            array.insert(value);
        }
    }

    // Проверяет, что массив содержит ровно переданные элементы в том же порядке
    template<typename T>
    ::testing::AssertionResult hasContents(const Array<T>& array,
        std::type_identity_t<std::initializer_list<T>> expected)
    {
        const std::vector<T> actual = toVector(array);
        const std::vector<T> wanted(expected);

        if (actual == wanted)
        {
            return ::testing::AssertionSuccess();
        }

        return ::testing::AssertionFailure()
            << "array contains " << ::testing::PrintToString(actual)
            << ", expected " << ::testing::PrintToString(wanted);
    }

    // Строка, которая не помещается во внутренний буфер std::string и хранится в куче
    std::string longString(const std::string& tag)
    {
        return tag + " - long string that does not fit into the small string buffer";
    }

    // Выполняет одинаковые случайные вставки и удаления над Array и std::vector, затем сравнивает их
    template<typename T, typename MakeValue>
    void checkAgainstVector(MakeValue makeValue)
    {
        std::mt19937 generator(12345);
        Array<T> array;
        std::vector<T> model;

        for (int step = 0; step < 2000; ++step)
        {
            const bool doInsert = model.empty() || generator() % 3 != 0;

            if (doInsert)
            {
                const int index = static_cast<int>(generator() % (model.size() + 1));
                const T value = makeValue(step);

                ASSERT_EQ(array.insert(index, value), index);
                model.insert(model.begin() + index, value);
            }
            else
            {
                const int index = static_cast<int>(generator() % model.size());

                array.remove(index);
                model.erase(model.begin() + index);
            }

            ASSERT_EQ(array.size(), static_cast<int>(model.size()));
        }

        EXPECT_EQ(toVector(array), model);
    }

    // Тип, который считает количество живых экземпляров
    class Counted
    {
    public:
        // Сколько объектов Counted существует в данный момент
        static inline int aliveCount = 0;

        // Значение, по которому можно отличать объекты друг от друга
        int value;

        explicit Counted(int v = 0) : value(v)
        {
            ++aliveCount;
        }

        Counted(const Counted& other) : value(other.value)
        {
            ++aliveCount;
        }

        Counted(Counted&& other) noexcept : value(other.value)
        {
            ++aliveCount;
        }

        Counted& operator=(const Counted& other) = default;

        Counted& operator=(Counted&& other) noexcept = default;

        ~Counted()
        {
            --aliveCount;
        }
    };

    // Фикстура для проверки времени жизни элементов, следит что объекты не теряются и не остаются
    class ArrayLifetimeTest : public ::testing::Test
    {
    protected:
        // Перед тестом живых объектов быть не должно
        void SetUp() override
        {
            ASSERT_EQ(Counted::aliveCount, 0);
        }

        // После теста все созданные объекты должны быть уничтожены
        void TearDown() override
        {
            EXPECT_EQ(Counted::aliveCount, 0);
        }
    };

} // namespace

// =====================================================================
// Группа 1: Array<int>, простой тип
// =====================================================================

// Конструктор по умолчанию создает пустой массив, точная емкость не регламентируется
TEST(ArrayIntTest, DefaultConstructorCreatesEmptyArray)
{
    Array<int> a;

    EXPECT_EQ(a.size(), 0);
    EXPECT_GT(a.capacity(), 0);
}

// Конструктор с параметром выделяет память ровно под заданное количество элементов
TEST(ArrayIntTest, CapacityConstructorUsesGivenCapacity)
{
    Array<int> a(5);

    EXPECT_EQ(a.size(), 0);
    EXPECT_EQ(a.capacity(), 5);
}

// Нулевая емкость заменяется на минимальную, массив остается рабочим
TEST(ArrayIntTest, ZeroCapacityArrayStillAcceptsElements)
{
    Array<int> a(0);

    a.insert(1);
    a.insert(2);

    EXPECT_TRUE(hasContents(a, { 1, 2 }));
}

// Вставка в конец возвращает индекс вставленного элемента
TEST(ArrayIntTest, InsertToEndReturnsIndexOfInsertedElement)
{
    Array<int> a;

    EXPECT_EQ(a.insert(10), 0);
    EXPECT_EQ(a.insert(20), 1);
    EXPECT_EQ(a.insert(30), 2);
    EXPECT_EQ(a.size(), 3);
    EXPECT_TRUE(hasContents(a, { 10, 20, 30 }));
}

// Вставка в начало сдвигает все элементы вправо
TEST(ArrayIntTest, InsertToBeginningShiftsElementsRight)
{
    Array<int> a;
    fillArray(a, { 2, 3, 4 });

    EXPECT_EQ(a.insert(0, 1), 0);
    EXPECT_TRUE(hasContents(a, { 1, 2, 3, 4 }));
}

// Вставка в середину сдвигает только элементы справа от позиции
TEST(ArrayIntTest, InsertToMiddleShiftsElementsRight)
{
    Array<int> a;
    fillArray(a, { 1, 2, 4, 5 });

    EXPECT_EQ(a.insert(2, 3), 2);
    EXPECT_TRUE(hasContents(a, { 1, 2, 3, 4, 5 }));
}

// Вставка по индексу, равному размеру, работает как вставка в конец
TEST(ArrayIntTest, InsertAtIndexEqualToSizeAppendsToEnd)
{
    Array<int> a;
    fillArray(a, { 1, 2 });

    EXPECT_EQ(a.insert(a.size(), 3), 2);
    EXPECT_TRUE(hasContents(a, { 1, 2, 3 }));
}

// При нехватке памяти емкость растет в 1.6..2 раза и элементы сохраняются
TEST(ArrayIntTest, InsertWhenFullGrowsCapacityAndKeepsElements)
{
    Array<int> a(10);

    for (int i = 0; i < 10; ++i)
    {
        a.insert(i);
    }
    ASSERT_EQ(a.capacity(), 10);

    a.insert(10);

    EXPECT_GE(a.capacity(), 16);
    EXPECT_LE(a.capacity(), 20);
    EXPECT_EQ(a.size(), 11);

    for (int i = 0; i < 11; ++i)
    {
        EXPECT_EQ(a[i], i);
    }
}

// Много вставок подряд, несколько перевыделений памяти, порядок сохраняется
TEST(ArrayIntTest, InsertManyElementsKeepsOrder)
{
    Array<int> a(1);

    for (int i = 0; i < 1000; ++i)
    {
        a.insert(i);
    }

    ASSERT_EQ(a.size(), 1000);

    for (int i = 0; i < 1000; ++i)
    {
        EXPECT_EQ(a[i], i);
    }
}

// Удаление первого элемента сдвигает остальные влево
TEST(ArrayIntTest, RemoveFirstShiftsElementsLeft)
{
    Array<int> a;
    fillArray(a, { 1, 2, 3, 4 });

    a.remove(0);

    EXPECT_TRUE(hasContents(a, { 2, 3, 4 }));
}

// Удаление элемента из середины
TEST(ArrayIntTest, RemoveMiddleShiftsElementsLeft)
{
    Array<int> a;
    fillArray(a, { 1, 2, 3, 4 });

    a.remove(1);

    EXPECT_TRUE(hasContents(a, { 1, 3, 4 }));
}

// Удаление последнего элемента ничего не сдвигает
TEST(ArrayIntTest, RemoveLastKeepsOtherElements)
{
    Array<int> a;
    fillArray(a, { 1, 2, 3 });

    a.remove(2);

    EXPECT_TRUE(hasContents(a, { 1, 2 }));
}

// Удаление единственного элемента делает массив пустым
TEST(ArrayIntTest, RemoveOnlyElementMakesArrayEmpty)
{
    Array<int> a;
    a.insert(7);

    a.remove(0);

    EXPECT_EQ(a.size(), 0);
    EXPECT_FALSE(a.iterator().hasNext());
}

// Удаление не освобождает память, емкость остается прежней
TEST(ArrayIntTest, RemoveDoesNotChangeCapacity)
{
    Array<int> a(4);
    fillArray(a, { 1, 2, 3, 4, 5 });
    const int capacityBefore = a.capacity();

    a.remove(0);
    a.remove(0);
    a.remove(0);

    EXPECT_EQ(a.capacity(), capacityBefore);
    EXPECT_EQ(a.size(), 2);
}

// После удаления всех элементов в массив снова можно добавлять
TEST(ArrayIntTest, InsertAfterRemovingAllElementsWorks)
{
    Array<int> a;
    fillArray(a, { 1, 2, 3 });

    a.remove(0);
    a.remove(0);
    a.remove(0);
    a.insert(9);

    EXPECT_TRUE(hasContents(a, { 9 }));
}

// Оператор индексирования позволяет записывать значения
TEST(ArrayIntTest, IndexOperatorWritesElement)
{
    Array<int> a;
    fillArray(a, { 1, 2, 3 });

    a[1] = 42;
    a[0] *= 5;

    EXPECT_TRUE(hasContents(a, { 5, 42, 3 }));
}

// Константный оператор индексирования позволяет читать значения
TEST(ArrayIntTest, ConstIndexOperatorReadsElement)
{
    Array<int> a;
    fillArray(a, { 1, 2, 3 });
    const Array<int>& view = a;

    EXPECT_EQ(view[0], 1);
    EXPECT_EQ(view[2], 3);
}

// Копия не зависит от оригинала: изменения одного не видны в другом
TEST(ArrayIntTest, CopyConstructorMakesIndependentCopy)
{
    Array<int> original;
    fillArray(original, { 1, 2, 3 });

    Array<int> copy(original);
    copy[0] = 100;
    copy.insert(4);
    original[2] = 300;

    EXPECT_TRUE(hasContents(original, { 1, 2, 300 }));
    EXPECT_TRUE(hasContents(copy, { 100, 2, 3, 4 }));
}

// Копирующее присваивание заменяет старое содержимое и тоже создает независимую копию
TEST(ArrayIntTest, CopyAssignmentMakesIndependentCopy)
{
    Array<int> source;
    fillArray(source, { 1, 2, 3 });
    Array<int> target;
    fillArray(target, { 7, 8, 9, 10, 11 });

    target = source;
    target[0] = 100;

    EXPECT_TRUE(hasContents(source, { 1, 2, 3 }));
    EXPECT_TRUE(hasContents(target, { 100, 2, 3 }));
}

// Присваивание самому себе не портит содержимое
TEST(ArrayIntTest, CopyAssignmentToSelfKeepsContent)
{
    Array<int> a;
    fillArray(a, { 1, 2, 3 });
    Array<int>& same = a;

    a = same;

    EXPECT_TRUE(hasContents(a, { 1, 2, 3 }));
}

// Перемещающий конструктор забирает элементы, исходный массив остается пустым
TEST(ArrayIntTest, MoveConstructorTakesElements)
{
    Array<int> source;
    fillArray(source, { 1, 2, 3 });

    Array<int> target(std::move(source));

    EXPECT_TRUE(hasContents(target, { 1, 2, 3 }));
    EXPECT_EQ(source.size(), 0);
}

// Перемещающее присваивание забирает элементы, исходный массив остается пустым
TEST(ArrayIntTest, MoveAssignmentTakesElements)
{
    Array<int> source;
    fillArray(source, { 1, 2, 3 });
    Array<int> target;
    fillArray(target, { 7, 8 });

    target = std::move(source);

    EXPECT_TRUE(hasContents(target, { 1, 2, 3 }));
    EXPECT_EQ(source.size(), 0);
}

// Массив после перемещения можно использовать повторно
TEST(ArrayIntTest, MovedFromArrayCanBeReused)
{
    Array<int> source;
    fillArray(source, { 1, 2, 3 });
    Array<int> target(std::move(source));

    source.insert(5);

    EXPECT_TRUE(hasContents(source, { 5 }));
    EXPECT_TRUE(hasContents(target, { 1, 2, 3 }));
}

// Обход из задания обязан посетить все элементы по порядку, включая последний
TEST(ArrayIntTest, IteratorVisitsAllElementsInOrder)
{
    Array<int> a;
    fillArray(a, { 1, 2, 3, 4 });
    Ints visited;

    for (auto it = a.iterator(); it.hasNext(); it.next())
    {
        visited.push_back(it.get());
    }

    EXPECT_EQ(visited, Ints({ 1, 2, 3, 4 }));
}

// Обратный обход идет от последнего элемента к первому
TEST(ArrayIntTest, ReverseIteratorVisitsAllElementsBackwards)
{
    Array<int> a;
    fillArray(a, { 1, 2, 3, 4 });
    Ints visited;

    for (auto it = a.reverseIterator(); it.hasNext(); it.next())
    {
        visited.push_back(it.get());
    }

    EXPECT_EQ(visited, Ints({ 4, 3, 2, 1 }));
}

// У пустого массива итераторы сразу не имеют элементов
TEST(ArrayIntTest, IteratorsOnEmptyArrayHaveNoElements)
{
    Array<int> a;

    EXPECT_FALSE(a.iterator().hasNext());
    EXPECT_FALSE(a.reverseIterator().hasNext());
}

// Массив из одного элемента обходится ровно один раз в обе стороны
TEST(ArrayIntTest, IteratorsOnSingleElementArrayVisitItOnce)
{
    Array<int> a;
    a.insert(5);
    int forwardCount = 0;
    int reverseCount = 0;

    for (auto it = a.iterator(); it.hasNext(); it.next())
    {
        ++forwardCount;
    }
    for (auto it = a.reverseIterator(); it.hasNext(); it.next())
    {
        ++reverseCount;
    }

    EXPECT_EQ(forwardCount, 1);
    EXPECT_EQ(reverseCount, 1);
}

// Метод set меняет элемент массива
TEST(ArrayIntTest, IteratorSetChangesElements)
{
    Array<int> a;
    fillArray(a, { 1, 2, 3 });

    for (auto it = a.iterator(); it.hasNext(); it.next())
    {
        it.set(it.get() * 10);
    }

    EXPECT_TRUE(hasContents(a, { 10, 20, 30 }));
}

// Константный итератор читает элементы константного массива
TEST(ArrayIntTest, ConstIteratorReadsElementsOfConstArray)
{
    Array<int> a;
    fillArray(a, { 1, 2, 3 });
    const Array<int>& view = a;
    Ints visited;

    for (auto it = view.iterator(); it.hasNext(); it.next())
    {
        visited.push_back(it.get());
    }
    for (auto it = view.reverseIterator(); it.hasNext(); it.next())
    {
        visited.push_back(it.get());
    }

    EXPECT_EQ(visited, Ints({ 1, 2, 3, 3, 2, 1 }));
}

// Разыменование и автоинкремент итератора, префиксный и постфиксный
TEST(ArrayIntTest, IteratorDereferenceAndIncrementOperators)
{
    Array<int> a;
    fillArray(a, { 1, 2, 3 });

    auto it = a.iterator();
    EXPECT_EQ(*it, 1);

    ++it;
    EXPECT_EQ(*it, 2);

    auto previous = it++;
    EXPECT_EQ(*previous, 2);
    EXPECT_EQ(*it, 3);

    *it = 30;
    EXPECT_EQ(a[2], 30);

    ++it;
    EXPECT_FALSE(it.hasNext());
}

// Цикл range-for работает для обычного и константного массива
TEST(ArrayIntTest, RangeForVisitsAndModifiesElements)
{
    Array<int> a;
    fillArray(a, { 1, 2, 3 });

    for (int& x : a)
    {
        x *= 2;
    }

    int sum = 0;
    const Array<int>& view = a;
    for (int x : view)
    {
        sum += x;
    }

    EXPECT_TRUE(hasContents(a, { 2, 4, 6 }));
    EXPECT_EQ(sum, 12);
}

// Пример использования из задания: числа от 1 до 10, каждое умножить на 2
TEST(ArrayIntTest, TaskExampleDoublesNumbersFromOneToTen)
{
    Array<int> a;
    for (int i = 0; i < 10; ++i)
    {
        a.insert(i + 1);
    }

    for (int i = 0; i < a.size(); ++i)
    {
        a[i] *= 2;
    }

    Ints printed;
    for (auto it = a.iterator(); it.hasNext(); it.next())
    {
        printed.push_back(it.get());
    }

    EXPECT_EQ(printed, Ints({ 2, 4, 6, 8, 10, 12, 14, 16, 18, 20 }));
}

// Случайные вставки и удаления дают тот же результат, что и std::vector
TEST(ArrayIntTest, RandomOperationsMatchStdVector)
{
    checkAgainstVector<int>([](int step)
        {
            return step * 7;
        });
}

// =====================================================================
// Группа 2: Array<std::string>, нетривиальный тип с динамической памятью
// =====================================================================

// Вставка строк, в том числе пустой
TEST(ArrayStringTest, InsertStoresStringsIncludingEmptyOne)
{
    Array<std::string> names;

    EXPECT_EQ(names.insert(longString("first")), 0);
    EXPECT_EQ(names.insert(std::string()), 1);
    EXPECT_EQ(names.insert(std::string("short")), 2);

    EXPECT_TRUE(hasContents(names, { longString("first"), std::string(), std::string("short") }));
}

// Вставка в начало и в середину корректно сдвигает строки
TEST(ArrayStringTest, InsertToBeginningAndMiddleShiftsStrings)
{
    Array<std::string> names;
    fillArray(names, { longString("b"), longString("d") });

    names.insert(0, longString("a"));
    names.insert(2, longString("c"));

    EXPECT_TRUE(hasContents(names, { longString("a"), longString("b"), longString("c"), longString("d") }));
}

// Удаление строк сдвигает оставшиеся влево, содержимое не портится
TEST(ArrayStringTest, RemoveShiftsStringsLeftAndKeepsContent)
{
    Array<std::string> names;
    fillArray(names, { longString("a"), longString("b"), longString("c"), longString("d") });

    names.remove(1);
    names.remove(2);
    names.remove(0);

    EXPECT_TRUE(hasContents(names, { longString("c") }));
}

// Перевыделение памяти переносит все строки без потери содержимого
TEST(ArrayStringTest, GrowthKeepsAllStrings)
{
    Array<std::string> names(2);

    for (int i = 0; i < 100; ++i)
    {
        names.insert(longString(std::to_string(i)));
    }

    ASSERT_EQ(names.size(), 100);

    for (int i = 0; i < 100; ++i)
    {
        EXPECT_EQ(names[i], longString(std::to_string(i)));
    }
}

// Вставка ссылки на собственный элемент в полный массив, ссылка не должна стать недействительной
TEST(ArrayStringTest, InsertOwnElementWhenArrayIsFull)
{
    const std::string first = longString("first");
    const std::string second = longString("second");
    Array<std::string> names(2);
    fillArray(names, { first, second });
    ASSERT_EQ(names.size(), names.capacity());

    names.insert(0, names[1]);

    EXPECT_TRUE(hasContents(names, { second, first, second }));
}

// Вставка ссылки на собственный элемент со сдвигом и в конец
TEST(ArrayStringTest, InsertOwnElementWithShiftAndAtEnd)
{
    const std::string first = longString("first");
    const std::string second = longString("second");
    Array<std::string> names(10);
    fillArray(names, { first, second });

    names.insert(0, names[1]);
    names.insert(names[0]);

    EXPECT_TRUE(hasContents(names, { second, first, second, second }));
}

// Оператор индексирования позволяет изменять строку на месте
TEST(ArrayStringTest, IndexOperatorModifiesStringInPlace)
{
    Array<std::string> names;
    fillArray(names, { longString("a"), longString("b") });

    names[0] += "!";
    names[1].clear();

    EXPECT_TRUE(hasContents(names, { longString("a") + "!", std::string() }));
}

// Копирующий конструктор делает глубокую копию, строки не разделяют память
TEST(ArrayStringTest, CopyConstructorMakesDeepCopy)
{
    Array<std::string> original;
    fillArray(original, { longString("a"), longString("b") });

    Array<std::string> copy(original);
    copy[0] += " changed";

    EXPECT_EQ(original[0], longString("a"));
    EXPECT_EQ(copy[0], longString("a") + " changed");
    EXPECT_NE(&original[0], &copy[0]);
}

// Копирующее присваивание делает глубокую копию и заменяет старое содержимое
TEST(ArrayStringTest, CopyAssignmentMakesDeepCopy)
{
    Array<std::string> source;
    fillArray(source, { longString("a"), longString("b") });
    Array<std::string> target;
    fillArray(target, { longString("x"), longString("y"), longString("z") });

    target = source;
    target[1] += " changed";

    EXPECT_TRUE(hasContents(source, { longString("a"), longString("b") }));
    EXPECT_TRUE(hasContents(target, { longString("a"), longString("b") + " changed" }));
}

// Присваивание самому себе не портит строки
TEST(ArrayStringTest, CopyAssignmentToSelfKeepsStrings)
{
    Array<std::string> names;
    fillArray(names, { longString("a"), longString("b") });
    Array<std::string>& same = names;

    names = same;

    EXPECT_TRUE(hasContents(names, { longString("a"), longString("b") }));
}

// Перемещающий конструктор забирает строки, исходный массив остается пустым
TEST(ArrayStringTest, MoveConstructorMovesStrings)
{
    Array<std::string> source;
    fillArray(source, { longString("a"), longString("b") });

    Array<std::string> target(std::move(source));

    EXPECT_TRUE(hasContents(target, { longString("a"), longString("b") }));
    EXPECT_EQ(source.size(), 0);
}

// Перемещающее присваивание забирает строки, исходный массив остается пустым
TEST(ArrayStringTest, MoveAssignmentMovesStrings)
{
    Array<std::string> source;
    fillArray(source, { longString("a"), longString("b") });
    Array<std::string> target;
    fillArray(target, { longString("x") });

    target = std::move(source);

    EXPECT_TRUE(hasContents(target, { longString("a"), longString("b") }));
    EXPECT_EQ(source.size(), 0);
}

// Метод set итератора заменяет строки
TEST(ArrayStringTest, IteratorSetReplacesStrings)
{
    Array<std::string> names;
    fillArray(names, { longString("a"), longString("b") });

    for (auto it = names.iterator(); it.hasNext(); it.next())
    {
        it.set(it.get() + "!");
    }

    EXPECT_TRUE(hasContents(names, { longString("a") + "!", longString("b") + "!" }));
}

// Прямой и обратный обход возвращают строки в нужном порядке
TEST(ArrayStringTest, IteratorsVisitStringsInBothDirections)
{
    Array<std::string> names;
    fillArray(names, { std::string("a"), std::string("b"), std::string("c") });
    Strings forward;
    Strings backward;

    for (auto it = names.iterator(); it.hasNext(); it.next())
    {
        forward.push_back(it.get());
    }
    for (auto it = names.reverseIterator(); it.hasNext(); it.next())
    {
        backward.push_back(it.get());
    }

    EXPECT_EQ(forward, Strings({ "a", "b", "c" }));
    EXPECT_EQ(backward, Strings({ "c", "b", "a" }));
}

// Константный итератор читает строки константного массива
TEST(ArrayStringTest, ConstIteratorReadsStrings)
{
    Array<std::string> names;
    fillArray(names, { longString("a"), longString("b") });
    const Array<std::string>& view = names;
    Strings visited;

    for (auto it = view.iterator(); it.hasNext(); it.next())
    {
        visited.push_back(it.get());
    }

    EXPECT_EQ(visited, Strings({ longString("a"), longString("b") }));
}

// Цикл range-for позволяет менять строки по ссылке
TEST(ArrayStringTest, RangeForModifiesStrings)
{
    Array<std::string> names;
    fillArray(names, { longString("a"), longString("b") });

    for (std::string& name : names)
    {
        name += "!";
    }

    EXPECT_TRUE(hasContents(names, { longString("a") + "!", longString("b") + "!" }));
}

// Случайные вставки и удаления строк дают тот же результат, что и std::vector
TEST(ArrayStringTest, RandomOperationsMatchStdVector)
{
    checkAgainstVector<std::string>([](int step)
        {
            return longString(std::to_string(step));
        });
}

// Деструктор массива уничтожает все элементы
TEST_F(ArrayLifetimeTest, DestructorDestroysAllElements)
{
    {
        Array<Counted> a;
        a.insert(Counted(1));
        a.insert(Counted(2));
        a.insert(Counted(3));

        EXPECT_EQ(Counted::aliveCount, 3);
    }

    EXPECT_EQ(Counted::aliveCount, 0);
}

// Удаление элемента сразу уничтожает его
TEST_F(ArrayLifetimeTest, RemoveDestroysRemovedElement)
{
    Array<Counted> a;
    a.insert(Counted(1));
    a.insert(Counted(2));
    a.insert(Counted(3));

    a.remove(1);

    EXPECT_EQ(Counted::aliveCount, 2);
    EXPECT_EQ(a[0].value, 1);
    EXPECT_EQ(a[1].value, 3);
}

// Сдвиги и перевыделения не создают лишних объектов и не теряют существующие
TEST_F(ArrayLifetimeTest, ShiftsAndGrowthKeepExactlyOneObjectPerElement)
{
    Array<Counted> a(2);

    for (int i = 0; i < 50; ++i)
    {
        a.insert(0, Counted(i));
        ASSERT_EQ(Counted::aliveCount, a.size());
    }

    for (int i = 0; i < 25; ++i)
    {
        a.remove(a.size() / 2);
        ASSERT_EQ(Counted::aliveCount, a.size());
    }
}

// Копирование создает новые объекты, а уничтожение копии удаляет только их
TEST_F(ArrayLifetimeTest, CopyCreatesNewObjectsAndDestroysOnlyThem)
{
    Array<Counted> original;
    original.insert(Counted(1));
    original.insert(Counted(2));
    original.insert(Counted(3));

    {
        Array<Counted> copy(original);

        EXPECT_EQ(Counted::aliveCount, 6);
    }

    EXPECT_EQ(Counted::aliveCount, 3);
}

// Перемещающий конструктор передает объекты без создания новых
TEST_F(ArrayLifetimeTest, MoveConstructorDoesNotCreateNewObjects)
{
    Array<Counted> source;
    source.insert(Counted(1));
    source.insert(Counted(2));

    Array<Counted> target(std::move(source));

    EXPECT_EQ(Counted::aliveCount, 2);
    EXPECT_EQ(target.size(), 2);
}

// Присваивание уничтожает старое содержимое получателя
TEST_F(ArrayLifetimeTest, AssignmentDestroysOldContentOfTarget)
{
    Array<Counted> source;
    source.insert(Counted(1));
    source.insert(Counted(2));
    source.insert(Counted(3));
    Array<Counted> target;
    target.insert(Counted(10));
    target.insert(Counted(20));
    ASSERT_EQ(Counted::aliveCount, 5);

    target = source;
    EXPECT_EQ(Counted::aliveCount, 6);

    Array<Counted> other;
    other.insert(Counted(7));
    target = std::move(other);
    EXPECT_EQ(Counted::aliveCount, 4);
}

class CopyOnly
{
public:
    int value;

    explicit CopyOnly(int v) : value(v)
    {
    }

    CopyOnly(const CopyOnly& other) : value(other.value)
    {
    }

    CopyOnly(CopyOnly&&) = delete;

    CopyOnly& operator=(const CopyOnly&) = delete;
    CopyOnly& operator=(CopyOnly&&) = delete;
};

TEST(ArrayCopyOnlyTest, InsertRemoveAndGrowthUseCopyConstruction)
{
    Array<CopyOnly> a(1);

    CopyOnly first(1);
    CopyOnly second(2);
    CopyOnly third(3);

    // Вставка первого элемента
    a.insert(first);

    // Перевыделение памяти
    a.insert(second);

    // Вставка в середину со сдвигом элементов
    a.insert(1, third);

    ASSERT_EQ(a.size(), 3);

    EXPECT_EQ(a[0].value, 1);
    EXPECT_EQ(a[1].value, 3);
    EXPECT_EQ(a[2].value, 2);

    // Удаление со сдвигом элементов
    a.remove(1);

    ASSERT_EQ(a.size(), 2);

    EXPECT_EQ(a[0].value, 1);
    EXPECT_EQ(a[1].value, 2);
}