#include "Spider.h"
#include <chrono>

Spider::Spider(Config& config, Database& db)
    : config_(config)
    , database_(db)
    , stopRequested_(false)
    , activeWorkers_(0)
    , pagesDownloaded_(0)
    , pagesIndexed_(0)
{
    // Начинаем со стартовой страницы
    addTask(config_.getSpiderStartUrl(), 0);
}

Spider::~Spider()
{
    stop();
}

void Spider::addTask(const std::string& url, int depth)
{
    std::unique_lock<std::mutex> lock(queueMutex_);

    // Проверяем, не превышена ли глубина
    if (depth > config_.getSpiderMaxDepth())
    {
        return;
    }

    // Проверяем, не обрабатывали ли уже этот URL
    {
        std::lock_guard<std::mutex> processedLock(processedMutex_);
        if (processedUrls_.find(url) != processedUrls_.end())
        {
            return;
        }
    }

    // Добавляем задачу в очередь
    taskQueue_.push({ url, depth });
    queueCV_.notify_one();
}

void Spider::workerFunction()
{
    activeWorkers_++;

    while (!stopRequested_)
    {
        DownloadTask task;

        {
            std::unique_lock<std::mutex> lock(queueMutex_);

            // Ждём задачу или команду остановки
            queueCV_.wait(lock, [this]() {
                return !taskQueue_.empty() || stopRequested_;
                });

            if (stopRequested_ && taskQueue_.empty())
            {
                break;
            }

            if (taskQueue_.empty())
            {
                continue;
            }

            // Берём задачу из очереди
            task = taskQueue_.front();
            taskQueue_.pop();
        }

        // Обрабатываем страницу
        try
        {
            processPage(task.url, task.depth);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Ошибка в рабочем потоке при обработке "
                << task.url << ": " << e.what() << std::endl;
        }
    }

    activeWorkers_--;
}

void Spider::processPage(const std::string& url, int depth)
{
    try
    {
        std::cout << "Попытка обработки страницы [" << depth << "]: " << url << std::endl;

        // Помечаем URL как обрабатываемый
        {
            std::lock_guard<std::mutex> lock(processedMutex_);
            if (processedUrls_.find(url) != processedUrls_.end())
            {
                std::cout << "URL уже обработан: " << url << std::endl;
                return;
            }
            processedUrls_.insert(url);
        }

        // Проверяем, существует ли уже документ в БД
        if (database_.urlExists(url))
        {
            std::cout << "Документ уже существует в БД: " << url << std::endl;
            return;
        }

        // Скачиваем страницу
        std::string html;
        try
        {
            std::cout << "Скачивание: " << url << std::endl;
            html = downloader_.download(url);
            pagesDownloaded_++;
            std::cout << "Успешно загружено: " << html.size() << " байт" << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Ошибка загрузки " << url << ": " << e.what() << std::endl;
            return;
        }

        // Индексируем страницу
        Indexer::IndexingResult result;
        try
        {
            std::cout << "Индексация: " << url << std::endl;
            result = indexer_.indexPage(html, url);
            pagesIndexed_++;
            std::cout << "Индексация завершена, слов: " << result.wordsFrequency.size() << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Ошибка индексации " << url << ": " << e.what() << std::endl;
            return;
        }

        // Сохраняем в БД
        try
        {
            std::cout << "Сохранение в БД: " << url << std::endl;
            int documentId = database_.savingDocument(url, result.title, result.cleanContent);
            if (documentId > 0 && !result.wordsFrequency.empty())
            {
                database_.savingWords(documentId, result.wordsFrequency);
                std::cout << "Сохранено в БД (ID: " << documentId << ")" << std::endl;
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Ошибка сохранения в БД " << url << ": " << e.what() << std::endl;
            return;
        }

        // Если не достигли максимальной глубины, извлекаем ссылки
        if (depth < config_.getSpiderMaxDepth())
        {
            try
            {
                std::cout << "Извлечение ссылок из: " << url << std::endl;
                std::vector<std::string> links = downloader_.extractLinks(html, url);
                std::cout << "Найдено ссылок: " << links.size() << std::endl;

                for (const auto& link : links)
                {
                    std::cout << "Добавляем в очередь: " << link << std::endl;
                    addTask(link, depth + 1);
                }
            }
            catch (const std::exception& e)
            {
                std::cerr << "Ошибка извлечения ссылок " << url << ": " << e.what() << std::endl;
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Критическая ошибка при обработке страницы " << url << ": " << e.what() << std::endl;
    }
}

void Spider::start()
{
    stopRequested_ = false;

    // Создаём пул потоков
    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 2; // fallback

    std::cout << "\n🚀 Запуск паука" << std::endl;
    std::cout << "   Потоков: " << numThreads << std::endl;
    std::cout << "   Глубина: " << config_.getSpiderMaxDepth() << std::endl;
    std::cout << "   Стартовая страница: " << config_.getSpiderStartUrl() << std::endl;

    for (unsigned int i = 0; i < numThreads; ++i)
    {
        workers_.emplace_back(&Spider::workerFunction, this);
    }
}

void Spider::stop()
{
    stopRequested_ = true;
    queueCV_.notify_all();

    for (auto& worker : workers_)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }

    workers_.clear();

    std::cout << "\n🛑 Паук остановлен" << std::endl;
    std::cout << "   Всего загружено: " << pagesDownloaded_ << " страниц" << std::endl;
    std::cout << "   Всего проиндексировано: " << pagesIndexed_ << " страниц" << std::endl;
}

Spider::SpiderStats Spider::getStats() const
{
    SpiderStats stats;
    stats.totalDownloaded = pagesDownloaded_;
    stats.totalIndexed = pagesIndexed_;
    stats.activeWorkers = activeWorkers_;

    std::lock_guard<std::mutex> lock(queueMutex_);
    stats.queueSize = static_cast<int>(taskQueue_.size());

    return stats;
}

bool Spider::isRunning() const
{
    return activeWorkers_ > 0;
}