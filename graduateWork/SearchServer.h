#ifndef SEARCHSERVER_H
#define SEARCHSERVER_H

#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <boost/asio.hpp>
#include "Config.h"
#include "Database.h"

using boost::asio::ip::tcp;

class SearchServer
{
private:
    Config& config_;
    Database& database_;
    std::atomic<bool> stopRequested_;
    std::thread serverThread_;

    // Основная функция сервера
    void runServer();

    // Обработка соединения
    void handleConnection(tcp::socket socket);

    // Обработка HTTP запроса
    std::string handleRequest(const std::string& request);

    // Генерация HTML страницы с формой поиска
    std::string generateSearchPage();

    // Генерация HTML страницы с результатами
    std::string generateResultsPage(const std::vector<Database::SearchResult>& results,
        const std::string& query);

    // Генерация HTML страницы с ошибкой
    std::string generateErrorPage(const std::string& error);

    // Парсинг поискового запроса
    std::vector<std::string> parseQuery(const std::string& query);

    // HTML шаблоны
    const std::string htmlHeader = R"(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Поисковая система</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; background: #f5f5f5; }
        .container { max-width: 800px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #333; text-align: center; }
        .search-form { display: flex; margin: 30px 0; }
        .search-input { flex-grow: 1; padding: 12px; font-size: 16px; border: 2px solid #ddd; border-radius: 5px 0 0 5px; }
        .search-button { padding: 12px 24px; font-size: 16px; background: #4285f4; color: white; border: none; border-radius: 0 5px 5px 0; cursor: pointer; }
        .search-button:hover { background: #3367d6; }
        .result { margin: 20px 0; padding: 15px; border-left: 4px solid #4285f4; background: #f8f9fa; }
        .result-title { font-size: 18px; font-weight: bold; margin-bottom: 5px; }
        .result-title a { color: #1a0dab; text-decoration: none; }
        .result-title a:hover { text-decoration: underline; }
        .result-url { color: #006621; font-size: 14px; margin-bottom: 5px; }
        .result-relevance { color: #70757a; font-size: 12px; }
        .no-results { text-align: center; color: #70757a; padding: 40px; }
        .error { color: #d93025; padding: 20px; background: #fce8e6; border-radius: 5px; }
        .stats { text-align: center; color: #70757a; font-size: 14px; margin-top: 30px; }
        .back-link { display: inline-block; margin: 20px 0; color: #4285f4; text-decoration: none; }
        .back-link:hover { text-decoration: underline; }
    </style>
</head>
<body>
    <div class="container">
)";

    const std::string htmlFooter = R"(
    </div>
</body>
</html>
)";

    // Вспомогательные методы
    std::string formatHttpResponse(int statusCode, const std::string& statusText,
        const std::string& contentType,
        const std::string& content);
    std::string parsePostBody(const std::string& body);
    std::string urlDecode(const std::string& encoded);

public:
    SearchServer(Config& config, Database& db);
    ~SearchServer();

    // Запуск сервера
    void start();

    // Остановка сервера
    void stop();

    // Проверка, работает ли сервер
    bool isRunning() const;
};

#endif // SEARCHSERVER_H