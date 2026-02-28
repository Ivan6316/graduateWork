#ifndef SPIDER_H
#define SPIDER_H

#include <iostream>
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <unordered_set>
#include <vector>
#include <functional>
#include "Config.h"
#include "Database.h"
#include "HTMLDownloader.h"
#include "Indexer.h"

class Spider
{
private:
    Config& config_;
    Database& database_;
    HTMLDownloader downloader_;
    Indexer indexer_;

    // Структура для задачи скачивания
    struct DownloadTask
    {
        std::string url;
        int depth;
    };

    // Очередь задач
    std::queue<DownloadTask> taskQueue_;
    mutable std::mutex queueMutex_;  // <-- делаем mutable
    std::condition_variable queueCV_;

    // Множество обработанных URL (для избежания дублирования)
    std::unordered_set<std::string> processedUrls_;
    mutable std::mutex processedMutex_;  // <-- делаем mutable

    // Пул потоков
    std::vector<std::thread> workers_;
    std::atomic<bool> stopRequested_;
    std::atomic<int> activeWorkers_;

    // Статистика
    std::atomic<int> pagesDownloaded_;
    std::atomic<int> pagesIndexed_;

    // Функция рабочего потока
    void workerFunction();

    // Обработка одной страницы
    void processPage(const std::string& url, int depth);

    // Добавление задачи в очередь
    void addTask(const std::string& url, int depth);

public:
    Spider(Config& config, Database& db);
    ~Spider();

    // Запуск паука
    void start();

    // Остановка паука
    void stop();

    // Получение статистики
    struct SpiderStats
    {
        int totalDownloaded;
        int totalIndexed;
        int queueSize;
        int activeWorkers;
    };

    SpiderStats getStats() const;

    // Проверка, работает ли паук
    bool isRunning() const;

    bool isFinished() const;
};

#endif // SPIDER_H