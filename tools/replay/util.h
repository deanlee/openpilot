#pragma once

#include <atomic>
#include <functional>
#include <string>

std::string sha256(const std::string &str);
void precise_nano_sleep(long sleep_ns);
std::string decompressBZ2(const std::string &in, std::atomic<bool> *abort = nullptr);
std::string decompressBZ2(const std::byte *in, size_t in_size, std::atomic<bool> *abort = nullptr);
std::string getUrlWithoutQuery(const std::string &url);
size_t getRemoteFileSize(const std::string &url, std::atomic<bool> *abort = nullptr);
std::string httpGet(const std::string &url, size_t chunk_size = 0, std::atomic<bool> *abort = nullptr);

typedef std::function<void(uint64_t cur, uint64_t total, bool success)> DownloadProgressHandler;
void installDownloadProgressHandler(DownloadProgressHandler);
bool httpDownload(const std::string &url, const std::string &file, size_t chunk_size = 0, std::atomic<bool> *abort = nullptr);
std::string formattedDataSize(size_t size);
