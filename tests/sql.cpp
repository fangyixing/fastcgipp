#include <iostream>
#include <memory>
#include <algorithm>
#include <array>
#include <queue>
#include <condition_variable>
#include <random>
#include <sstream>
#include <iomanip>

#include "fastcgi++/sql/connection.hpp"

const unsigned totalQueries = 1;//30000;
const unsigned maxQueriesSize = 1000;

class TestQuery
{
private:
    static Fastcgipp::SQL::Connection connection;

    std::tuple<
        int16_t,
        int64_t,
        std::string,
        float,
        double,
        std::vector<char>,
        std::wstring> m_parameters;

    std::shared_ptr<Fastcgipp::SQL::Results<int32_t>> m_insertResult;

    std::shared_ptr<Fastcgipp::SQL::Results<
        int16_t,
        int64_t,
        std::string,
        float,
        double,
        std::vector<char>,
        std::wstring,
        std::wstring>> m_selectResults;

    std::shared_ptr<Fastcgipp::SQL::Results<>> m_deleteResult;

    std::function<void(Fastcgipp::Message)> m_callback;

    unsigned m_state;

    static std::map<unsigned, TestQuery> queries;

    static void callback(unsigned id, Fastcgipp::Message message);
    static std::queue<unsigned> queue;
    static std::mutex mutex;
    static std::condition_variable wake;

    static std::random_device device;
    static std::uniform_int_distribution<int16_t> int16dist;
    static std::uniform_int_distribution<int32_t> int32dist;
    static std::uniform_int_distribution<int64_t> int64dist;
    static std::normal_distribution<float> floatdist;
    static std::normal_distribution<double> doubledist;
    static std::array<std::wstring, 6> wstrings;
    static std::array<std::string, 6> strings;
    static std::array<std::vector<char>, 6> vectors;
    static std::uniform_int_distribution<unsigned> stringdist;

    bool handle();

public:
    TestQuery():
        m_state(0)
    {
    }

    static void init()
    {
        connection.init(
                "",
                "fastcgipp_test",
                "fastcgipp_test",
                "fastcgipp_test",
                8);
        connection.start();
    }

    static void stop()
    {
        connection.stop();
        connection.join();
    }

    static void handler();
};

Fastcgipp::SQL::Connection TestQuery::connection;
std::map<unsigned, TestQuery> TestQuery::queries;
std::queue<unsigned> TestQuery::queue;
std::mutex TestQuery::mutex;
std::condition_variable TestQuery::wake;

std::random_device TestQuery::device;
std::uniform_int_distribution<int16_t> TestQuery::int16dist(
        std::numeric_limits<int16_t>::min(),
        std::numeric_limits<int16_t>::max());
std::uniform_int_distribution<int32_t> TestQuery::int32dist(
        std::numeric_limits<int32_t>::min(),
        std::numeric_limits<int32_t>::max());
std::uniform_int_distribution<int64_t> TestQuery::int64dist(
        std::numeric_limits<int64_t>::min(),
        std::numeric_limits<int64_t>::max());
std::normal_distribution<float> TestQuery::floatdist(0, 1000);
std::normal_distribution<double> TestQuery::doubledist(0, 10000);
std::array<std::wstring, 6> TestQuery::wstrings
{
    L"Hello World",
    L"Привет мир",
    L"Γεια σας κόσμο",
    L"世界您好",
    L"今日は世界",
    L"ᚺᛖᛚᛟ ᚹᛟᛉᛚᛞ"
};
std::array<std::string, 6> TestQuery::strings
{
    "Leviathan Wakes",
    "Caliban's War",
    "Abaddon's Gate",
    "Cibola Burn",
    "Nemesis Games",
    "Babylon's Ashes"
};
std::array<std::vector<char>, 6> TestQuery::vectors
{{
    {'a', 'b', 'c', 'd', 'e', 'f'},
    {'b', 'c', 'd', 'e', 'f'},
    {'c', 'd', 'e', 'f'},
    {'d', 'e', 'f'},
    {'e', 'f'},
    {'f'}
}};
std::uniform_int_distribution<unsigned> TestQuery::stringdist(0,5);

void TestQuery::callback(unsigned id, Fastcgipp::Message message)
{
    std::lock_guard<std::mutex> lock(mutex);
    queue.push(id);
    wake.notify_one();
}

void TestQuery::handler()
{
    unsigned remaining = totalQueries;
    unsigned index=0;

    while(remaining)
    {
        while(index<totalQueries && queries.size()<maxQueriesSize)
        {
            if(queries.find(index) != queries.end())
                FAIL_LOG("Fastcgipp::SQL::Connection test fail #1")
            auto& query = queries[index];
            query.m_callback = std::bind(
                    &TestQuery::callback,
                    index,
                    std::placeholders::_1);
            query.handle();
            ++index;
        }

        unsigned id;
        {
            std::unique_lock<std::mutex> lock(mutex);
            if(queue.empty())
                wake.wait(lock);
            id = queue.front();
            queue.pop();
        }

        const auto it = queries.find(id);
        if(it == queries.end())
            FAIL_LOG("Fastcgipp::SQL::Connection test fail #2")
        if(it->second.handle())
        {
            queries.erase(it);
            --remaining;
        }
    }
}

bool TestQuery::handle()
{
    switch(m_state)
    {
        case 0:
        {
            m_parameters = std::make_tuple(
                int16dist(device),
                int64dist(device),
                strings[stringdist(device)],
                floatdist(device),
                doubledist(device),
                vectors[stringdist(device)],
                wstrings[stringdist(device)]);
            m_insertResult.reset(new Fastcgipp::SQL::Results<int32_t>);

            Fastcgipp::SQL::Query query;
            query.statement = "INSERT INTO fastcgipp_test (zero, one, two, three, four, five, six, seven) VALUES (DEFAULT, $1, $2, $3, $4, $5, $6, $7) RETURNING zero;";
            query.parameters = Fastcgipp::SQL::make_Parameters(m_parameters);
            query.results = m_insertResult;
            query.callback = m_callback;
            if(!connection.query(query))
                FAIL_LOG("Fastcgipp::SQL::Connection test fail #3")

            ++m_state;
            return false;
        }

        case 1:
        {
            if(m_insertResult->status() != Fastcgipp::SQL::Status::rowsOk)
                FAIL_LOG("Fastcgipp::SQL::Connection test fail #4: " << m_insertResult->errorMessage())
            if(m_insertResult->rows() != 1)
                FAIL_LOG("Fastcgipp::SQL::Connection test fail #5")
            if(m_insertResult->verify() != 0)
                FAIL_LOG("Fastcgipp::SQL::Connection test fail #6")

            const int32_t id = std::get<0>(m_insertResult->row(0));

            auto parameters = Fastcgipp::SQL::make_Parameters(id);
            m_selectResults.reset(new Fastcgipp::SQL::Results<
                    int16_t,
                    int64_t,
                    std::string,
                    float,
                    double,
                    std::vector<char>,
                    std::wstring,
                    std::wstring>);

            Fastcgipp::SQL::Query query;
            query.statement = "SELECT one, two, three, four, five, six, seven, zero::text || ' ' || one::text || ' ' || two::text || ' ' || three || ' ' || to_char(four, '9.999EEEE') || ' ' || to_char(five, '9.999EEEE') || ' ' || seven AS eight FROM fastcgipp_test WHERE zero=$1;";
            query.parameters = parameters;
            query.results = m_selectResults;
            query.callback = m_callback;
            if(!connection.query(query))
                FAIL_LOG("Fastcgipp::SQL::Connection test fail #7")

            ++m_state;
            return false;
        }

        case 2:
        {
            if(m_selectResults->status() != Fastcgipp::SQL::Status::rowsOk)
                FAIL_LOG("Fastcgipp::SQL::Connection test fail #8: " << m_selectResults->errorMessage())
            if(m_selectResults->rows() != 1)
                FAIL_LOG("Fastcgipp::SQL::Connection test fail #9")
            if(m_selectResults->verify() != 0)
                FAIL_LOG("Fastcgipp::SQL::Connection test fail #10: " << m_selectResults->verify())

            const auto row = m_selectResults->row(0);

            if(std::get<0>(row) != std::get<0>(m_parameters))
                FAIL_LOG("Fastcgipp::SQL::Connection test fail #11")
            if(std::get<1>(row) != std::get<1>(m_parameters))
                FAIL_LOG("Fastcgipp::SQL::Connection test fail #12")
            if(std::get<2>(row) != std::get<2>(m_parameters))
                FAIL_LOG("Fastcgipp::SQL::Connection test fail #13")
            if(std::get<3>(row) != std::get<3>(m_parameters))
                FAIL_LOG("Fastcgipp::SQL::Connection test fail #14")
            if(std::get<4>(row) != std::get<4>(m_parameters))
                FAIL_LOG("Fastcgipp::SQL::Connection test fail #15")
            if(std::get<5>(row) != std::get<5>(m_parameters))
                FAIL_LOG("Fastcgipp::SQL::Connection test fail #16")
            if(std::get<6>(row) != std::get<6>(m_parameters))
                FAIL_LOG("Fastcgipp::SQL::Connection test fail #17")

            std::wostringstream ss;
            ss  << std::get<0>(m_insertResult->row(0)) << ' '
                << std::get<0>(m_parameters) << ' '
                << std::get<1>(m_parameters) << ' '
                << std::get<2>(m_parameters).c_str() << ' '
                << std::scientific << std::setprecision(3)
                << (std::get<3>(m_parameters)>0?" ":"")
                << std::get<3>(m_parameters) << ' '
                << (std::get<4>(m_parameters)>0?" ":"")
                << std::get<4>(m_parameters) << ' '
                << std::get<6>(m_parameters);

            if(ss.str() != std::get<7>(row))
                FAIL_LOG("Fastcgipp::SQL::Connection test fail #19")

            const int32_t id = std::get<0>(m_insertResult->row(0));

            auto parameters = Fastcgipp::SQL::make_Parameters(id);
            m_deleteResult.reset(new Fastcgipp::SQL::Results<>);

            Fastcgipp::SQL::Query query;
            query.statement = "DELETE FROM fastcgipp_test WHERE zero=$1;";
            query.parameters = parameters;
            query.results = m_deleteResult;
            query.callback = m_callback;
            if(!connection.query(query))
                FAIL_LOG("Fastcgipp::SQL::Connection test fail #20")

            ++m_state;
            return false;
        }

        case 3:
        {
            if(m_deleteResult->status() != Fastcgipp::SQL::Status::commandOk)
                FAIL_LOG("Fastcgipp::SQL::Connection test fail #21: " << m_deleteResult->errorMessage())
            if(m_deleteResult->rows() != 0)
                FAIL_LOG("Fastcgipp::SQL::Connection test fail #22")
            if(m_deleteResult->affectedRows() != 1)
                FAIL_LOG("Fastcgipp::SQL::Connection test fail #23")
            if(m_deleteResult->verify() != 0)
                FAIL_LOG("Fastcgipp::SQL::Connection test fail #24: " << m_deleteResult->verify())

            return true;
        }
    }
}

int main()
{
    // Test the SQL parameters stuff
    {
        static const int16_t zero = -1413;
        static const int32_t one = 123342945;
        static const int64_t two = -123342945112312323;
        static const std::string three = "This is a test!!34234";
        static const float four = -1656e-8;
        static const double five = 2354e15;
        static const std::vector<char> six{'a', 'b', 'c', 'd', 'e', 'f'};

        static const std::wstring seven(L"インターネット");
        static const std::array<unsigned char, 21> properEight =
        {
            0xe3, 0x82, 0xa4, 0xe3, 0x83, 0xb3, 0xe3, 0x82, 0xbf, 0xe3, 0x83,
            0xbc, 0xe3, 0x83, 0x8d, 0xe3, 0x83, 0x83, 0xe3, 0x83, 0x88
        };

        auto data(Fastcgipp::SQL::make_Parameters(
                zero,
                one,
                two,
                three,
                four,
                five,
                six,
                seven));
        std::shared_ptr<Fastcgipp::SQL::Parameters_base> base(data);
        data.reset();

        base->build();

        if(
                *(base->oids()+0) != INT2OID ||
                *(base->sizes()+0) != 2 ||
                Fastcgipp::Protocol::BigEndian<int16_t>::read(
                    *(base->raws()+0)) != zero)
            FAIL_LOG("Fastcgipp::SQL::Parameters failed on column 0")
        if(
                *(base->oids()+1) != INT4OID ||
                *(base->sizes()+1) != 4 ||
                Fastcgipp::Protocol::BigEndian<int32_t>::read(
                    *(base->raws()+1)) != one)
            FAIL_LOG("Fastcgipp::SQL::Parameters failed on column 1")
        if(
                *(base->oids()+2) != INT8OID ||
                *(base->sizes()+2) != 8 ||
                Fastcgipp::Protocol::BigEndian<int64_t>::read(
                    *(base->raws()+2)) != two)
            FAIL_LOG("Fastcgipp::SQL::Parameters failed on column 2")
        if(
                *(base->oids()+3) != TEXTOID ||
                *(base->sizes()+3) != three.size() ||
                !std::equal(
                    three.begin(),
                    three.end(),
                    *(base->raws()+3),
                    *(base->raws()+3)+*(base->sizes()+3)))
            FAIL_LOG("Fastcgipp::SQL::Parameters failed on column 3")
        if(
                *(base->oids()+4) != FLOAT4OID ||
                *(base->sizes()+4) != 4 ||
                Fastcgipp::Protocol::BigEndian<float>::read(
                    *(base->raws()+4)) != four)
            FAIL_LOG("Fastcgipp::SQL::Parameters failed on column 4")
        if(
                *(base->oids()+5) != FLOAT8OID ||
                *(base->sizes()+5) != 8 ||
                Fastcgipp::Protocol::BigEndian<double>::read(
                    *(base->raws()+5)) != five)
            FAIL_LOG("Fastcgipp::SQL::Parameters failed on column 5")
        if(
                *(base->oids()+6) != BYTEAOID ||
                *(base->sizes()+6) != six.size() ||
                !std::equal(six.cbegin(), six.cend(), *(base->raws()+6)))
            FAIL_LOG("Fastcgipp::SQL::Parameters failed on column 6")
        if(
                *(base->oids()+7) != TEXTOID ||
                *(base->sizes()+7) != properEight.size() ||
                !std::equal(
                    reinterpret_cast<const char*>(properEight.begin()),
                    reinterpret_cast<const char*>(properEight.end()),
                    *(base->raws()+7),
                    *(base->raws()+7)+*(base->sizes()+7)))
            FAIL_LOG("Fastcgipp::SQL::Parameters failed on column 7")
        for(
                const int* value = base->formats();
                value != base->formats() + base->size();
                ++value)
            if(*value != 1)
                FAIL_LOG("Fastcgipp::SQL::Parameters failed formats array")
    }

    // Test the SQL Connection
    {
        using namespace std::chrono_literals;
        TestQuery::init();
        std::this_thread::sleep_for(1s);
        TestQuery::handler();
        TestQuery::stop();
    }

    return 0;
}
