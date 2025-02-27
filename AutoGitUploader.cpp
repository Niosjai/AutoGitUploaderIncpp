#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Configuration variables
std::string GITHUB_TOKEN;
std::string GITHUB_USERNAME;
std::string REPO_NAME;
std::string BRANCH;
bool HIDDEN; // New configuration variable

// Function to load configuration from a JSON file
void load_config(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file) {
        std::cerr << "Error: Could not open config file: " << config_file << std::endl;
        exit(1);
    }

    json config;
    file >> config;

    GITHUB_TOKEN = config.value("GITHUB_TOKEN", "");
    GITHUB_USERNAME = config.value("GITHUB_USERNAME", "");
    REPO_NAME = config.value("REPO_NAME", "");
    BRANCH = config.value("BRANCH", "main");
    HIDDEN = config.value("HIDDEN", true); // Default to true if not specified

    if (GITHUB_TOKEN.empty() || GITHUB_USERNAME.empty() || REPO_NAME.empty()) {
        std::cerr << "Error: Missing required configuration in config file." << std::endl;
        exit(1);
    }
}

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string base64_encode(const std::vector<unsigned char>& data) {
    const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    std::string encoded;
    int i = 0;
    unsigned char char_array_3[3], char_array_4[4];

    for (unsigned char byte : data) {
        char_array_3[i++] = byte;
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (int j = 0; j < 4; j++)
                encoded += base64_chars[char_array_4[j]];
            i = 0;
        }
    }

    if (i) {
        for (int j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (int j = 0; j < i + 1; j++)
            encoded += base64_chars[char_array_4[j]];

        while (i++ < 3)
            encoded += '=';
    }

    return encoded;
}

std::string url_encode_path(const std::string& path) {
    CURL* curl = curl_easy_init();
    std::ostringstream oss;
    std::istringstream iss(path);
    std::string part;

    while (std::getline(iss, part, '/')) {
        if (curl) {
            char* encoded = curl_easy_escape(curl, part.c_str(), part.size());
            oss << (oss.tellp() > 0 ? "/" : "") << encoded;
            curl_free(encoded);
        }
    }
    curl_easy_cleanup(curl);
    return oss.str();
}

std::string api_get_request(const std::string& url) {
    CURL* curl = curl_easy_init();
    std::string response;
    struct curl_slist* headers = NULL;

    if (curl) {
        headers = curl_slist_append(headers, ("Authorization: token " + GITHUB_TOKEN).c_str());
        headers = curl_slist_append(headers, "Accept: application/vnd.github.v3+json");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "curl/7.79.1");

        curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    return response;
}

bool api_put_request(const std::string& url, const std::string& data) {
    CURL* curl = curl_easy_init();
    struct curl_slist* headers = NULL;
    long response_code;
    std::string response;

    if (curl) {
        headers = curl_slist_append(headers, ("Authorization: token " + GITHUB_TOKEN).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, "Accept: application/vnd.github.v3+json");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data.length());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "curl/7.79.1");

        curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    return response_code == 200 || response_code == 201;
}

std::vector<std::filesystem::path> get_files_to_upload() {
    std::vector<std::filesystem::path> files;
    for (auto it = std::filesystem::recursive_directory_iterator("."); 
         it != std::filesystem::recursive_directory_iterator(); ++it) {
        
        const auto& entry = *it;

        // Skip hidden directories if HIDDEN is false
        if (entry.is_directory()) {
            std::string dirname = entry.path().filename().string();
            if (!HIDDEN && dirname[0] == '.') {
                it.disable_recursion_pending(); // Prevent entering the directory
                continue;
            }
        }

        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            std::string ext = entry.path().extension().string();

            
            if (filename == "config.json" || filename == "autogituploader" || filename == "AutoGitUploader.cpp" || ext == ".bak") {
                continue;
            }

            // Skip hidden files if HIDDEN is false
            if (!HIDDEN && filename[0] == '.') {
                continue;
            }

            files.push_back(entry.path());
        }
    }
    return files;
}

void upload_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return;

    std::vector<unsigned char> content(file.tellg());
    file.seekg(0);
    file.read(reinterpret_cast<char*>(content.data()), content.size());

    std::string github_path = path.generic_string();
    if (github_path.find("./") == 0) github_path.erase(0, 2);
    std::string api_url = "https://api.github.com/repos/" + GITHUB_USERNAME + "/" + REPO_NAME + 
                        "/contents/" + url_encode_path(github_path);

    json get_response = json::parse(api_get_request(api_url));
    json put_data = {
        {"message", "uploaded via automation script by mario 🚀"},
        {"content", base64_encode(content)},
        {"branch", BRANCH}
    };

    if (get_response.contains("sha")) {
        put_data["sha"] = get_response["sha"].get<std::string>();
    }

    bool success = api_put_request(api_url, put_data.dump());
    std::cout << (success ? "✅ " : "❌ ") << github_path << std::endl;
}

int main() {
    // Load configuration from config.json
    load_config("config.json");

    curl_global_init(CURL_GLOBAL_ALL);
    for (const auto& file : get_files_to_upload()) upload_file(file);
    curl_global_cleanup();
    return 0;
}