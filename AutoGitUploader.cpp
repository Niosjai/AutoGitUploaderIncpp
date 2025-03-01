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
bool HIDDEN;

// Load configuration from a JSON file
void load_config(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file) {
        std::cerr << "Error: Could not open config file: " << config_file << std::endl;
        exit(1);
    }

    json config;
    try {
        file >> config;
    } catch (const json::parse_error& e) {
        std::cerr << "Config JSON Error: " << e.what() << std::endl;
        exit(1);
    }

    GITHUB_TOKEN = config.value("GITHUB_TOKEN", "");
    GITHUB_USERNAME = config.value("GITHUB_USERNAME", "");
    REPO_NAME = config.value("REPO_NAME", "");
    BRANCH = config.value("BRANCH", "main");
    HIDDEN = config.value("HIDDEN", true);

    if (GITHUB_TOKEN.empty() || GITHUB_USERNAME.empty() || REPO_NAME.empty()) {
        std::cerr << "Error: Missing required configuration in config file." << std::endl;
        exit(1);
    }
}

// Callback function for CURL to write response data
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Base64 encode file content
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

// URL encode file paths
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

// Perform a GET request to GitHub API
std::string api_get_request(const std::string& url) {
    CURL* curl = curl_easy_init();
    std::string response;
    struct curl_slist* headers = nullptr;

    if (curl) {
        headers = curl_slist_append(headers, ("Authorization: token " + GITHUB_TOKEN).c_str());
        headers = curl_slist_append(headers, "Accept: application/vnd.github.v3+json");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "AutoGitUploader/1.0");
        curl_easy_setopt(curl, CURLOPT_CAINFO, "cacert.pem");  // SSL certificate bundle

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "CURL Error: " << curl_easy_strerror(res) << std::endl;
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    return response;
}

// Perform a PUT request to GitHub API
bool api_put_request(const std::string& url, const std::string& data) {
    CURL* curl = curl_easy_init();
    struct curl_slist* headers = nullptr;
    long response_code = 0;
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
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "AutoGitUploader/1.0");
        curl_easy_setopt(curl, CURLOPT_CAINFO, "cacert.pem");  // SSL certificate bundle

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "CURL Error: " << curl_easy_strerror(res) << std::endl;
        }

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    return response_code == 200 || response_code == 201;
}

// Get list of files to upload
std::vector<std::filesystem::path> get_files_to_upload() {
    std::vector<std::filesystem::path> files;
    for (auto it = std::filesystem::recursive_directory_iterator(".");
         it != std::filesystem::recursive_directory_iterator(); ++it) {
        const auto& entry = *it;

        if (entry.is_directory()) {
            std::string dirname = entry.path().filename().string();
            if (!HIDDEN && dirname[0] == '.') {
                it.disable_recursion_pending();
                continue;
            }
        }

        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            std::string ext = entry.path().extension().string();

            if (filename == "config.json" || 
                filename == "autogituploader" || 
                filename == "AutoGitUploader.cpp" || 
                filename == "cacert.pem" || 
                filename == "libcurl-x64.dll" || // Now correctly excluding libcurl-x64
                ext == ".bak") {
                continue;
            }

            if (!HIDDEN && filename[0] == '.') {
                continue;
            }

            files.push_back(entry.path());
        }
    }
    return files;
}

// Upload a file to GitHub
void upload_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "❌ Failed to open file: " << path << std::endl;
        return;
    }

    std::vector<unsigned char> content(file.tellg());
    file.seekg(0);
    file.read(reinterpret_cast<char*>(content.data()), content.size());

    std::string github_path = path.generic_string();
    if (github_path.find("./") == 0) github_path.erase(0, 2);
    std::string api_url = "https://api.github.com/repos/" + GITHUB_USERNAME + "/" + REPO_NAME +
                        "/contents/" + url_encode_path(github_path);

    std::string response = api_get_request(api_url);
    if (response.empty()) {
        std::cerr << "❌ Empty API response for: " << github_path
                  << "\n   Check: Token permissions, repo existence, and network connection" << std::endl;
        return;
    }

    json get_response;
    try {
        get_response = json::parse(response);
    } catch (const json::parse_error& e) {
        std::cerr << "❌ JSON Error (" << github_path << "): " << e.what()
                  << "\n   API Response: " << response << std::endl;
        return;
    }

    json put_data = {
        {"message", "Uploaded via automation script by Mario 🚀"},
        {"content", base64_encode(content)},
        {"branch", BRANCH}
    };

    if (get_response.contains("sha")) {
        put_data["sha"] = get_response["sha"].get<std::string>();
    }

    bool success = api_put_request(api_url, put_data.dump());
    std::cout << (success ? "✅ " : "❌ ") << github_path << std::endl;
}

// Main function
int main() {
    load_config("config.json");
    curl_global_init(CURL_GLOBAL_ALL);

    try {
        auto files = get_files_to_upload();
        std::cout << "Found " << files.size() << " files to upload\n";
        for (const auto& file : files) upload_file(file);
    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
    }

    curl_global_cleanup();
    return 0;
}