// Решите загадку: Сколько чисел от 1 до 1000 содержат как минимум одну цифру 3?
// Напишите ответ здесь:
#include <iostream>
#include <string>

int main()
{
    int count {0};
    for (int i = 1; i <= 1000; ++i) {
        if (std::to_string(i).find('3') != std::string::npos)
            count++;
    }
    std::cout << "kol-vo vhojdeniy 3 : " << count << std::endl;

    return 0;
}
// Закомитьте изменения и отправьте их в свой репозиторий.
