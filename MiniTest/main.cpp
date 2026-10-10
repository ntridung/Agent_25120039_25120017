#include <iostream>
#include <string>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

using json = nlohmann::json;

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

std::string callKaggleAgent(const std::string& ngrokUrl, const std::string& userPrompt) {
    CURL* curl = curl_easy_init();
    if (!curl) return "Lỗi: không khởi tạo được libcurl";

    std::string readBuffer;
    std::string fullUrl = ngrokUrl + "/api/chat";

    json payload = {
        {"model", "qwen3.8:27b"},
        {"messages", json::array({
            {{"role", "system"}, {"content", "You are an AI core inside a C++ application."}},
            {{"role", "user"},   {"content", userPrompt}}
        })},
        {"stream", false}
    };
    std::string jsonStr = payload.dump();

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "ngrok-skip-browser-warning: true");

    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(jsonStr.size()));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);   // timeout kết nối
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 180L);         // timeout tổng (model có thể chậm)
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return "Lỗi kết nối: " + std::string(curl_easy_strerror(res));
    }
    if (httpCode != 200) {
        return "Lỗi HTTP " + std::to_string(httpCode) + "\nRaw response: " + readBuffer;
    }

    try {
        json resJson = json::parse(readBuffer);

        if (resJson.contains("error")) {
            return "Lỗi từ server: " + resJson["error"].dump();
        }
        if (resJson.contains("message") && resJson["message"].contains("content")) {
            return resJson["message"]["content"].get<std::string>();
        }
        return "Response không đúng định dạng mong đợi:\n" + readBuffer;
    } catch (const std::exception& e) {
        return "Lỗi parse JSON: " + std::string(e.what()) + "\nRaw response: " + readBuffer;
    }
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    curl_global_init(CURL_GLOBAL_DEFAULT);

    std::string ngrokBaseUrl = "https://crusader-wildly-urology.ngrok-free.dev";
    std::string prompt;
    std::getline(std::cin, prompt);
    std::cin.ignore();

    std::cout << "Đang gửi request tới Kaggle Backend...\n";
    std::string response = callKaggleAgent(ngrokBaseUrl, prompt);

    std::cout << "\n--- AI Response ---\n" << response << std::endl;

    curl_global_cleanup();
    return 0;
}