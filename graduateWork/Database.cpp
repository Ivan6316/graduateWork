#include "Database.h"
#include <stdexcept>

Database::Database(const Config& config) : database_(
	"host=" + config.getDbHost() + " " +
	"port=" + std::to_string(config.getDbPort()) + " " +
	"dbname=" + config.getDbName() + " " +
	"user=" + config.getDbUser() + " " +
	"password=" + config.getDbPassword())
{
	// Проверка подключения
	try
	{
		if (database_.is_open())
		{
			// Уведомляем о успешном подключении к БД
			std::cout << "✔ Подключение к БД установлено" << std::endl;
			std::cout << "Название БД: " << database_.dbname() << std::endl;
		}
		else
		{
			throw std::runtime_error("Не удалось подключиться к БД");
		}
	}
	// Обработка sql ошибок
	catch (const pqxx::sql_error& e)
	{
		throw std::runtime_error("Sql ошибка при подключении к БД:" + std::string(e.what()));
	}
	// Обработка системных ошибок
	catch (const std::exception& e)
	{
		throw std::runtime_error("Системная ошибка при подключении к БД: " + std::string(e.what()));
	}
}

bool Database::isConnected() const
{
	return database_.is_open();
}

void Database::creatingTables()
{
	// Захватываем мьютекс
	std::unique_lock<std::shared_mutex> lock(databaseMutex_);

	// Проверка на открытие
	if (!database_.is_open())
	{
		throw std::runtime_error("БД была закрыта");
	}

	pqxx::work db(database_);
	try
	{
		// Создание таблицы документов
		db.exec(
			"CREATE TABLE IF NOT EXISTS documents("
			"id SERIAL PRIMARY KEY,"
			"url TEXT UNIQUE NOT NULL,"
			"title TEXT,"
			"content TEXT,"
			"created_at TIMESTAMP DEFAULT NOW()"
			");"
		);

		// Создание таблицы слов
		db.exec(
			"CREATE TABLE IF NOT EXISTS words("
			"id SERIAL PRIMARY KEY,"
			"word VARCHAR(32) UNIQUE NOT NULL"
			");"
		);

		// Создание таблицы соединения документов и слов
		db.exec(
			"CREATE TABLE IF NOT EXISTS document_words("
			"document_id INTEGER NOT NULL REFERENCES documents(id) ON DELETE CASCADE,"
			"word_id INTEGER NOT NULL REFERENCES words(id) ON DELETE CASCADE,"
			"frequency INTEGER NOT NULL CHECK (frequency > 0),"
			"PRIMARY KEY (document_id, word_id)"
			");"
		);

		db.commit();
		std::cout << "✔ Таблицы созданы успешно" << std::endl;
	}
	catch (const pqxx::sql_error& e)
	{
		throw std::runtime_error("SQL ошибка при создании таблиц: " + std::string(e.what()));
	}
	catch (const std::exception& e)
	{
		throw std::runtime_error("Системная ошибка при создании таблиц: " + std::string(e.what()));
	}
}

int Database::savingDocument(const std::string& url, const std::string& title, const std::string& content)
{
	// Захватываем мьютекс
	std::unique_lock<std::shared_mutex> lock(databaseMutex_);

	// Проверка на открытие
	if (!database_.is_open())
	{
		throw std::runtime_error("БД была закрыта");
	}

	pqxx::work db(database_);
	try
	{
		// Добавляем или обновляем документ
		pqxx::result result = db.exec(
			"INSERT INTO documents (url, title, content) "
			"VALUES ($1, $2, $3) "
			"ON CONFLICT (url) DO UPDATE "
			"SET title = $2, content = $3 "
			"RETURNING id",
			pqxx::params{ url, title, content });  // ← params

		db.commit();

		int documentId = result[0][0].as<int>();
		std::cout << "✔ Документ сохранён, ID: " << documentId << std::endl;
		return documentId;
	}
	catch (const pqxx::sql_error& e)
	{
		throw std::runtime_error("SQL ошибка при сохранении документов: " + std::string(e.what()));
	}
	catch (const std::exception& e)
	{
		throw std::runtime_error("Системная ошибка при сохранении документов: " + std::string(e.what()));
	}
}

void Database::savingWords(int documentId, const std::vector<std::pair<std::string, int>>& wordsAndFrequency)
{
	// Захватываем мьютекс
	std::unique_lock<std::shared_mutex> lock(databaseMutex_);

	// Проверка на открытие
	if (!database_.is_open())
	{
		throw std::runtime_error("БД была закрыта");
	}

	pqxx::work db(database_);
	try
	{
		// Проходимся по всем парам слов и значений
		for (const auto& [wordText, frequency] : wordsAndFrequency)
		{
			// Ищем слово в таблице words
			pqxx::result wordResult = db.exec(
				"SELECT id FROM words WHERE word = $1",
				pqxx::params{ wordText });

			int wordId;

			// Если слово не найдено - добавляем
			if (wordResult.empty())
			{
				// Добавляем новое слово и получаем его ID
				wordResult = db.exec(
					"INSERT INTO words (word) VALUES ($1) RETURNING id",
					pqxx::params{ wordText });
				wordId = wordResult[0][0].as<int>();
			}
			else
			{
				// Слово уже есть - берём существующий ID
				wordId = wordResult[0][0].as<int>();
			}

			// Добавляем или обновляем связь в document_words
			db.exec(
				"INSERT INTO document_words (document_id, word_id, frequency) "
				"VALUES ($1, $2, $3) "
				"ON CONFLICT (document_id, word_id) "
				"DO UPDATE SET frequency = $3",
				pqxx::params{ documentId, wordId, frequency });
		}

		db.commit();
		std::cout << "✔ Слова сохранены для документа ID: " << documentId << std::endl;
	}
	catch (const pqxx::sql_error& e)
	{
		throw std::runtime_error("SQL ошибка при сохранении слов: " + std::string(e.what()));
	}
	catch (const std::exception& e)
	{
		throw std::runtime_error("Системная ошибка при сохранении слов: " + std::string(e.what()));
	}
}

bool Database::urlExists(const std::string& url)
{
	// Захватываем мьютекс для чтения
	std::shared_lock<std::shared_mutex> lock(databaseMutex_);

	// Проверка на открытие
	if (!database_.is_open())
	{
		throw std::runtime_error("БД была закрыта");
	}

	pqxx::work db(database_);
	try
	{
		pqxx::result result = db.exec(
			"SELECT 1 FROM documents WHERE url = $1 LIMIT 1",
			pqxx::params{ url });

		return !result.empty();
	}
	catch (const pqxx::sql_error& e)
	{
		throw std::runtime_error("SQL ошибка при проверке URL: " + std::string(e.what()));
	}
	catch (const std::exception& e)
	{
		throw std::runtime_error("Системная ошибка при проверке URL: " + std::string(e.what()));
	}
}

int Database::getDocumentIdByUrl(const std::string& url)
{
	// Захватываем мьютекс для чтения
	std::shared_lock<std::shared_mutex> lock(databaseMutex_);

	// Проверка на открытие
	if (!database_.is_open())
	{
		throw std::runtime_error("БД была закрыта");
	}

	pqxx::work db(database_);
	try
	{
		pqxx::result result = db.exec(
			"SELECT id FROM documents WHERE url = $1",
			pqxx::params{ url });

		if (result.empty())
		{
			return -1; // Документ не найден
		}

		return result[0][0].as<int>();
	}
	catch (const pqxx::sql_error& e)
	{
		throw std::runtime_error("SQL ошибка при получении ID документа: " + std::string(e.what()));
	}
}

int Database::getWordId(const std::string& word)
{
	// Захватываем мьютекс для чтения
	std::shared_lock<std::shared_mutex> lock(databaseMutex_);

	// Проверка на открытие
	if (!database_.is_open())
	{
		throw std::runtime_error("БД была закрыта");
	}

	pqxx::work db(database_);
	try
	{
		pqxx::result result = db.exec(
			"SELECT id FROM words WHERE word = $1",
			pqxx::params{ word });

		if (result.empty())
		{
			return -1; // Слово не найдено
		}

		return result[0][0].as<int>();
	}
	catch (const pqxx::sql_error& e)
	{
		throw std::runtime_error("SQL ошибка при получении ID слова: " + std::string(e.what()));
	}
}

std::vector<Database::SearchResult> Database::searchDocuments(const std::vector<std::string>& words, int limit)
{
	// Захватываем мьютекс для чтения
	std::shared_lock<std::shared_mutex> lock(databaseMutex_);

	// Проверка на открытие
	if (!database_.is_open())
	{
		throw std::runtime_error("БД была закрыта");
	}

	std::vector<SearchResult> results;

	if (words.empty())
	{
		return results;
	}

	pqxx::work db(database_);
	try
	{
		// Создаём временную таблицу с искомыми словами
		std::string tempTableSql = "WITH search_words AS (";
		for (size_t i = 0; i < words.size(); ++i)
		{
			if (i > 0) tempTableSql += " UNION ALL ";
			tempTableSql += "SELECT '" + db.esc(words[i]) + "'::text AS word";
		}
		tempTableSql += ") ";

		// Основной запрос поиска
		std::string query = tempTableSql +
			"SELECT d.url, d.title, SUM(dw.frequency) as relevance "
			"FROM documents d "
			"JOIN document_words dw ON d.id = dw.document_id "
			"JOIN words w ON dw.word_id = w.id "
			"JOIN search_words sw ON w.word = sw.word "
			"GROUP BY d.id, d.url, d.title "
			"HAVING COUNT(DISTINCT w.word) = " + std::to_string(words.size()) + " "
			"ORDER BY relevance DESC "
			"LIMIT " + std::to_string(limit);

		pqxx::result dbResult = db.exec(query);

		for (const auto& row : dbResult)
		{
			SearchResult result;
			result.url = row["url"].as<std::string>();
			result.title = row["title"].as<std::string>();
			result.relevance = row["relevance"].as<int>();
			results.push_back(result);
		}

		return results;
	}
	catch (const pqxx::sql_error& e)
	{
		throw std::runtime_error("SQL ошибка при поиске документов: " + std::string(e.what()));
	}
	catch (const std::exception& e)
	{
		throw std::runtime_error("Системная ошибка при поиске документов: " + std::string(e.what()));
	}
}

std::vector<std::tuple<int, std::string, std::string>> Database::getAllDocuments()
{
	// Захватываем мьютекс для чтения
	std::shared_lock<std::shared_mutex> lock(databaseMutex_);

	// Проверка на открытие
	if (!database_.is_open())
	{
		throw std::runtime_error("БД была закрыта");
	}

	std::vector<std::tuple<int, std::string, std::string>> documents;

	pqxx::work db(database_);
	try
	{
		pqxx::result result = db.exec(
			"SELECT id, url, title FROM documents ORDER BY id");

		for (const auto& row : result)
		{
			documents.emplace_back(
				row["id"].as<int>(),
				row["url"].as<std::string>(),
				row["title"].as<std::string>()
			);
		}

		return documents;
	}
	catch (const pqxx::sql_error& e)
	{
		throw std::runtime_error("SQL ошибка при получении документов: " + std::string(e.what()));
	}
}

std::vector<std::pair<std::string, int>> Database::getWordsByDocumentId(int documentId)
{
	// Захватываем мьютекс для чтения
	std::shared_lock<std::shared_mutex> lock(databaseMutex_);

	// Проверка на открытие
	if (!database_.is_open())
	{
		throw std::runtime_error("БД была закрыта");
	}

	std::vector<std::pair<std::string, int>> words;

	pqxx::work db(database_);
	try
	{
		pqxx::result result = db.exec(
			"SELECT w.word, dw.frequency "
			"FROM words w "
			"JOIN document_words dw ON w.id = dw.word_id "
			"WHERE dw.document_id = $1 "
			"ORDER BY dw.frequency DESC",
			pqxx::params{ documentId });

		for (const auto& row : result)
		{
			words.emplace_back(
				row["word"].as<std::string>(),
				row["frequency"].as<int>()
			);
		}

		return words;
	}
	catch (const pqxx::sql_error& e)
	{
		throw std::runtime_error("SQL ошибка при получении слов документа: " + std::string(e.what()));
	}
}

void Database::deleteDocument(int documentId)
{
	// Захватываем мьютекс
	std::unique_lock<std::shared_mutex> lock(databaseMutex_);

	// Проверка на открытие
	if (!database_.is_open())
	{
		throw std::runtime_error("БД была закрыта");
	}

	pqxx::work db(database_);
	try
	{
		db.exec(
			"DELETE FROM documents WHERE id = $1",
			pqxx::params{ documentId });

		db.commit();
		std::cout << "✔ Документ ID: " << documentId << " удалён" << std::endl;
	}
	catch (const pqxx::sql_error& e)
	{
		throw std::runtime_error("SQL ошибка при удалении документа: " + std::string(e.what()));
	}
}

Database::DatabaseStats Database::getStatistics() const
{
	// Захватываем мьютекс для чтения
	std::shared_lock<std::shared_mutex> lock(databaseMutex_);

	// Проверка на открытие
	if (!database_.is_open())
	{
		throw std::runtime_error("БД была закрыта");
	}

	DatabaseStats stats = { 0, 0, 0 };

	try
	{
		pqxx::work db(const_cast<pqxx::connection&>(database_));

		// Количество документов
		pqxx::result docResult = db.exec("SELECT COUNT(*) FROM documents");
		if (!docResult.empty())
		{
			stats.documentsCount = docResult[0][0].as<int>();
		}

		// Количество слов
		pqxx::result wordsResult = db.exec("SELECT COUNT(*) FROM words");
		if (!wordsResult.empty())
		{
			stats.wordsCount = wordsResult[0][0].as<int>();
		}

		// Количество связей
		pqxx::result relResult = db.exec("SELECT COUNT(*) FROM document_words");
		if (!relResult.empty())
		{
			stats.totalRelations = relResult[0][0].as<int>();
		}

		return stats;
	}
	catch (const pqxx::sql_error& e)
	{
		throw std::runtime_error("SQL ошибка при получении статистики: " + std::string(e.what()));
	}
	catch (const std::exception& e)
	{
		throw std::runtime_error("Системная ошибка при получении статистики: " + std::string(e.what()));
	}
}