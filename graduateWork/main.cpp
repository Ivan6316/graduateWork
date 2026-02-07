#include <iostream>
#include <memory>
#include <csignal>
#include <thread>
#include <chrono>
#include "Config.h"
#include "Database.h"
#include "Spider.h"
#include "SearchServer.h"

// Глобальные указатели для обработки сигналов
std::unique_ptr<Spider> g_spider;
std::unique_ptr<SearchServer> g_searchServer;

// Функция для обработки сигналов завершения
void signalHandler(int signal)
{
    std::cout << "\n\n📢 Получен сигнал " << signal << ", завершаем работу..." << std::endl;

    if (g_spider)
    {
        std::cout << "🛑 Останавливаем паука..." << std::endl;
        g_spider->stop();
    }

    if (g_searchServer)
    {
        std::cout << "🛑 Останавливаем поисковый сервер..." << std::endl;
        g_searchServer->stop();
    }

    std::cout << "👋 Завершение работы поисковой системы" << std::endl;
    exit(0);
}

// Функция для вывода статистики в реальном времени
void printStats(const Spider& spider, const Database& db)
{
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        auto stats = spider.getStats();
        auto dbStats = db.getStatistics();

        std::cout << "\n📊 Статистика в реальном времени:" << std::endl;
        std::cout << "   Паук: " << stats.activeWorkers << " активных потоков" << std::endl;
        std::cout << "   Очередь задач: " << stats.queueSize << std::endl;
        std::cout << "   Загружено страниц: " << stats.totalDownloaded << std::endl;
        std::cout << "   Проиндексировано: " << stats.totalIndexed << std::endl;
        std::cout << "   БД документов: " << dbStats.documentsCount << std::endl;
        std::cout << "   БД уникальных слов: " << dbStats.wordsCount << std::endl;

        if (stats.queueSize == 0 && stats.activeWorkers == 0 && g_spider && g_spider->isRunning())
        {
            std::cout << "\n✅ Паук завершил обход всех страниц!" << std::endl;
            break;
        }
    }
}

int main(int argc, char* argv[])
{
    // Подключение Русского языка
    setlocale(LC_ALL, "rus");

    std::cout << "========================================" << std::endl;
    std::cout << "🔍 Поисковая система v1.0" << std::endl;
    std::cout << "========================================" << std::endl;

    try
    {
        // Устанавливаем обработчики сигналов
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        // Определяем путь к конфигурационному файлу
        std::string configFile = "config.ini";
        if (argc > 1)
        {
            configFile = argv[1];
        }

        std::cout << "\n📄 Загрузка конфигурации из: " << configFile << std::endl;

        // Загружаем конфигурацию
        Config config(configFile);

        // Подключаемся к базе данных
        std::cout << "💾 Подключение к базе данных..." << std::endl;
        Database db(config);

        // Создаём таблицы если их нет
        std::cout << "🗃️  Создание таблиц БД..." << std::endl;
        db.creatingTables();

        // Получаем начальную статистику
        auto initialStats = db.getStatistics();
        std::cout << "\n📈 Начальная статистика базы данных:" << std::endl;
        std::cout << "   Документов: " << initialStats.documentsCount << std::endl;
        std::cout << "   Уникальных слов: " << initialStats.wordsCount << std::endl;
        std::cout << "   Связей: " << initialStats.totalRelations << std::endl;

        // Запускаем паука
        std::cout << "\n🕷️  Инициализация паука..." << std::endl;
        std::cout << "   Стартовая страница: " << config.getSpiderStartUrl() << std::endl;
        std::cout << "   Глубина обхода: " << config.getSpiderMaxDepth() << std::endl;

        g_spider = std::make_unique<Spider>(config, db);

        // Запускаем паука в отдельном потоке
        std::thread spiderThread([&]() {
            g_spider->start();
            });

        // Запускаем поток для вывода статистики
        std::thread statsThread([&]() {
            printStats(*g_spider, db);
            });

        // Даем пауку немного поработать перед запуском сервера
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Запускаем поисковый сервер
        std::cout << "\n🌐 Инициализация поискового сервера..." << std::endl;
        std::cout << "   Порт: " << config.getSearcherPort() << std::endl;

        g_searchServer = std::make_unique<SearchServer>(config, db);
        g_searchServer->start();

        std::cout << "\n========================================" << std::endl;
        std::cout << "✅ Система запущена и работает!" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "\n📋 Инструкция:" << std::endl;
        std::cout << "   1. Откройте браузер и перейдите по адресу:" << std::endl;
        std::cout << "      http://localhost:" << config.getSearcherPort() << std::endl;
        std::cout << "   2. Введите поисковый запрос в форму" << std::endl;
        std::cout << "   3. Нажмите Ctrl+C для завершения работы" << std::endl;
        std::cout << "\n🔄 Паук работает в фоновом режиме..." << std::endl;

        // Ждём завершения работы паука
        spiderThread.join();
        statsThread.join();

        // Финальная статистика
        auto finalStats = db.getStatistics();
        std::cout << "\n========================================" << std::endl;
        std::cout << "🎯 Финальная статистика:" << std::endl;
        std::cout << "   Всего документов в БД: " << finalStats.documentsCount << std::endl;
        std::cout << "   Всего уникальных слов: " << finalStats.wordsCount << std::endl;
        std::cout << "========================================" << std::endl;

        std::cout << "\n👋 Завершение работы поисковой системы" << std::endl;

    }
    catch (const std::exception& e)
    {
        std::cerr << "\n❌ Критическая ошибка: " << e.what() << std::endl;

        // Останавливаем все компоненты
        if (g_spider)
        {
            g_spider->stop();
        }

        if (g_searchServer)
        {
            g_searchServer->stop();
        }

        return 1;
    }

    return 0;
}