#include "SearchServer.h"
#include <regex>
#include <sstream>
#include <iostream>

SearchServer::SearchServer(Config& config, Database& db)
    : config_(config)
    , database_(db)
    , stopRequested_(false)
{
}

SearchServer::~SearchServer()
{
    stop();
}

void SearchServer::runServer()
{
    try
    {
        boost::asio::io_context io_context;
        tcp::acceptor acceptor(io_context,
            tcp::endpoint(tcp::v4(), config_.getSearcherPort()));

        std::cout << "🌐 Поисковый сервер запущен на порту " << config_.getSearcherPort() << std::endl;
        std::cout << "   Для поиска откройте: http://localhost:" << config_.getSearcherPort() << std::endl;

        while (!stopRequested_)
        {
            tcp::socket socket(io_context);
            boost::system::error_code ec;

            acceptor.accept(socket, ec);
            if (ec)
            {
                if (!stopRequested_)
                {
                    std::cerr << "Ошибка accept: " << ec.message() << std::endl;
                }
                continue;
            }

            // Обрабатываем соединение
            handleConnection(std::move(socket));
        }
    }
    catch (const std::exception& e)
    {
        if (!stopRequested_)
        {
            std::cerr << "❌ Ошибка сервера: " << e.what() << std::endl;
        }
    }
}

void SearchServer::handleConnection(tcp::socket socket)
{
    try
    {
        // Читаем запрос
        boost::asio::streambuf buffer;
        boost::asio::read_until(socket, buffer, "\r\n\r\n");

        std::istream request_stream(&buffer);
        std::string request;
        std::getline(request_stream, request, '\0');

        // Обрабатываем запрос
        std::string response = handleRequest(request);

        // Отправляем ответ
        boost::asio::write(socket, boost::asio::buffer(response));

        socket.shutdown(tcp::socket::shutdown_both);
        socket.close();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Ошибка при обработке соединения: " << e.what() << std::endl;
    }
}

std::string SearchServer::handleRequest(const std::string& request)
{
    try
    {
        std::istringstream request_stream(request);
        std::string method, path;
        request_stream >> method >> path;

        std::cout << "📥 HTTP запрос: " << method << " " << path << std::endl;

        if (method == "GET")
        {
            if (path == "/" || path == "/search" || path == "/index.html")
            {
                // Главная страница с формой поиска
                std::string html = generateSearchPage();

                std::string response =
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/html; charset=utf-8\r\n"
                    "Content-Length: " + std::to_string(html.size()) + "\r\n"
                    "Connection: close\r\n"
                    "\r\n" + html;

                return response;
            }
            else
            {
                // 404 Not Found
                std::string error = "<h1>404 Not Found</h1><p>Страница " + path + " не найдена</p>";

                std::string response =
                    "HTTP/1.1 404 Not Found\r\n"
                    "Content-Type: text/html; charset=utf-8\r\n"
                    "Content-Length: " + std::to_string(error.size()) + "\r\n"
                    "Connection: close\r\n"
                    "\r\n" + error;

                return response;
            }
        }
        else if (method == "POST" && path == "/search")
        {
            // Читаем тело запроса
            std::string body;
            std::string line;
            while (std::getline(request_stream, line) && !line.empty())
            {
                // Пропускаем заголовки
            }

            // Читаем тело
            while (std::getline(request_stream, line))
            {
                body += line + "\n";
            }

            // Парсим параметры формы
            std::regex paramRegex(R"(query=([^&]+))");
            std::smatch match;

            std::string query;
            if (std::regex_search(body, match, paramRegex) && match.size() > 1)
            {
                query = match[1].str();

                // Декодируем URL-encoded строку
                std::regex plusRegex("\\+");
                query = std::regex_replace(query, plusRegex, " ");

                // Декодируем %20 в пробелы
                std::regex percentRegex("%20");
                query = std::regex_replace(query, percentRegex, " ");
            }

            if (query.empty())
            {
                std::string html = generateErrorPage("Пустой поисковый запрос");

                std::string response =
                    "HTTP/1.1 400 Bad Request\r\n"
                    "Content-Type: text/html; charset=utf-8\r\n"
                    "Content-Length: " + std::to_string(html.size()) + "\r\n"
                    "Connection: close\r\n"
                    "\r\n" + html;

                return response;
            }

            // Парсим запрос
            std::vector<std::string> words = parseQuery(query);

            if (words.empty())
            {
                std::string html = generateErrorPage("После обработки запрос стал пустым");

                std::string response =
                    "HTTP/1.1 400 Bad Request\r\n"
                    "Content-Type: text/html; charset=utf-8\r\n"
                    "Content-Length: " + std::to_string(html.size()) + "\r\n"
                    "Connection: close\r\n"
                    "\r\n" + html;

                return response;
            }

            if (words.size() > 4)
            {
                std::string html = generateErrorPage("Слишком много слов в запросе (максимум 4)");

                std::string response =
                    "HTTP/1.1 400 Bad Request\r\n"
                    "Content-Type: text/html; charset=utf-8\r\n"
                    "Content-Length: " + std::to_string(html.size()) + "\r\n"
                    "Connection: close\r\n"
                    "\r\n" + html;

                return response;
            }

            // Выполняем поиск
            std::vector<Database::SearchResult> results;
            try
            {
                results = database_.searchDocuments(words, 10);
            }
            catch (const std::exception& e)
            {
                std::cerr << "Ошибка поиска в БД: " << e.what() << std::endl;
                std::string html = generateErrorPage("Ошибка при поиске в базе данных");

                std::string response =
                    "HTTP/1.1 500 Internal Server Error\r\n"
                    "Content-Type: text/html; charset=utf-8\r\n"
                    "Content-Length: " + std::to_string(html.size()) + "\r\n"
                    "Connection: close\r\n"
                    "\r\n" + html;

                return response;
            }

            // Генерируем страницу с результатами
            std::string html = generateResultsPage(results, query);

            std::string response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html; charset=utf-8\r\n"
                "Content-Length: " + std::to_string(html.size()) + "\r\n"
                "Connection: close\r\n"
                "\r\n" + html;

            return response;
        }
        else
        {
            // 405 Method Not Allowed
            std::string error = "<h1>405 Method Not Allowed</h1><p>Метод " + method + " не поддерживается</p>";

            std::string response =
                "HTTP/1.1 405 Method Not Allowed\r\n"
                "Content-Type: text/html; charset=utf-8\r\n"
                "Content-Length: " + std::to_string(error.size()) + "\r\n"
                "Connection: close\r\n"
                "\r\n" + error;

            return response;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Ошибка обработки запроса: " << e.what() << std::endl;

        std::string error = "<h1>500 Internal Server Error</h1><p>" + std::string(e.what()) + "</p>";

        std::string response =
            "HTTP/1.1 500 Internal Server Error\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: " + std::to_string(error.size()) + "\r\n"
            "Connection: close\r\n"
            "\r\n" + error;

        return response;
    }
}

std::vector<std::string> SearchServer::parseQuery(const std::string& query)
{
    std::vector<std::string> words;
    std::istringstream stream(query);
    std::string word;

    while (stream >> word)
    {
        // Очищаем слово от знаков препинания
        word.erase(std::remove_if(word.begin(), word.end(),
            [](char c) {
                return std::ispunct(static_cast<unsigned char>(c)) && c != '-';
            }),
            word.end());

        // Приводим к нижнему регистру
        std::transform(word.begin(), word.end(), word.begin(),
            [](unsigned char c) { return std::tolower(c); });

        // Фильтруем короткие слова
        if (word.length() >= 3 && word.length() <= 32)
        {
            // Проверяем, что слово содержит хотя бы одну букву
            bool hasLetter = false;
            for (char c : word)
            {
                if (std::isalpha(static_cast<unsigned char>(c)))
                {
                    hasLetter = true;
                    break;
                }
            }

            if (hasLetter)
            {
                words.push_back(word);
            }
        }
    }

    return words;
}

std::string SearchServer::generateSearchPage()
{
    std::stringstream html;

    html << htmlHeader;
    html << R"(
        <h1>🔍 Поисковая система</h1>
        <form method="POST" action="/search" class="search-form">
            <input type="text" name="query" placeholder="Введите поисковый запрос..." 
                   class="search-input" required>
            <button type="submit" class="search-button">Найти</button>
        </form>
        <div class="stats">
            <p>Примеры запросов: программирование, web разработка, база данных</p>
            <p>Максимум 4 слова в запросе</p>
            <p>Минимальная длина слова: 3 символа</p>
        </div>
    )";
    html << htmlFooter;

    return html.str();
}

std::string SearchServer::generateResultsPage(
    const std::vector<Database::SearchResult>& results,
    const std::string& query)
{
    std::stringstream html;

    html << htmlHeader;
    html << R"(
        <h1>🔍 Результаты поиска</h1>
        <form method="POST" action="/search" class="search-form">
            <input type="text" name="query" value=")" << query << R"(" 
                   class="search-input">
            <button type="submit" class="search-button">Найти</button>
        </form>
        <a href="/" class="back-link">← Новый поиск</a>
    )";

    if (results.empty())
    {
        html << R"(
            <div class="no-results">
                <h2>😕 Ничего не найдено</h2>
                <p>По запросу ")" << query << R"(" ничего не найдено.</p>
                <p>Попробуйте:</p>
                <ul>
                    <li>Проверить правильность написания</li>
                    <li>Использовать другие слова</li>
                    <li>Упростить запрос</li>
                </ul>
            </div>
        )";
    }
    else
    {
        html << R"(<h2>Найдено результатов: )" << results.size() << R"(</h2>)";

        for (size_t i = 0; i < results.size(); ++i)
        {
            const auto& result = results[i];

            html << R"(
                <div class="result">
                    <div class="result-title">
                        <a href=")" << result.url << R"(" target="_blank">)"
                << result.title << R"(</a>
                    </div>
                    <div class="result-url">)" << result.url << R"(</div>
                    <div class="result-relevance">
                        Релевантность: )" << result.relevance << R"( | 
                        Результат #)" << (i + 1) << R"(
                    </div>
                </div>
            )";
        }
    }

    html << htmlFooter;
    return html.str();
}

std::string SearchServer::generateErrorPage(const std::string& error)
{
    std::stringstream html;

    html << htmlHeader;
    html << R"(
        <div class="error">
            <h2>❌ Ошибка</h2>
            <p>)" << error << R"(</p>
            <a href="/" class="back-link">← Вернуться к поиску</a>
        </div>
    )";
    html << htmlFooter;

    return html.str();
}

void SearchServer::start()
{
    if (serverThread_.joinable())
    {
        return;
    }

    stopRequested_ = false;
    serverThread_ = std::thread(&SearchServer::runServer, this);
}

void SearchServer::stop()
{
    stopRequested_ = true;

    if (serverThread_.joinable())
    {
        serverThread_.join();
    }
}

bool SearchServer::isRunning() const
{
    return serverThread_.joinable() && !stopRequested_;
}