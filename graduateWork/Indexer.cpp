#include "Indexer.h"
#include <regex>
#include <algorithm>
#include <iostream>
#include <locale>
#include <cctype>
#include <unordered_map>
#include <sstream>

std::string Indexer::cleanHtml(const std::string& html)
{
    std::string result = html;

    try
    {
        // Удаляем содержимое script и style тегов
        // Без dotall флага, используем более простое регулярное выражение
        std::regex scriptRegex(R"(<script\b[^>]*>.*?</script>)", std::regex::icase);
        result = std::regex_replace(result, scriptRegex, " ");

        std::regex styleRegex(R"(<style\b[^>]*>.*?</style>)", std::regex::icase);
        result = std::regex_replace(result, styleRegex, " ");

        // Заменяем HTML-сущности
        std::regex nbspRegex("&nbsp;", std::regex::icase);
        result = std::regex_replace(result, nbspRegex, " ");

        std::regex ampRegex("&amp;", std::regex::icase);
        result = std::regex_replace(result, ampRegex, "&");

        std::regex ltRegex("&lt;", std::regex::icase);
        result = std::regex_replace(result, ltRegex, "<");

        std::regex gtRegex("&gt;", std::regex::icase);
        result = std::regex_replace(result, gtRegex, ">");

        std::regex quotRegex("&quot;", std::regex::icase);
        result = std::regex_replace(result, quotRegex, "\"");

        // Удаляем HTML теги
        std::regex tagRegex("<[^>]*>");
        result = std::regex_replace(result, tagRegex, " ");

        // Заменяем спецсимволы на пробелы
        std::regex specialRegex("[\\t\\n\\r]+");
        result = std::regex_replace(result, specialRegex, " ");

        // Удаляем лишние знаки препинания (оставляем только буквы, цифры и пробелы)
        std::regex punctRegex("[^\\w\\sа-яА-ЯёЁ]");
        result = std::regex_replace(result, punctRegex, " ");

        // Удаляем множественные пробелы
        std::regex spaceRegex("\\s+");
        result = std::regex_replace(result, spaceRegex, " ");

        // Обрезаем пробелы в начале и конце
        result.erase(0, result.find_first_not_of(" \t\n\r"));
        result.erase(result.find_last_not_of(" \t\n\r") + 1);

        return result;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Ошибка при очистке HTML: " << e.what() << std::endl;
        return result;
    }
}

std::string Indexer::extractTitle(const std::string& html)
{
    try
    {
        // Ищем тег <title> - без dotall
        size_t titleStart = html.find("<title");
        if (titleStart != std::string::npos)
        {
            titleStart = html.find('>', titleStart);
            if (titleStart != std::string::npos)
            {
                size_t titleEnd = html.find("</title>", titleStart);
                if (titleEnd != std::string::npos)
                {
                    std::string title = html.substr(titleStart + 1, titleEnd - titleStart - 1);

                    // Очищаем заголовок от HTML тегов
                    std::regex tagRegex("<[^>]*>");
                    title = std::regex_replace(title, tagRegex, " ");

                    // Удаляем лишние пробелы
                    std::regex spaceRegex("\\s+");
                    title = std::regex_replace(title, spaceRegex, " ");

                    // Обрезаем пробелы
                    title.erase(0, title.find_first_not_of(" \t\n\r"));
                    title.erase(title.find_last_not_of(" \t\n\r") + 1);

                    if (!title.empty())
                    {
                        return title;
                    }
                }
            }
        }

        // Если title не найден, ищем h1
        size_t h1Start = html.find("<h1");
        if (h1Start != std::string::npos)
        {
            h1Start = html.find('>', h1Start);
            if (h1Start != std::string::npos)
            {
                size_t h1End = html.find("</h1>", h1Start);
                if (h1End != std::string::npos)
                {
                    std::string h1 = html.substr(h1Start + 1, h1End - h1Start - 1);

                    std::regex tagRegex("<[^>]*>");
                    h1 = std::regex_replace(h1, tagRegex, " ");

                    std::regex spaceRegex("\\s+");
                    h1 = std::regex_replace(h1, spaceRegex, " ");

                    h1.erase(0, h1.find_first_not_of(" \t\n\r"));
                    h1.erase(h1.find_last_not_of(" \t\n\r") + 1);

                    if (!h1.empty())
                    {
                        return h1;
                    }
                }
            }
        }

        return "Без заголовка";
    }
    catch (const std::exception& e)
    {
        std::cerr << "Ошибка при извлечении заголовка: " << e.what() << std::endl;
        return "Без заголовка";
    }
}

std::string Indexer::normalizeWord(const std::string& word)
{
    if (word.empty())
    {
        return "";
    }

    std::string normalized;
    normalized.reserve(word.size());

    // Приводим к нижнему регистру
    for (char c : word)
    {
        // Для русских букв
        if (c >= 'А' && c <= 'Я')
        {
            normalized.push_back(c + 32); // 'а' - 'А' = 32
        }
        else if (c == 'Ё')
        {
            normalized.push_back('ё');
        }
        // Для английских букв
        else if (c >= 'A' && c <= 'Z')
        {
            normalized.push_back(c + 32); // 'a' - 'A' = 32
        }
        else
        {
            normalized.push_back(std::tolower(static_cast<unsigned char>(c)));
        }
    }

    return normalized;
}

std::vector<std::pair<std::string, int>> Indexer::countWords(const std::string& text)
{
    std::vector<std::pair<std::string, int>> result;

    if (text.empty())
    {
        return result;
    }

    try
    {
        // Временный map для подсчета
        std::unordered_map<std::string, int> wordCount;

        // Разбиваем текст на слова
        std::istringstream stream(text);
        std::string word;

        while (stream >> word)
        {
            // Нормализуем слово
            std::string normalized = normalizeWord(word);

            // Фильтруем короткие/длинные слова и слова только из цифр
            if (normalized.length() >= 3 && normalized.length() <= 32)
            {
                // Проверяем, что слово содержит хотя бы одну букву
                bool hasLetter = false;
                for (char c : normalized)
                {
                    if (std::isalpha(static_cast<unsigned char>(c)))
                    {
                        hasLetter = true;
                        break;
                    }
                }

                if (hasLetter)
                {
                    wordCount[normalized]++;
                }
            }
        }

        // Преобразуем map в vector
        result.reserve(wordCount.size());
        for (const auto& [word, count] : wordCount)
        {
            result.emplace_back(word, count);
        }

        // Сортируем по частоте (по убыванию)
        std::sort(result.begin(), result.end(),
            [](const auto& a, const auto& b) {
                return a.second > b.second;
            });

        return result;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Ошибка при подсчёте слов: " << e.what() << std::endl;
        return result;
    }
}

Indexer::IndexingResult Indexer::indexPage(const std::string& html, const std::string& url)
{
    IndexingResult result;

    try
    {
        std::cout << "⌛ Индексация страницы: " << url << std::endl;

        // Извлекаем заголовок
        result.title = extractTitle(html);
        if (result.title == "Без заголовка")
        {
            // Используем часть URL как заголовок
            size_t lastSlash = url.find_last_of('/');
            if (lastSlash != std::string::npos && lastSlash < url.length() - 1)
            {
                result.title = url.substr(lastSlash + 1);

                // Убираем параметры после ?
                size_t questionPos = result.title.find('?');
                if (questionPos != std::string::npos)
                {
                    result.title = result.title.substr(0, questionPos);
                }
            }
            else
            {
                result.title = url;
            }
        }

        // Очищаем HTML
        result.cleanContent = cleanHtml(html);

        // Подсчитываем слова
        result.wordsFrequency = countWords(result.cleanContent);

        std::cout << "✔ Страница проиндексирована: " << url << std::endl;
        std::cout << "  Заголовок: " << result.title << std::endl;
        std::cout << "  Найдено уникальных слов: " << result.wordsFrequency.size() << std::endl;

        return result;
    }
    catch (const std::exception& e)
    {
        std::cerr << "❌ Ошибка при индексации страницы " << url << ": " << e.what() << std::endl;
        result.title = "Ошибка индексации";
        return result;
    }
}