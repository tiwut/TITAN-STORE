# Titan Store (Linux V2)

Titan Store is a sleek, modern, and open-source application store for Linux. Built with C++ (Qt6) and WebEngine, it features a stunning glassmorphism UI and a robust, automated application package manager.

![Titan Store Video](Tiwut__Decoding_the_Decentralized_Desktop.mp4)

## Features
- **Glassmorphism Design:** A beautiful, responsive interface built with HTML/CSS inside Qt WebEngine.
- **Native App Packages:** Downloads and extracts `.tar.gz` app packages seamlessly.
- **Start Menu Integration:** Automatically generates `.desktop` shortcuts so installed apps appear instantly in your GNOME/KDE launcher.
- **Decentralized Repositories:** Add any valid JSON manifest URL to load apps from external sources.

---

## 📦 How to Submit / Package Your App

Titan Store doesn't require a central database. We use a **JSON repository** format.
This allows any developer to submit an app simply by adding an entry to our repository list via a GitHub Pull Request!

### 1. Create your App Package (`.tar.gz`)
Instead of distributing single binaries, Titan Store works with folders.
1. Create a folder for your app (e.g., `MyAwesomeApp`).
2. Place your main Linux executable file inside.
3. Place an image named `icon.png` in the folder. This will be used for the Linux Start Menu.
4. Add any other libraries or assets your app needs into the same folder.
5. Select all the files inside the folder, right-click, and compress them into a `.tar.gz` archive.
6. Host this `.tar.gz` file anywhere (e.g., GitHub Releases).

### 2. Update the JSON Manifest
We use JSON instead of YAML because the Qt C++ framework has native, lightning-fast JSON parsing without requiring any heavy third-party dependencies.

To submit your app, make a Pull Request to our `default.json` and add an object to the array:

```json
{
  "id": "my-awesome-app",
  "title": "My Awesome App",
  "version": "1.0",
  "developer": "Your Name",
  "copyright": "© 2026 Your Name",
  "size": "50 MB",
  "download_url": "https://github.com/you/repo/releases/download/v1.0/package.tar.gz",
  "executable": "start_my_app",
  "icon": "https://your-website.com/icon.png",
  "description": "This is the best app ever made for Linux.",
  "categories": ["Gaming", "Network", "Simulation"],
  "permissions": ["Network Access", "Filesystem", "Audio"],
  "screenshots": [
    "https://your-website.com/screenshot1.png",
    "https://your-website.com/screenshot2.png"
  ]
}
```
**Important Fields:**
- `download_url`: Must point directly to your `.tar.gz` file.
- `executable`: The exact filename of the binary inside your archive that should be executed when the user clicks the shortcut.
- `icon`: An absolute URL pointing to your app's icon, which is displayed in the Store UI.
- `categories`: An array of strings classifying your app (e.g., "Gaming", "Development").
- `permissions`: A list of strings informing the user what system rights the app requires.

---

## 🛠️ Build Instructions

**Dependencies:**
- CMake 3.16+
- Qt 6 (Core, Widgets, Network, WebEngineWidgets)
- Make / GCC

**Compilation:**
```bash
mkdir build && cd build
cmake ..
make
./TitanStore
```
