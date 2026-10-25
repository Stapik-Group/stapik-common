#include "stapik/archive/ZipReader.hpp"
#include "stapik/archive/ZipWriter.hpp"

#include "support/FakeClock.hpp"
#include "support/TemporaryDirectory.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using namespace stapik::archive;

    std::vector<std::uint8_t> randomBytes(const std::size_t count)
    {
        std::mt19937 generator(7);
        std::vector<std::uint8_t> bytes(count);
        for (auto& byte : bytes)
            byte = static_cast<std::uint8_t>(generator());
        return bytes;
    }

    std::vector<std::uint8_t> readBytes(const fs::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
    }

    void writeBytes(const fs::path& path, const std::vector<std::uint8_t>& bytes)
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    class ZipTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            m_directory = stapik::test::createTemporaryDirectory("stapik-zip");
            m_path = m_directory / "nested" / "test.zip";
        }

        void TearDown() override
        {
            std::error_code errorCode;
            fs::remove_all(m_directory, errorCode);
        }

        void writeArchive(const std::vector<std::pair<std::string, std::vector<std::uint8_t>>>& files) const
        {
            auto writer = ZipWriter::create(m_path, stapik::test::FakeClock::DEFAULT_START);
            ASSERT_TRUE(writer) << writer.error().message();

            for (const auto& [name, content] : files)
                ASSERT_TRUE((*writer)->add(name, content));

            const auto finished = (*writer)->finish();
            ASSERT_TRUE(finished) << finished.error().message();
        }

        fs::path m_directory;
        fs::path m_path;
    };
}

TEST_F(ZipTest, ReadsBackWhatWasWritten)
{
    const auto photo = randomBytes(4000);
    writeArchive({ { "document.json", { 'a', 'b' } }, { "assets/photo.jpg", photo } });

    const auto reader = ZipReader::open(m_path);
    ASSERT_TRUE(reader) << reader.error().message();
    ASSERT_EQ(reader->entries().size(), 2U);

    const auto document = reader->readText("document.json");
    ASSERT_TRUE(document);
    EXPECT_EQ(*document, "ab");

    const auto restoredPhoto = reader->read("assets/photo.jpg");
    ASSERT_TRUE(restoredPhoto);
    EXPECT_EQ(*restoredPhoto, photo);
}

TEST_F(ZipTest, CreatesTheFolderOfTheArchive)
{
    writeArchive({ { "a", { 1 } } });

    EXPECT_TRUE(fs::exists(m_path));
}

TEST_F(ZipTest, CompressesTextAndStoresDataThatDoesNotShrink)
{
    const std::string text(5000, 'x');
    writeArchive({ { "text.txt", { text.begin(), text.end() } }, { "noise.bin", randomBytes(2000) } });

    const auto reader = ZipReader::open(m_path);
    ASSERT_TRUE(reader);

    EXPECT_EQ(reader->find("text.txt")->method, 8);
    EXPECT_LT(reader->find("text.txt")->compressedSize, 100U);
    EXPECT_EQ(reader->find("noise.bin")->method, 0);
}

TEST_F(ZipTest, KeepsEmptyFilesAndUtf8Names)
{
    writeArchive({ { "assets/zażółć.txt", {} } });

    const auto reader = ZipReader::open(m_path);
    ASSERT_TRUE(reader);

    const auto content = reader->read("assets/zażółć.txt");
    ASSERT_TRUE(content);
    EXPECT_TRUE(content->empty());
}

TEST_F(ZipTest, LeavesNoFileUntilFinished)
{
    {
        auto writer = ZipWriter::create(m_path, stapik::test::FakeClock::DEFAULT_START);
        ASSERT_TRUE(writer);
        ASSERT_TRUE((*writer)->addText("a", "text"));
        EXPECT_FALSE(fs::exists(m_path));
    }

    EXPECT_FALSE(fs::exists(m_path));
    EXPECT_FALSE(fs::exists(fs::path(m_path).concat(".tmp")));
}

TEST_F(ZipTest, ReplacesAnExistingArchive)
{
    writeArchive({ { "old", { 1 } } });
    writeArchive({ { "new", { 2 } } });

    const auto reader = ZipReader::open(m_path);
    ASSERT_TRUE(reader);
    EXPECT_EQ(reader->find("old"), nullptr);
    EXPECT_NE(reader->find("new"), nullptr);
}

TEST_F(ZipTest, ReportsAMissingEntry)
{
    writeArchive({ { "a", { 1 } } });

    const auto reader = ZipReader::open(m_path);
    ASSERT_TRUE(reader);

    EXPECT_EQ(reader->find("b"), nullptr);
    const auto missing = reader->read("b");
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().kind, ArchiveErrorKind::Corrupted);
}

TEST_F(ZipTest, RejectsAFileThatIsNotAnArchive)
{
    fs::create_directories(m_path.parent_path());
    writeBytes(m_path, { 'n', 'o', 't', ' ', 'a', ' ', 'z', 'i', 'p', ' ', 'f', 'i', 'l', 'e', '!', '!', '!', '!', '!', '!', '!', '!' });

    const auto reader = ZipReader::open(m_path);
    ASSERT_FALSE(reader);
    EXPECT_EQ(reader.error().kind, ArchiveErrorKind::NotAnArchive);
}

TEST_F(ZipTest, RejectsALargeFileThatIsNotAnArchiveWithoutDelay)
{
    fs::create_directories(m_path.parent_path());
    writeBytes(m_path, randomBytes(200000));

    const auto started = std::chrono::steady_clock::now();
    const auto reader = ZipReader::open(m_path);
    const auto elapsed = std::chrono::steady_clock::now() - started;

    ASSERT_FALSE(reader);
    EXPECT_EQ(reader.error().kind, ArchiveErrorKind::NotAnArchive);
    EXPECT_LT(elapsed, std::chrono::seconds(1));
}

TEST_F(ZipTest, OpensAnArchiveWithManyEntriesWithoutDelay)
{
    constexpr int ENTRY_COUNT = 5000;
    std::vector<std::pair<std::string, std::vector<std::uint8_t>>> files;
    for (int index = 0; index < ENTRY_COUNT; ++index)
        files.push_back({ "entries/file-" + std::to_string(index) + ".txt", { 'x' } });
    writeArchive(files);

    const auto started = std::chrono::steady_clock::now();
    const auto reader = ZipReader::open(m_path);
    const auto elapsed = std::chrono::steady_clock::now() - started;

    ASSERT_TRUE(reader) << reader.error().message();
    EXPECT_EQ(reader->entries().size(), static_cast<std::size_t>(ENTRY_COUNT));
    EXPECT_LT(elapsed, std::chrono::seconds(1));
}

TEST_F(ZipTest, RejectsATruncatedArchive)
{
    writeArchive({ { "a", randomBytes(1000) } });
    auto bytes = readBytes(m_path);
    bytes.resize(bytes.size() / 2);
    writeBytes(m_path, bytes);

    const auto reader = ZipReader::open(m_path);
    ASSERT_FALSE(reader);
    EXPECT_EQ(reader.error().kind, ArchiveErrorKind::NotAnArchive);
}

TEST_F(ZipTest, ReportsAMissingFile)
{
    const auto reader = ZipReader::open(m_directory / "missing.zip");
    ASSERT_FALSE(reader);
    EXPECT_EQ(reader.error().kind, ArchiveErrorKind::Io);
}

TEST_F(ZipTest, DetectsDamagedContent)
{
    const auto content = randomBytes(1000);
    writeArchive({ { "a", content }, { "b", content } });

    auto bytes = readBytes(m_path);
    bytes[50] ^= 0xFF;
    writeBytes(m_path, bytes);

    const auto reader = ZipReader::open(m_path);
    ASSERT_TRUE(reader);

    const auto damaged = reader->read("a");
    ASSERT_FALSE(damaged);
    EXPECT_EQ(damaged.error().kind, ArchiveErrorKind::Corrupted);
    EXPECT_TRUE(reader->read("b"));
}

TEST_F(ZipTest, TheArchiveIsReadableWithAnyZipTool)
{
    writeArchive({ { "a.txt", { 'h', 'i' } } });

    // The signature of the first local header and of the end of the central directory.
    const auto bytes = readBytes(m_path);
    ASSERT_GT(bytes.size(), 22U);
    EXPECT_EQ((std::vector<std::uint8_t>{ bytes[0], bytes[1], bytes[2], bytes[3] }), (std::vector<std::uint8_t>{ 'P', 'K', 3, 4 }));
    const auto end = bytes.size() - 22;
    EXPECT_EQ((std::vector<std::uint8_t>{ bytes[end], bytes[end + 1], bytes[end + 2], bytes[end + 3] }), (std::vector<std::uint8_t>{ 'P', 'K', 5, 6 }));
}
