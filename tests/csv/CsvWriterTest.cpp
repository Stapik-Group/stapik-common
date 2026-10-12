#include "stapik/csv/CsvWriter.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    using stapik::csv::CsvOptions;
    using stapik::csv::CsvSeparator;
    using stapik::csv::CsvWriter;

    constexpr auto SEMICOLON_OPTIONS = CsvOptions{ .separator = CsvSeparator::Semicolon, .byteOrderMark = false };

    std::string readAll(const fs::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
    }

    class CsvWriterFileTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            auto pattern = (fs::temp_directory_path() / "stapik-csv-XXXXXX").string();
            ASSERT_NE(mkdtemp(pattern.data()), nullptr);
            m_directory = pattern;
        }

        void TearDown() override
        {
            std::error_code errorCode;
            fs::remove_all(m_directory, errorCode);
        }

        fs::path m_directory;
    };
}

TEST(CsvWriterTest, StartsEmpty)
{
    const CsvWriter writer;

    EXPECT_TRUE(writer.content().empty());
}

TEST(CsvWriterTest, EndsEveryRowWithCrLf)
{
    CsvWriter writer;
    writer.addRow({ "date", "amount" });
    writer.addRow({ "2026-10-12", "12.50" });

    EXPECT_EQ(writer.content(), "date,amount\r\n2026-10-12,12.50\r\n");
}

TEST(CsvWriterTest, QuotesFieldWithSeparator)
{
    CsvWriter writer;
    writer.addRow({ "milk, 2%", "3" });

    EXPECT_EQ(writer.content(), "\"milk, 2%\",3\r\n");
}

TEST(CsvWriterTest, QuotesFieldWithLineBreaks)
{
    CsvWriter writer;
    writer.addRow({ "first\nsecond", "first\r\nsecond", "first\rsecond" });

    EXPECT_EQ(writer.content(), "\"first\nsecond\",\"first\r\nsecond\",\"first\rsecond\"\r\n");
}

TEST(CsvWriterTest, DoublesQuotesInsideQuotedField)
{
    CsvWriter writer;
    writer.addRow({ "say \"hi\"", "\"" });

    EXPECT_EQ(writer.content(), "\"say \"\"hi\"\"\",\"\"\"\"\r\n");
}

TEST(CsvWriterTest, KeepsEmptyFieldsBetweenOthersUnquoted)
{
    CsvWriter writer;
    writer.addRow({ "a", "", "b", "" });

    EXPECT_EQ(writer.content(), "a,,b,\r\n");
}

TEST(CsvWriterTest, QuotesLoneEmptyFieldSoTheRowIsNotABlankLine)
{
    CsvWriter writer;
    writer.addRow({ "" });

    EXPECT_EQ(writer.content(), "\"\"\r\n");
}

TEST(CsvWriterTest, KeepsSpacesAndUtf8Untouched)
{
    CsvWriter writer;
    writer.addRow({ " Zażółć gęślą jaźń ", "€" });

    EXPECT_EQ(writer.content(), " Zażółć gęślą jaźń ,€\r\n");
}

TEST(CsvWriterTest, SemicolonSeparatesFieldsAndLeavesCommasUnquoted)
{
    CsvWriter writer(SEMICOLON_OPTIONS);
    writer.addRow({ "12,50", "PLN" });

    EXPECT_EQ(writer.content(), "12,50;PLN\r\n");
}

TEST(CsvWriterTest, SemicolonSeparatorQuotesFieldsWithSemicolon)
{
    CsvWriter writer(SEMICOLON_OPTIONS);
    writer.addRow({ "a;b", "c" });

    EXPECT_EQ(writer.content(), "\"a;b\";c\r\n");
}

TEST(CsvWriterTest, CommaSeparatorLeavesSemicolonsUnquoted)
{
    CsvWriter writer;
    writer.addRow({ "a;b", "c" });

    EXPECT_EQ(writer.content(), "a;b,c\r\n");
}

TEST(CsvWriterTest, WritesNoByteOrderMarkByDefault)
{
    CsvWriter writer;
    writer.addRow({ "a" });

    EXPECT_EQ(writer.content(), "a\r\n");
}

TEST(CsvWriterTest, WritesByteOrderMarkOnceAtTheStart)
{
    CsvWriter writer(CsvOptions{ .separator = CsvSeparator::Comma, .byteOrderMark = true });
    writer.addRow({ "a" });
    writer.addRow({ "b" });

    EXPECT_EQ(writer.content(), "\xEF\xBB\xBF" "a\r\nb\r\n");
}

TEST(CsvWriterTest, ByteOrderMarkAloneIsWrittenForEmptyDocument)
{
    const CsvWriter writer(CsvOptions{ .separator = CsvSeparator::Comma, .byteOrderMark = true });

    EXPECT_EQ(writer.content(), "\xEF\xBB\xBF");
}

TEST(CsvOptionsTest, ExcelOptionsForPolishUseSemicolonAndByteOrderMark)
{
    const auto options = CsvOptions::forExcel("pl");

    EXPECT_EQ(options.separator, CsvSeparator::Semicolon);
    EXPECT_TRUE(options.byteOrderMark);
}

TEST(CsvOptionsTest, ExcelOptionsForRegionVariantOfPolishUseSemicolon)
{
    EXPECT_EQ(CsvOptions::forExcel("pl-PL").separator, CsvSeparator::Semicolon);
}

TEST(CsvOptionsTest, ExcelOptionsForGermanUseSemicolon)
{
    EXPECT_EQ(CsvOptions::forExcel("de").separator, CsvSeparator::Semicolon);
}

TEST(CsvOptionsTest, ExcelOptionsForEnglishUseCommaAndByteOrderMark)
{
    const auto options = CsvOptions::forExcel("en");

    EXPECT_EQ(options.separator, CsvSeparator::Comma);
    EXPECT_TRUE(options.byteOrderMark);
}

TEST_F(CsvWriterFileTest, SavesContentByteForByte)
{
    CsvWriter writer(CsvOptions::forExcel("pl"));
    writer.addRow({ "nazwa", "kwota" });
    writer.addRow({ "mleko", "3,50" });
    const auto path = m_directory / "export" / "data.csv";

    ASSERT_TRUE(writer.saveTo(path));

    EXPECT_EQ(readAll(path), "\xEF\xBB\xBF" "nazwa;kwota\r\nmleko;3,50\r\n");
}

TEST_F(CsvWriterFileTest, SaveFailsWhenParentIsNotADirectory)
{
    const auto blocker = m_directory / "blocker";
    std::ofstream(blocker) << "i am a file";
    CsvWriter writer;
    writer.addRow({ "a" });

    const auto result = writer.saveTo(blocker / "data.csv");

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().path, blocker);
}
