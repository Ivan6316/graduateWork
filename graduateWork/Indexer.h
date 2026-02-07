#ifndef INDEXER_H
#define INDEXER_H

#include <string>
#include <vector>
#include <utility>

class Indexer
{
public:
    Indexer() = default;

    // Структура для результатов индексации
    struct IndexingResult
    {
        std::string title;
        std::string cleanContent;
        std::vector<std::pair<std::string, int>> wordsFrequency;
    };

    // Основная функция индексации
    IndexingResult indexPage(const std::string& html, const std::string& url);

private:
    // Вспомогательные методы
    std::string cleanHtml(const std::string& html);
    std::string extractTitle(const std::string& html);
    std::string normalizeWord(const std::string& word);
    std::vector<std::pair<std::string, int>> countWords(const std::string& text);
};

#endif // INDEXER_H