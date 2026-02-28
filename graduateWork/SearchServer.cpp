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
        // Разбираем запрос вручную
        size_t headerEnd = request.find("\r\n\r\n");
        if (headerEnd == std::string::npos) {
            // Исправлено: используем generateErrorPage вместо generateErrorResponse
            std::string html = generateErrorPage("Неверный формат HTTP запроса");
            return formatHttpResponse(400, "Bad Request", "text/html", html);
        }

        std::string headers = request.substr(0, headerEnd);
        std::string body;
        if (request.length() > headerEnd + 4) {
            body = request.substr(headerEnd + 4);
        }

        // Парсим первую строку
        std::istringstream headerStream(headers);
        std::string method, path, version;
        headerStream >> method >> path >> version;

        std::cout << "📥 HTTP запрос: " << method << " " << path << std::endl;

        if (method == "GET")
        {
            if (path == "/" || path == "/search" || path == "/index.html")
            {
                std::string html = generateSearchPage();
                return formatHttpResponse(200, "OK", "text/html", html);
            }
            else
            {
                // Исправлено: используем generateErrorPage
                std::string html = generateErrorPage("404 Not Found");
                return formatHttpResponse(404, "Not Found", "text/html", html);
            }
        }
        else if (method == "POST" && path == "/search")
        {
            // Извлекаем Content-Length
            std::regex contentLengthRegex(R"(Content-Length:\s*(\d+))", std::regex::icase);
            std::smatch match;
            int contentLength = 0;

            if (std::regex_search(headers, match, contentLengthRegex) && match.size() > 1) {
                contentLength = std::stoi(match[1].str());
            }

            // Если тело не полное, пробуем добрать (для простоты считаем, что все данные уже есть)
            if (body.length() < static_cast<size_t>(contentLength)) {
                std::cerr << "Предупреждение: тело запроса неполное" << std::endl;
            }

            // Парсим параметры из тела
            std::string query = parsePostBody(body);

            if (query.empty()) {
                std::string html = generateErrorPage("Пустой поисковый запрос");
                return formatHttpResponse(400, "Bad Request", "text/html", html);
            }

            // Декодируем URL-encoded строку
            query = urlDecode(query);

            // Убираем "query=" если есть
            if (query.find("query=") == 0) {
                query = query.substr(6);
            }

            if (query.empty()) {
                std::string html = generateErrorPage("Пустой поисковый запрос");
                return formatHttpResponse(400, "Bad Request", "text/html", html);
            }

            // Парсим слова
            std::vector<std::string> words = parseQuery(query);

            if (words.empty()) {
                std::string html = generateErrorPage("Нет допустимых слов в запросе");
                return formatHttpResponse(400, "Bad Request", "text/html", html);
            }

            if (words.size() > 4) {
                std::string html = generateErrorPage("Слишком много слов в запросе (максимум 4)");
                return formatHttpResponse(400, "Bad Request", "text/html", html);
            }

            // Выполняем поиск
            std::vector<Database::SearchResult> results;
            try {
                results = database_.searchDocuments(words, 10);
            }
            catch (const std::exception& e) {
                std::cerr << "Ошибка поиска в БД: " << e.what() << std::endl;
                std::string html = generateErrorPage("Ошибка при поиске в базе данных");
                return formatHttpResponse(500, "Internal Server Error", "text/html", html);
            }

            // Генерируем страницу с результатами
            std::string html = generateResultsPage(results, query);
            return formatHttpResponse(200, "OK", "text/html", html);
        }
        else
        {
            // Исправлено: используем generateErrorPage
            std::string html = generateErrorPage("405 Method Not Allowed");
            return formatHttpResponse(405, "Method Not Allowed", "text/html", html);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Ошибка обработки запроса: " << e.what() << std::endl;
        std::string html = generateErrorPage(std::string("Внутренняя ошибка сервера: ") + e.what());
        return formatHttpResponse(500, "Internal Server Error", "text/html", html);
    }
}

// Новый вспомогательный метод для форматирования ответа
std::string SearchServer::formatHttpResponse(int statusCode, const std::string& statusText,
    const std::string& contentType,
    const std::string& content)
{
    std::stringstream response;
    response << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n"
        << "Content-Type: " << contentType << "; charset=utf-8\r\n"
        << "Content-Length: " << content.size() << "\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << content;
    return response.str();
}

// Новый метод для парсинга тела POST запроса
std::string SearchServer::parsePostBody(const std::string& body)
{
    // Ищем параметр query
    size_t queryPos = body.find("query=");
    if (queryPos == std::string::npos) {
        return "";
    }

    size_t start = queryPos + 6; // длина "query="
    size_t end = body.find('&', start);

    if (end == std::string::npos) {
        return body.substr(start);
    }
    else {
        return body.substr(start, end - start);
    }
}

std::string SearchServer::urlDecode(const std::string& encoded)
{
    std::string result;
    result.reserve(encoded.size());

    for (size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.size()) {
            int hexValue;
            std::istringstream iss(encoded.substr(i + 1, 2));
            if (iss >> std::hex >> hexValue) {
                result += static_cast<char>(hexValue);
                i += 2;
            }
            else {
                result += encoded[i];
            }
        }
        else if (encoded[i] == '+') {
            result += ' ';
        }
        else {
            result += encoded[i];
        }
    }

    return result;
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