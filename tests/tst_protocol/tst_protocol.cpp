// Юнит-тесты для текстового протокола GraphingProtocol.h.
// Покрывает тест-кейсы: TC-12 (Parsing error), TC-23 (SQL-инъекция как параметр),
// TC-35 (неизвестный тип), TC-37 (экранирование спецсимволов).

#include <QtTest>
#include <string>
#include <stdexcept>
#include "GraphingProtocol.h"

namespace {

bool parseThrows(const std::string& raw)
{
    try {
        GraphingProtocol::parse(raw);
        return false;
    } catch (const std::runtime_error&) {
        return true;
    } catch (...) {
        return false;
    }
}

}

class TestGraphingProtocol : public QObject
{
    Q_OBJECT

private slots:
    // TC-12: Сервер не падает на мусоре — парсер бросает std::runtime_error,
    // сервер ловит и отвечает "Parsing error: ...".
    void rejectsGarbageInput_TC12()
    {
        QVERIFY(parseThrows(""));
        QVERIFY(parseThrows("hello"));
        QVERIFY(parseThrows("hello world"));
        QVERIFY(parseThrows("unknown 1 type"));
        QVERIFY(parseThrows("request"));
        QVERIFY(parseThrows("request foo bar"));        // нечисловой requestId
        QVERIFY(parseThrows("request 1"));               // нет type
        QVERIFY(parseThrows("response 1 maybe"));        // response не ok/error
        QVERIFY(parseThrows("response 1 error"));        // error без payload
        QVERIFY(parseThrows("response 1 error abc"));    // нечисловой код ошибки
        QVERIFY(parseThrows(std::string("\xff\xff\xff")));
    }

    // TC-12: tryParseCorrelationId должен достать correlationId даже из частично битого сообщения,
    // чтобы сервер мог ответить корректным error response.
    void recoversCorrelationIdFromBadMessage_TC12()
    {
        std::uint64_t id = 999;
        // Корректное сообщение, для убедительности.
        QVERIFY(GraphingProtocol::tryParseCorrelationId("request 42 calculate 1|2|3", id));
        QCOMPARE(id, std::uint64_t(42));

        // Битое (лишние поля) — но первые два токена корректны.
        id = 0;
        QVERIFY(GraphingProtocol::tryParseCorrelationId("request 17 bad extra junk", id));
        QCOMPARE(id, std::uint64_t(17));

        // Совсем мусор — должен вернуть false без падений.
        QVERIFY(!GraphingProtocol::tryParseCorrelationId("totally garbage", id));
        QVERIFY(!GraphingProtocol::tryParseCorrelationId("", id));
    }

    // TC-23: Строка с SQL-инъекцией передается в качестве параметра как обычный текст.
    // Протокол не должен трактовать ' OR 1=1 -- как структуру.
    void carriesSqlInjectionAsPlainParameter_TC23()
    {
        const std::string injection = "' OR 1=1 --";
        auto request = GraphingProtocol::Message::request(1, "login", {injection, "password"});
        std::string serialized = GraphingProtocol::serialize(request);

        // В сериализованной строке текст инъекции присутствует целиком (после экранирования пробелов).
        QVERIFY(serialized.find("OR") != std::string::npos);
        QVERIFY(serialized.find("1=1") != std::string::npos);

        // Roundtrip: после parse параметры возвращаются в исходном виде.
        auto parsed = GraphingProtocol::parse(serialized);
        QCOMPARE(parsed.parameters.size(), std::size_t(2));
        QCOMPARE(parsed.parameters[0], injection);
        QCOMPARE(parsed.parameters[1], std::string("password"));
        QCOMPARE(parsed.type, std::string("login"));
    }

    // TC-35: Неизвестный тип запроса корректно парсится протоколом.
    // Бизнес-логика сервера дальше отвечает 'Type "hack" is not implemented'.
    void parsesUnknownRequestType_TC35()
    {
        auto parsed = GraphingProtocol::parse("request 7 hack");
        QCOMPARE(parsed.kind, GraphingProtocol::MessageKind::Request);
        QCOMPARE(parsed.correlationId, std::uint64_t(7));
        QCOMPARE(parsed.type, std::string("hack"));
        QVERIFY(parsed.parameters.empty());

        auto parsedWithParams = GraphingProtocol::parse("request 8 totallyUnknownType arg1|arg2");
        QCOMPARE(parsedWithParams.type, std::string("totallyUnknownType"));
        QCOMPARE(parsedWithParams.parameters.size(), std::size_t(2));
        QCOMPARE(parsedWithParams.parameters[0], std::string("arg1"));
        QCOMPARE(parsedWithParams.parameters[1], std::string("arg2"));
    }

    // TC-37: Экранирование спецсимволов в SQL — структура таблиц не нарушена,
    // данные сохраняются как литералы. Проверяем сохранение целостности через сериализацию/парсинг.
    void escapesSpecialCharactersSafely_TC37()
    {
        const std::vector<std::string> payloads = {
            std::string("test'; DROP TABLE users; --"),
            std::string("\""),
            std::string("\\|"),
            std::string("\\n"),
            std::string("line1\nline2"),
            std::string("space test"),
            std::string("pipe|inside|value"),
            std::string("backslash\\and\\more"),
        };

        for (std::size_t i = 0; i < payloads.size(); ++i) {
            auto request = GraphingProtocol::Message::request(
                static_cast<std::uint64_t>(i + 1),
                "register",
                {payloads[i], "p"}
            );
            std::string serialized = GraphingProtocol::serialize(request);

            // Внутри сериализованной строки не должно быть «сырого» \n,
            // иначе TCP-разделитель сообщений разорвет данные.
            QVERIFY2(
                serialized.find('\n') == std::string::npos,
                qPrintable(QString("Newline leaked through serialization for payload #%1").arg(i))
            );

            auto parsed = GraphingProtocol::parse(serialized);
            QCOMPARE(parsed.parameters.size(), std::size_t(2));
            QCOMPARE(parsed.parameters[0], payloads[i]);
        }
    }

    // Дополнительно: нуль-байт упомянут в TC-37. Текущая реализация протокола
    // считает строку до первого `\n`, нуль-байты внутри значения сохраняются.
    void allowsNulByteInsideParameter_TC37()
    {
        std::string withNul;
        withNul.push_back('a');
        withNul.push_back('\0');
        withNul.push_back('b');

        auto request = GraphingProtocol::Message::request(1, "login", {withNul, "pass"});
        std::string serialized = GraphingProtocol::serialize(request);
        auto parsed = GraphingProtocol::parse(serialized);

        QCOMPARE(parsed.parameters[0].size(), std::size_t(3));
        QCOMPARE(parsed.parameters[0][0], 'a');
        QCOMPARE(parsed.parameters[0][1], '\0');
        QCOMPARE(parsed.parameters[0][2], 'b');
    }

    // Дополнительно: roundtrip с разными типами запросов.
    void roundtripsRegistrationRequest()
    {
        auto request = GraphingProtocol::Message::request(
            100,
            "register",
            {"u", "p", "name", "u@example.com"}
        );
        auto parsed = GraphingProtocol::parse(GraphingProtocol::serialize(request));
        QCOMPARE(parsed.kind, GraphingProtocol::MessageKind::Request);
        QCOMPARE(parsed.correlationId, std::uint64_t(100));
        QCOMPARE(parsed.type, std::string("register"));
        QCOMPARE(parsed.parameters.size(), std::size_t(4));
    }

    void roundtripsErrorResponse()
    {
        auto response = GraphingProtocol::Message::responseError(5, 3, "Forbidden|access\n");
        auto parsed = GraphingProtocol::parse(GraphingProtocol::serialize(response));
        QCOMPARE(parsed.kind, GraphingProtocol::MessageKind::Response);
        QVERIFY(!parsed.success);
        QCOMPARE(parsed.errorCode, 3);
        QCOMPARE(parsed.errorMessage, std::string("Forbidden|access\n"));
    }

    void roundtripsSuccessResponseWithParameters()
    {
        std::vector<std::string> points;
        for (int i = 0; i < 1000; ++i) {
            points.push_back(std::to_string(i) + ":" + std::to_string(i * 0.5));
        }
        auto response = GraphingProtocol::Message::responseSuccess(42, points);
        auto parsed = GraphingProtocol::parse(GraphingProtocol::serialize(response));
        QVERIFY(parsed.success);
        QCOMPARE(parsed.correlationId, std::uint64_t(42));
        QCOMPARE(parsed.parameters.size(), std::size_t(1000));
        QCOMPARE(parsed.parameters[0], std::string("0:0.000000"));
    }

    // Защита от переполнения целочисленного парсера.
    void rejectsOverflowingCorrelationId()
    {
        QVERIFY(parseThrows("request 99999999999999999999999999999999 calculate 1|2|3"));
    }
};

QTEST_APPLESS_MAIN(TestGraphingProtocol)
#include "tst_protocol.moc"
