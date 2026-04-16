# This directory contains samples for installation scripts used by **TITAN STORE**.

## How it works

When a user clicks "Install" in the TITAN STORE:
1. The store downloads the script from the `download_url` specified in your `store.json`.
2. The store creates an isolated directory (e.g., `~/.local/share/TITAN_STORE/apps/your.app.id/`).
3. The store sets the **Working Directory** to this folder.
4. The script is executed via `bash`.

### Sample Linux Installation Script

Save this as `install.sh` on your server:

```bash
#!/bin/bash
# TITAN STORE - Automated Installation Script Sample

echo "Starting installation of TITAN Test App..."

# Use relative paths! TITAN STORE already set the working directory for you.
# 1. Create app structure
mkdir -p ./bin
mkdir -p ./game_data

# 2. Add application data
# (In a real scenario: curl -L your_url -o ./bin/binary)
echo "The app was successfully installed by TITAN STORE!" > ./game_data/success.txt

# 3. Finalize
# Ensure your binaries are executable
chmod +x ./bin

# 4. Return Success
# Exit with 0 to notify TITAN STORE that everything went well.
exit 0
```

## Important Notes
- **Relative Paths**: Always use `./` or relative paths. TITAN STORE manages the root directory of your app.
- **Exit Codes**: Always `exit 0` on success. Any other number will trigger an "Installation Failed" error in the UI.
- **Permissions**: TITAN STORE automatically grants execution permissions to the `.sh` file after download, but not as sys root.
