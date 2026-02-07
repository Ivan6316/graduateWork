#include "HTMLDownloader.h"
#include <regex>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <curl/curl.h>

// Callback для записи данных
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp)
{
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

HTMLDownloader::HTMLDownloader()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

HTMLDownloader::~HTMLDownloader()
{
    curl_global_cleanup();
}

std::string HTMLDownloader::download(const std::string& url)
{
    CURL* curl = curl_easy_init();
    if (!curl)
    {
        throw std::runtime_error("Не удалось инициализировать CURL");
    }

    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "SearchEngineBot/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    // Для HTTPS
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK)
    {
        std::string error = curl_easy_strerror(res);
        curl_easy_cleanup(curl);
        throw std::runtime_error("Ошибка CURL: " + error + " для URL: " + url);
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_easy_cleanup(curl);

    if (http_code != 200)
    {
        throw std::runtime_error("HTTP ошибка " + std::to_string(http_code) + " для URL: " + url);
    }

    std::cout << "✔ Страница загружена: " << url << " (" << response.size() << " байт)" << std::endl;
    return response;
}

std::vector<std::string> HTMLDownloader::extractLinks(const std::string& html, const std::string& baseUrl)
{
    std::vector<std::string> links;

    try
    {
        // Регулярное выражение для поиска ссылок
        std::regex linkRegex(R"(<a\s[^>]*href\s*=\s*["']([^"']+)["'])", std::regex::icase);

        auto begin = std::sregex_iterator(html.begin(), html.end(), linkRegex);
        auto end = std::sregex_iterator();

        // Извлекаем базовый URL
        std::string baseDomain;
        size_t protoPos = baseUrl.find("://");
        if (protoPos != std::string::npos)
        {
            size_t slashPos = baseUrl.find('/', protoPos + 3);
            if (slashPos != std::string::npos)
            {
                baseDomain = baseUrl.substr(0, slashPos);
            }
            else
            {
                baseDomain = baseUrl;
            }
        }
        else
        {
            baseDomain = "http://" + baseUrl;
        }

        for (std::sregex_iterator i = begin; i != end; ++i)
        {
            std::smatch match = *i;
            if (match.size() > 1)
            {
                std::string link = match[1].str();

                // Пропускаем пустые и javascript ссылки
                if (link.empty() ||
                    link[0] == '#' ||
                    link.find("javascript:") == 0 ||
                    link.find("mailto:") == 0 ||
                    link.find("tel:") == 0)
                {
                    continue;
                }

                // Преобразуем относительные ссылки в абсолютные
                if (link.find("://") == std::string::npos)
                {
                    if (link[0] == '/')
                    {
                        // Абсолютный путь от корня
                        if (baseDomain.back() == '/')
                        {
                            link = baseDomain.substr(0, baseDomain.length() - 1) + link;
                        }
                        else
                        {
                            link = baseDomain + link;
                        }
                    }
                    else
                    {
                        // Относительный путь
                        std::string basePath = baseUrl;
                        size_t lastSlash = basePath.find_last_of('/');
                        if (lastSlash != std::string::npos)
                        {
                            basePath = basePath.substr(0, lastSlash + 1);
                            link = basePath + link;
                        }
                        else
                        {
                            link = baseDomain + "/" + link;
                        }
                    }
                }

                // Убираем якоря
                size_t anchorPos = link.find('#');
                if (anchorPos != std::string::npos)
                {
                    link = link.substr(0, anchorPos);
                }

                // Добавляем протокол если нужно
                if (link.find("://") == std::string::npos && link[0] != '/')
                {
                    link = "http://" + link;
                }

                // Убираем дубликаты
                if (std::find(links.begin(), links.end(), link) == links.end())
                {
                    links.push_back(link);
                }
            }
        }

        std::cout << "  Найдено ссылок: " << links.size() << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Ошибка при извлечении ссылок: " << e.what() << std::endl;
    }

    return links;
}