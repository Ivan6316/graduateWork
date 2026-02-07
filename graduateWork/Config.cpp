#include "Config.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

Config::Config(const std::string& filePath)
{
	try
	{
		// Создаём объект для чтения конфигурации
		boost::property_tree::ptree config;
		boost::property_tree::read_ini(filePath, config);

		// Читаем настройки БД
		dbHost_ = config.get<std::string>("database.host");
		dbName_ = config.get<std::string>("database.name");
		dbPassword_ = config.get<std::string>("database.password");
		dbPort_ = config.get<int>("database.port");
		dbUser_ = config.get<std::string>("database.user");
		
		// Читаем настройки паука
		spiderMaxDepth_ = config.get<int>("spider.maxDepth");
		spiderStartUrl_ = config.get<std::string>("spider.startUrl");

		// Читаем настройки поисковика
		searcherPort_ = config.get<int>("searcher.port");
	}
	catch (const std::exception& e)
	{
		// Если файл не найден или ошибка парсинга
		// Выкидываем ошибку чтения ini-файла
		throw std::runtime_error("Error reading the configuration file: " + std::string(e.what()));
	}
}

const std::string& Config::getDbHost() const { return dbHost_; }

int Config::getDbPort() const { return dbPort_; }

const std::string& Config::getDbName() const { return dbName_; }

const std::string& Config::getDbUser() const { return dbUser_; }

const std::string& Config::getDbPassword() const { return dbPassword_; }

const std::string& Config::getSpiderStartUrl() const { return spiderStartUrl_; }

int Config::getSpiderMaxDepth() const { return spiderMaxDepth_; }

int Config::getSearcherPort() const { return searcherPort_; }