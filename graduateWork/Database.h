#ifndef DATABASE_H
#define DATABASE_H

#include <iostream>
#include <vector>
#include <string>
#include <pqxx/pqxx>
#include <shared_mutex>
#include "Config.h"

class Database
{
private:
    std::string connectionString_;  // ← Храним строку подключения, а не само соединение
    mutable std::shared_mutex databaseMutex_;

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

    DatabaseStats getStatistics();

    // Проверка соединения
    bool isConnected() const;

    // Вспомогательный метод для создания соединения
    pqxx::connection createConnection() const;

    // Отчистка данных
    void deleteAllDocuments();

private:
    // Вспомогательный метод для выполнения операций в транзакции
    template<typename Func>
    auto executeTransaction(Func&& func) -> decltype(func(std::declval<pqxx::work&>()));
};

#endif // !DATABASE_H