const { app, BrowserWindow, ipcMain } = require("electron");
const path = require("path");
const { spawn } = require("child_process");

function createWindow() {
  const win = new BrowserWindow({
    width: 1200,
    height: 800,
    webPreferences: {
      preload: path.join(__dirname, "preload.js")
    }
  });

  win.loadFile(path.join(__dirname, "index.html"));
}

app.whenReady().then(createWindow);

ipcMain.handle("run-analyzer", async (_event, args) => {
  const analyzerPath = path.join(
    app.getAppPath(),
    "resources",
    "rapid-craft-analyzer.exe"
  );

  return new Promise((resolve) => {
    const proc = spawn(analyzerPath, args, {
      cwd: app.getAppPath(),
      shell: false
    });

    let stdout = "";
    let stderr = "";

    proc.stdout.on("data", (d) => (stdout += d.toString()));
    proc.stderr.on("data", (d) => (stderr += d.toString()));

    proc.on("close", (code) => {
      resolve({
        code,
        stdout,
        stderr
      });
    });
  });
});
