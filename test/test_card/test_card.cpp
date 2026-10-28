#include "card.h"
#include "pins.h"

#include <unity.h>

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "../../src/card.cpp"

namespace {

std::string tempRoot;

void removeTree(const std::string& path) {
    struct stat info;
    if (lstat(path.c_str(), &info) != 0) {
        return;
    }
    if (S_ISLNK(info.st_mode) || !S_ISDIR(info.st_mode)) {
        unlink(path.c_str());
        return;
    }
    DIR* dir = opendir(path.c_str());
    if (dir != nullptr) {
        while (dirent* entry = readdir(dir)) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            removeTree(path + "/" + entry->d_name);
        }
        closedir(dir);
    }
    rmdir(path.c_str());
}

void writeFile(const std::string& path) {
    FILE* file = fopen(path.c_str(), "w");
    TEST_ASSERT_NOT_NULL(file);
    fputs("x", file);
    fclose(file);
}

bool listed(const std::vector<CardEntry>& entries, const char* name, bool* directory) {
    for (const CardEntry& entry : entries) {
        if (entry.name == name) {
            if (directory != nullptr) {
                *directory = entry.directory;
            }
            return true;
        }
    }
    return false;
}

}  // namespace

extern "C" void setUp(void) {
    if (!tempRoot.empty()) {
        cardSetRootForTest(nullptr);
        removeTree(tempRoot);
        tempRoot.clear();
    }
    hostFs().reset();
    hostCard().reset();
    hostSd.reset();
    hostResetPins();
    hostDelayCalls = 0;
    hostPullupPin = -1;
    hostDacDisabledPin = -1;
    Serial.out.clear();
    cardSetRootForTest(nullptr);
}

extern "C" void tearDown(void) {
    cardSetRootForTest(nullptr);
    if (!tempRoot.empty()) {
        removeTree(tempRoot);
        tempRoot.clear();
    }
}

extern "C" void test_mount_reports_the_bus_and_retries(void) {
    hostSetPin(kEpdCs, LOW);
    hostSetPin(kSdCs, LOW);
    releaseDisplayBus();
    TEST_ASSERT_EQUAL_INT(HIGH, hostPins[kEpdCs]);
    TEST_ASSERT_EQUAL_INT(HIGH, hostPins[kSdCs]);

    hostSd.failRemaining = 3;
    TEST_ASSERT_FALSE(mountCard());
    TEST_ASSERT_FALSE(cardMounted());
    TEST_ASSERT_EQUAL_INT(3, hostSd.calls);
    TEST_ASSERT_EQUAL_INT(2, hostDelayCalls);
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "No microSD card"));

    hostSd.reset();
    hostSd.failRemaining = 1;
    hostDelayCalls = 0;
    Serial.out.clear();
    TEST_ASSERT_TRUE(mountCard());
    TEST_ASSERT_TRUE(cardMounted());
    TEST_ASSERT_EQUAL_INT(2, hostSd.calls);
    TEST_ASSERT_EQUAL_INT(1, hostDelayCalls);

    hostSd.reset();
    hostDelayCalls = 0;
    Serial.out.clear();
    TEST_ASSERT_TRUE(mountCard());
    TEST_ASSERT_EQUAL_INT(1, hostSd.calls);
    TEST_ASSERT_EQUAL_INT(0, hostDelayCalls);
    TEST_ASSERT_EQUAL_INT(kSdCs, hostSd.cs);
    TEST_ASSERT_EQUAL_UINT32(15000000, hostSd.hz);
    TEST_ASSERT_EQUAL_STRING("/sd", hostSd.mount.c_str());
    TEST_ASSERT_EQUAL_INT(10, hostSd.maxFiles);
    TEST_ASSERT_EQUAL_INT(kSpiMiso, hostPullupPin);
    TEST_ASSERT_EQUAL_INT(kSdCs, hostDacDisabledPin);
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "microSD mounted"));
}

extern "C" void test_listing_skips_dotfiles_and_stats_links(void) {
    char tmpl[] = "/tmp/ereader-card-XXXXXX";
    char* made = mkdtemp(tmpl);
    TEST_ASSERT_NOT_NULL(made);
    tempRoot = made;
    cardSetRootForTest(tempRoot.c_str());

    TEST_ASSERT_EQUAL_INT(0, mkdir((tempRoot + "/Author").c_str(), 0755));
    TEST_ASSERT_EQUAL_INT(0, mkdir((tempRoot + "/Author/Title").c_str(), 0755));
    writeFile(tempRoot + "/Author/Title/book.epub");
    writeFile(tempRoot + "/Author/notes.txt");
    writeFile(tempRoot + "/Author/direct.epub");
    writeFile(tempRoot + "/loose.epub");
    writeFile(tempRoot + "/my book.epub");
    writeFile(tempRoot + "/.hidden");
    TEST_ASSERT_EQUAL_INT(0, symlink("Author", (tempRoot + "/linkdir").c_str()));
    TEST_ASSERT_EQUAL_INT(0, symlink("missing-target", (tempRoot + "/broken").c_str()));

    hostSetPin(kEpdCs, LOW);
    hostSetPin(kSdCs, LOW);
    std::vector<CardEntry> root;
    TEST_ASSERT_TRUE(readCardDirectory("/", root));
    TEST_ASSERT_EQUAL_INT(HIGH, hostPins[kEpdCs]);
    TEST_ASSERT_EQUAL_INT(HIGH, hostPins[kSdCs]);
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "d_type"));

    std::vector<CardEntry> fromNull;
    std::vector<CardEntry> fromEmpty;
    TEST_ASSERT_TRUE(readCardDirectory(nullptr, fromNull));
    TEST_ASSERT_TRUE(readCardDirectory("", fromEmpty));
    TEST_ASSERT_EQUAL_INT(root.size(), fromNull.size());
    TEST_ASSERT_EQUAL_INT(root.size(), fromEmpty.size());

    bool directory = false;
    TEST_ASSERT_TRUE(listed(root, "Author", &directory));
    TEST_ASSERT_TRUE(directory);
    TEST_ASSERT_TRUE(listed(root, "loose.epub", &directory));
    TEST_ASSERT_FALSE(directory);
    TEST_ASSERT_TRUE(listed(root, "my book.epub", &directory));
    TEST_ASSERT_FALSE(directory);
    TEST_ASSERT_TRUE(listed(root, "linkdir", &directory));
    TEST_ASSERT_TRUE(directory);
    TEST_ASSERT_TRUE(listed(root, "broken", &directory));
    TEST_ASSERT_FALSE(directory);
    TEST_ASSERT_FALSE(listed(root, ".hidden", nullptr));
    TEST_ASSERT_FALSE(listed(root, "book.epub", nullptr));

    std::vector<CardEntry> author;
    TEST_ASSERT_TRUE(readCardDirectory("/Author", author));
    TEST_ASSERT_EQUAL_INT(3, author.size());
    TEST_ASSERT_TRUE(listed(author, "Title", &directory));
    TEST_ASSERT_TRUE(directory);
    TEST_ASSERT_TRUE(listed(author, "direct.epub", &directory));
    TEST_ASSERT_FALSE(directory);
    TEST_ASSERT_TRUE(listed(author, "notes.txt", nullptr));
    TEST_ASSERT_FALSE(listed(author, "book.epub", nullptr));

    std::vector<CardEntry> title;
    TEST_ASSERT_TRUE(readCardDirectory("/Author/Title", title));
    TEST_ASSERT_EQUAL_INT(1, title.size());
    TEST_ASSERT_TRUE(listed(title, "book.epub", &directory));
    TEST_ASSERT_FALSE(directory);

    Serial.out.clear();
    std::vector<CardEntry> missing;
    TEST_ASSERT_FALSE(readCardDirectory("/no-such", missing));
    TEST_ASSERT_TRUE(missing.empty());
    TEST_ASSERT_NOT_NULL(strstr(Serial.out.c_str(), "opendir failed"));
    TEST_ASSERT_FALSE(readCardDirectory("/loose.epub", missing));
}

extern "C" void test_book_bytes_come_from_the_card(void) {
    File file = bookFs().open("/note.txt", "w");
    TEST_ASSERT_TRUE(file);
    file.print("card");
    file.close();
    TEST_ASSERT_EQUAL_STRING("card", hostCard().files["/note.txt"].c_str());
    TEST_ASSERT_TRUE(hostFs().files.find("/note.txt") == hostFs().files.end());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_mount_reports_the_bus_and_retries);
    RUN_TEST(test_listing_skips_dotfiles_and_stats_links);
    RUN_TEST(test_book_bytes_come_from_the_card);
    return UNITY_END();
}
