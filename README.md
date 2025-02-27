
# 📘 AutoGitUploader Guide

A C++ tool to automate uploading files to GitHub repositories. Perfect for backups, deployments, or syncing workflows! 🚀

![GitHub](https://img.shields.io/badge/GitHub-Automation-blue?logo=github)

---

## 🌟 Features
- **Recursive directory scanning**  
- **Skip hidden files/directories** (configurable)  
- **Base64 encoding** for GitHub API compliance  
- **Update existing files** using SHA checks  
- Simple **JSON configuration**  

---

## 🛠️ Prerequisites
- `libcurl` (for HTTP requests)
- `nlohmann/json` (JSON parsing)
- C++17 compiler

**Install dependencies on Ubuntu:**
```bash
sudo apt-get install libcurl4-openssl-dev
```

---

## ⚙️ Installation
1. **Clone the repository** (or download `AutoGitUploader.cpp`):
   ```bash
   git clone https://github.com/Niosjai/AutoGitUploaderIncpp.git
   cd AutoGitUploaderIncpp
   ```
2. **Compile the code**:
   ```bash
   g++ -std=c++17 -o autogituploader AutoGitUploader.cpp -lcurl -lstdc++fs
   ```

---

## 🔧 Configuration
Create a `config.json` file:
```json
{
  "GITHUB_TOKEN": "your_github_token",
  "GITHUB_USERNAME": "your_username",
  "REPO_NAME": "your-repo",
  "BRANCH": "main",
  "HIDDEN": false
}
```
- 🔒 **Token Guide**: Create a GitHub token [here](https://github.com/settings/tokens) with `repo` scope.  
- `HIDDEN: true` skips files/directories starting with `.`.

---

## 🚀 Usage
```bash
./autogituploader
```
**Example Output:**
```
✅ src/main.cpp
✅ docs/README.md
❌ failed_file.txt
```

---

## 📝 Notes
- Files automatically skipped:  
  `config.json`, `autogituploader`, `*.bak`, and hidden files (if `HIDDEN: true`).  
- Commit message is hardcoded as:  
  `"uploaded via automation script by mario 🚀"`.  
---

## 🚨 Troubleshooting
- **Compilation errors**: Ensure `libcurl` and `nlohmann/json` are installed.  
- **Permission denied**: Run `chmod +x autogituploader`.  
- **API failures**: Verify token permissions and repo name.  

---

💻 **Happy Automating!**  
*Star this repo if you find it useful!* ⭐️  

