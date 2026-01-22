const { contextBridge, ipcRenderer } = require("electron");

contextBridge.exposeInMainWorld("rapidCraft", {
  runAnalyzer: (args) => ipcRenderer.invoke("run-analyzer", args)
});
