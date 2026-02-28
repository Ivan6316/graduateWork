#ifndef CONFIG_H
#define CONFIG_H

#include <iostream>

class Config
{
private:
	// Параметры БД
	std::string dbHost_{};
	int dbPort_{};
	std::string dbName_{};
	std::string dbUser_{};
	std::string dbPassword_{};

	// Параметры паука
	std::string spiderStartUrl_{};
	int spiderMaxDepth_{};
	bool runSpider_;

	// Параметры поисковика
	int searcherPort_{};

public:
	// Конструктор с указанием пути к файлу
	Config(const std::string& filePath);

	// Получение параметров БД
	const std::string& getDbHost() const;
	int getDbPort() const;
	const std::string& getDbName() const;
	const std::string& getDbUser() const;
	const std::string& getDbPassword() const;

	// Получение параметров паука
	const std::string& getSpiderStartUrl() const;
	int getSpiderMaxDepth() const;
	bool shouldRunSpider() const;

	// Получение параметров поисковика
	int getSearcherPort() const;
};


#endif // !CONFIG_H