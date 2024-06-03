(Module["preRun"] = Module["preRun"] || []).push(function () {
    addRunDependency('syncfs')

    FS.mkdir('/fastnoise2')
    FS.mount(IDBFS, {}, '/fastnoise2')
    FS.syncfs(true, function (err) {
        if (err) throw err
        removeRunDependency('syncfs')
        console.log("FS Synced")
    })
});