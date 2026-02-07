#ifndef DATABASE_H
#define DATABASE_H

#include <iostream>
#include <vector>
#include <pqxx/pqxx>
#include <shared_mutex>
#include "Config.h"

class Database
{
private:
    pqxx::connection database_;
    mutable std::shared_mutex databaseMutex_;  // <-- делаем mutable

public:
    // Конструктор с подключением к БД
    Database(const Config& config);

    // Создание таблиц
    void creatingTables();

    // Сохранение документа (возвращает ID)
    int savingDocument(const std::string& url, const std::string& title, const std::string& content);

    // Сохранение слов и их частоты
    void savingWords(int documentId, const std::vector<std::pair<std::string, int>>& wordsAndFrequency);

    // Проверка существует ли URL
    bool urlExists(const std::string& url);

    // Получить ID документа по URL
    int getDocumentIdByUrl(const std::string& url);

    // Получить ID слова
    int getWordId(const std::string& word);

    // Поиск документов по словам
    struct SearchResult
    {
        std::string url;
        std::string title;
        int relevance;
    };

    std::vector<SearchResult> searchDocuments(const std::vector<std::string>& words, int limit = 10);

    // Получить все документы (для отладки)
    std::vector<std::tuple<int, std::string, std::string>> getAllDocuments();

    // Получить все слова документа
    std::vector<std::pair<std::string, int>> getWordsByDocumentId(int documentId);

    // Удалить документ (для очистки)
    void deleteDocument(int documentId);

    // Получить статистику
    struct DatabaseStats
    {
        int documentsCount;
        int wordsCount;
        int totalRelations;
    };

    DatabaseStats getStatistics() const;  // <-- добавляем const

    // Проверка соединения
    bool isConnected() const;
};

#endif // !DATABASE_H