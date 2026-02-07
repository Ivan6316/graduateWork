#ifndef HTMLDOWNLOADER_H
#define HTMLDOWNLOADER_H

#include <string>
#include <vector>

class HTMLDownloader
{
public:
    HTMLDownloader();
    ~HTMLDownloader();

    // Скачивание HTML-страницы
    std::string download(const std::string& url);

    // Извлечение ссылок из HTML
    std::vector<std::string> extractLinks(const std::string& html, const std::string& baseUrl);
};

#endif // HTMLDOWNLOADER_H