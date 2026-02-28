#include <iostream>
#include <memory>
#include <csignal>
#include <thread>
#include <chrono>
#include <atomic>
#include "Config.h"
#include "Database.h"
#include "Spider.h"
#include "SearchServer.h"
#include <Windows.h>

// Глобальные указатели для обработки сигналов
std::unique_ptr<Spider> g_spider;
std::unique_ptr<SearchServer> g_searchServer;
std::atomic<bool> g_running{ true };

// Функция для обработки сигналов завершения
void signalHandler(int signal)
{
    std::cout << "\n\n📢 Получен сигнал " << signal << ", завершаем работу..." << std::endl;
    g_running = false;

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
}

// Функция для мониторинга паука
void spiderMonitor(Spider* spider)
{
    int idleCounter = 0;

    while (g_running && spider && spider->isRunning())
    {
        std::this_thread::sleep_for(std::chrono::seconds(2));

        auto stats = spider->getStats();

        std::cout << "\n📊 Статистика паука:" << std::endl;
        std::cout << "   Активных потоков: " << stats.activeWorkers << std::endl;
        std::cout << "   В очереди: " << stats.queueSize << std::endl;
        std::cout << "   Загружено: " << stats.totalDownloaded << std::endl;
        std::cout << "   Проиндексировано: " << stats.totalIndexed << std::endl;

        // Если очередь пуста и нет активных потоков - паук завершил работу
        if (stats.queueSize == 0 && stats.activeWorkers == 0)
        {
            idleCounter++;
            if (idleCounter > 3) // Даем время на завершение всех потоков
            {
                std::cout << "\n✅ Паук завершил обход всех страниц!" << std::endl;
                break;
            }
        }
        else
        {
            idleCounter = 0;
        }
    }

    if (spider && spider->isRunning())
    {
        spider->stop();
    }
}

int main(int argc, char* argv[])
{
    // Подключение Русского языка
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    std::cout << "========================================" << std::endl;
    std::cout << "🔍 Поисковая система v1.1" << std::endl;
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
        std::cout << "\n📈 Текущая статистика базы данных:" << std::endl;
        std::cout << "   Документов: " << initialStats.documentsCount << std::endl;
        std::cout << "   Уникальных слов: " << initialStats.wordsCount << std::endl;

        // ЗАПУСКАЕМ ПОИСКОВЫЙ СЕРВЕР
        std::cout << "\n🌐 Инициализация поискового сервера..." << std::endl;
        std::cout << "   Порт: " << config.getSearcherPort() << std::endl;

        g_searchServer = std::make_unique<SearchServer>(config, db);

        // Запускаем сервер в отдельном потоке
        std::thread serverThread([&]() {
            g_searchServer->start();
            });

        // Даем серверу время запуститься
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::thread spiderMonitorThread;
        std::thread spiderThread;

        if (config.shouldRunSpider())
        {
            std::cout << "\n🕷️  Инициализация паука..." << std::endl;
            std::cout << "   Стартовая страница: " << config.getSpiderStartUrl() << std::endl;
            std::cout << "   Глубина обхода: " << config.getSpiderMaxDepth() << std::endl;

            g_spider = std::make_unique<Spider>(config, db);

            // Запускаем паука в отдельном потоке
            spiderThread = std::thread([&]() {
                g_spider->start();
                });

            // Запускаем монитор паука
            spiderMonitorThread = std::thread(spiderMonitor, g_spider.get());
        }

        std::cout << "\n========================================" << std::endl;
        std::cout << "✅ Система запущена и работает!" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "\n📋 Инструкция:" << std::endl;
        std::cout << "   1. Откройте браузер и перейдите по адресу:" << std::endl;
        std::cout << "      http://localhost:" << config.getSearcherPort() << std::endl;
        std::cout << "   2. Введите поисковый запрос в форму" << std::endl;
        std::cout << "   3. Нажмите Ctrl+C для завершения работы" << std::endl;

        if (config.shouldRunSpider())
        {
            std::cout << "\n🔄 Паук работает в фоновом режиме..." << std::endl;
        }
        else
        {
            std::cout << "\n⚠️  Паук не запущен (запустите отдельно для индексации)" << std::endl;
        }

        // Ждем сигнала завершения
        while (g_running)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // Проверяем, не завершился ли паук самостоятельно
            if (config.shouldRunSpider() && g_spider && !g_spider->isRunning())
            {
                std::cout << "\n✅ Паук завершил работу автоматически" << std::endl;

                // Если паук завершился, но сервер продолжает работу
                if (g_searchServer && g_searchServer->isRunning())
                {
                    std::cout << "🔄 Сервер продолжает работу. Нажмите Ctrl+C для выхода." << std::endl;
                }
            }
        }

        std::cout << "\n👋 Завершение работы поисковой системы" << std::endl;

        // Ждем завершения всех потоков
        if (spiderThread.joinable())
        {
            spiderThread.join();
        }

        if (spiderMonitorThread.joinable())
        {
            spiderMonitorThread.join();
        }

        if (serverThread.joinable())
        {
            // Сервер может работать в бесконечном цикле, поэтому присоединяемся после остановки
            g_searchServer->stop();
            serverThread.join();
        }

        // Финальная статистика
        auto finalStats = db.getStatistics();
        std::cout << "\n========================================" << std::endl;
        std::cout << "🎯 Финальная статистика:" << std::endl;
        std::cout << "   Всего документов в БД: " << finalStats.documentsCount << std::endl;
        std::cout << "   Всего уникальных слов: " << finalStats.wordsCount << std::endl;
        std::cout << "========================================" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "\n❌ Критическая ошибка: " << e.what() << std::endl;

        // Останавливаем все компоненты
        g_running = false;

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